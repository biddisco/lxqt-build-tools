#!/usr/bin/env python3

import argparse
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path


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


def graceful_shutdown(socket, proc: subprocess.Popen) -> None:
    try:
        socket.send_json({"shutdown": True})
        _ = socket.recv_string()
    except Exception:
        pass

    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGTERM)
        except Exception:
            proc.terminate()


def objective_factory(algorithm: str, metric: str, client):
    def objective(trial):
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
            params["gradient_upper"] = trial.suggest_float("gradient_upper", -0.5, 0.5)
            params["gradient_lower"] = trial.suggest_float("gradient_lower", -0.5, 0.5)

        client.send_json(params)
        response = client.recv_string()

        if response.startswith("ERROR:"):
            raise RuntimeError(response)

        parsed = parse_result_line(response)
        if metric not in parsed:
            raise RuntimeError(f"Metric '{metric}' not found in response: {response}")

        try:
            return float(parsed[metric])
        except ValueError as exc:
            raise RuntimeError(
                f"Metric '{metric}' is not numeric: {parsed[metric]}"
            ) from exc

    return objective


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Optuna smoke test for ZeroMQ backtester"
    )
    parser.add_argument("--algorithm", default="trade_sell_sliding_stop")
    parser.add_argument("--index", type=int, default=0)
    parser.add_argument(
        "--endpoint", default=None, help="Base endpoint; defaults to IPC socket"
    )
    parser.add_argument("--trials", type=int, default=15)
    parser.add_argument(
        "--metric",
        default="final_value",
        choices=["final_value", "final_tokens", "final_cash", "buys", "sells"],
    )
    parser.add_argument(
        "--direction", default="maximize", choices=["maximize", "minimize"]
    )
    parser.add_argument(
        "--backtester-bin", default=None, help="Path to backtester executable"
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

    effective_endpoint = compute_endpoint(args.index, args.endpoint)

    if effective_endpoint.startswith("ipc://"):
        socket_path = effective_endpoint[len("ipc://") :]
        try:
            if os.path.exists(socket_path):
                os.unlink(socket_path)
        except OSError:
            pass

    server_proc = launch_server(
        backtester_bin, args.algorithm, args.index, args.endpoint
    )
    print(f"Started backtester PID={server_proc.pid} endpoint={effective_endpoint}")

    context = zmq.Context()
    client = context.socket(zmq.REQ)
    client.connect(effective_endpoint)

    try:
        wait_until_ready(client)

        study = optuna.create_study(direction=args.direction)
        study.optimize(
            objective_factory(args.algorithm, args.metric, client), n_trials=args.trials
        )

        print("\nOptimization complete")
        print(f"Best value ({args.metric}): {study.best_value}")
        print(f"Best params: {json.dumps(study.best_params, indent=2)}")
        return 0
    finally:
        graceful_shutdown(client, server_proc)
        client.close(0)
        context.term()


if __name__ == "__main__":
    raise SystemExit(main())
