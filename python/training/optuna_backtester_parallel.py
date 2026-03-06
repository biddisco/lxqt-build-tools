#!/usr/bin/env python3

import argparse
import json
import multiprocessing as mp
import os
import signal
import subprocess
import sys
import time
from pathlib import Path
from typing import Any


def compute_endpoint(index: int, endpoint: str | None) -> str:
    if not endpoint:
        return (
            "ipc:///tmp/backtester.sock"
            if index == 0
            else f"ipc:///tmp/backtester-{index}.sock"
        )

    if endpoint.startswith("ipc://"):
        if index == 0:
            return endpoint
        if endpoint.endswith(".sock"):
            return endpoint[:-5] + f"-{index}.sock"
        return endpoint

    if endpoint.startswith("tcp://"):
        if index == 0:
            return endpoint
        host, _, port = endpoint.rpartition(":")
        try:
            return f"{host}:{int(port) + index}"
        except ValueError:
            return endpoint

    return endpoint


def parse_result_line(line: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for token in line.strip().split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        result[key] = value
    return result


def wait_until_ready(socket, timeout_s: float = 8.0) -> None:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            socket.send_json({"samples": 5})
            _ = socket.recv_string()
            return
        except Exception:
            time.sleep(0.2)
    raise RuntimeError("Backtester server did not respond in time")


def launch_server(
    backtester_bin: Path, algorithm: str, index: int, endpoint_flag: str | None
) -> subprocess.Popen:
    cmd = [
        str(backtester_bin),
        "--zmq-mode",
        "--algorithm",
        algorithm,
        "--index",
        str(index),
    ]
    if endpoint_flag:
        cmd += ["--endpoint", endpoint_flag]

    return subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        preexec_fn=os.setsid,
    )


def graceful_shutdown(socket, proc: subprocess.Popen | None) -> None:
    if socket:
        try:
            socket.send_json({"shutdown": True})
            _ = socket.recv_string()
        except Exception:
            pass
        socket.close(0)

    if proc:
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(proc.pid, signal.SIGTERM)
            except Exception:
                proc.terminate()


def objective_factory(algorithm: str, metric: str, endpoint: str):
    def objective(trial):
        try:
            import zmq
        except ImportError:
            raise RuntimeError("pyzmq is required: pip install pyzmq")

        context = zmq.Context()
        client = context.socket(zmq.REQ)
        client.connect(endpoint)

        try:
            params = {
                "samples": 270,  # Fixed: varying this confounds the optimization
                "window_size": trial.suggest_int("window_size", 5, 30),
                "fee_buy": trial.suggest_float("fee_buy", 0.0, 0.3),
                "fee_sell": trial.suggest_float("fee_sell", 0.0, 0.3),
                "upper_gap_percent": trial.suggest_float("upper_gap_percent", 0.1, 3.0),
                "lower_gap_percent": trial.suggest_float("lower_gap_percent", 0.1, 3.0),
                "rsi_multiplier": trial.suggest_int("rsi_multiplier", 1, 5),
                "resolution": "4h",
                "currency": "XRP",
                "quote_currency": "USD",
                "ohlc_mode": "mid",
            }

            if algorithm in ("trade_sell_sliding_stop", "sliding_stop"):
                params["gradient_upper"] = trial.suggest_float(
                    "gradient_upper", -0.5, 0.5
                )
                params["gradient_lower"] = trial.suggest_float(
                    "gradient_lower", -0.5, 0.5
                )

            client.send_json(params)
            response = client.recv_string()

            if response.startswith("ERROR:"):
                raise RuntimeError(response)

            parsed = parse_result_line(response)
            if metric not in parsed:
                raise RuntimeError(
                    f"Metric '{metric}' not found in response: {response}"
                )

            try:
                return float(parsed[metric])
            except ValueError as exc:
                raise RuntimeError(
                    f"Metric '{metric}' is not numeric: {parsed[metric]}"
                ) from exc
        finally:
            client.close(0)
            context.term()

    return objective


def run_study_worker(worker_id: int, args_dict: dict[str, Any]) -> dict[str, Any]:
    """Run a single Optuna study in a worker process."""
    try:
        import optuna
    except ImportError:
        return {
            "error": "optuna not installed; run: pip install optuna",
            "worker_id": worker_id,
        }

    backtester_bin = Path(args_dict["backtester_bin"])
    algorithm = args_dict["algorithm"]
    endpoint_flag = args_dict["endpoint"]
    trials = args_dict["trials_per_worker"]
    metric = args_dict["metric"]
    direction = args_dict["direction"]

    effective_endpoint = compute_endpoint(worker_id, endpoint_flag)

    # Clean up stale IPC socket
    if effective_endpoint.startswith("ipc://"):
        socket_path = effective_endpoint[len("ipc://") :]
        try:
            if os.path.exists(socket_path):
                os.unlink(socket_path)
        except OSError:
            pass

    # Start server for this worker
    server_proc = launch_server(backtester_bin, algorithm, worker_id, endpoint_flag)
    print(f"[Worker {worker_id}] Started backtester PID={server_proc.pid}")

    try:
        import zmq

        context = zmq.Context()
        client = context.socket(zmq.REQ)
        client.connect(effective_endpoint)

        try:
            wait_until_ready(client)
        except RuntimeError as e:
            return {"error": str(e), "worker_id": worker_id}

        # Run study
        study = optuna.create_study(direction=direction)
        study.optimize(
            objective_factory(algorithm, metric, effective_endpoint), n_trials=trials
        )

        result = {
            "worker_id": worker_id,
            "best_value": study.best_value,
            "best_params": study.best_params,
            "n_trials": len(study.trials),
        }

        graceful_shutdown(client, server_proc)
        context.term()
        return result
    except Exception as e:
        graceful_shutdown(None, server_proc)
        return {"error": str(e), "worker_id": worker_id}


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Parallel Optuna optimization for ZeroMQ backtester"
    )
    parser.add_argument("--algorithm", default="trade_sell_sliding_stop")
    parser.add_argument(
        "--workers", type=int, default=2, help="Number of parallel workers"
    )
    parser.add_argument(
        "--endpoint",
        default=None,
        help="Base endpoint; indices offset automatically",
    )
    parser.add_argument(
        "--total-trials",
        type=int,
        default=30,
        help="Total trials (split across workers)",
    )
    parser.add_argument(
        "--metric",
        default="final_value",
        choices=["final_value", "final_tokens", "final_cash", "buys", "sells"],
    )
    parser.add_argument(
        "--direction",
        default="maximize",
        choices=["maximize", "minimize"],
    )
    parser.add_argument(
        "--backtester-bin",
        default=None,
        help="Path to backtester executable",
    )
    args = parser.parse_args()

    try:
        import optuna
    except ImportError:
        print(
            "Missing dependency: optuna. Install with: pip install optuna",
            file=sys.stderr,
        )
        return 2

    try:
        import zmq
    except ImportError:
        print(
            "Missing dependency: pyzmq. Install with: pip install pyzmq",
            file=sys.stderr,
        )
        return 2

    repo_root = Path(__file__).resolve().parents[2]
    default_backtester = repo_root.parent / "build" / "grox" / "bin" / "backtester"
    backtester_bin = (
        Path(args.backtester_bin) if args.backtester_bin else default_backtester
    )

    if not backtester_bin.exists():
        print(f"Backtester executable not found: {backtester_bin}", file=sys.stderr)
        return 2

    trials_per_worker = args.total_trials // args.workers

    args_dict = {
        "backtester_bin": str(backtester_bin),
        "algorithm": args.algorithm,
        "endpoint": args.endpoint,
        "trials_per_worker": trials_per_worker,
        "metric": args.metric,
        "direction": args.direction,
    }

    print(f"Starting {args.workers} parallel studies ({trials_per_worker} trials each)")
    print(f"Total expected trials: {args.workers * trials_per_worker}")

    with mp.Pool(processes=args.workers) as pool:
        results = pool.starmap(
            run_study_worker,
            [(i, args_dict) for i in range(args.workers)],
        )

    print("\n" + "=" * 80)
    print("PARALLEL OPTIMIZATION RESULTS")
    print("=" * 80)

    all_best_values = []
    for result in results:
        if "error" in result:
            print(f"Worker {result['worker_id']}: ERROR - {result['error']}")
        else:
            print(f"\nWorker {result['worker_id']}:")
            print(f"  Best {args.metric}: {result['best_value']}")
            print(f"  Best params: {json.dumps(result['best_params'], indent=4)}")
            print(f"  Trials completed: {result['n_trials']}")
            all_best_values.append(result["best_value"])

    print("\n" + "=" * 80)
    if all_best_values:
        if args.direction == "maximize":
            overall_best = max(all_best_values)
            print(f"Overall best {args.metric}: {overall_best}")
        else:
            overall_best = min(all_best_values)
            print(f"Overall best {args.metric}: {overall_best}")
    print("=" * 80)

    return 0


if __name__ == "__main__":
    mp.set_start_method("spawn", force=True)
    raise SystemExit(main())
