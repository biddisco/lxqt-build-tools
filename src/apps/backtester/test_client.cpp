// Simple ZeroMQ test client for backtester
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <zmq.hpp>

namespace {
  std::string normalize_algorithm(std::string_view algorithm)
  {
    if ((algorithm == "trade_sell_sliding_stop") || (algorithm == "sliding_stop"))
      return "trade_sell_sliding_stop";
    if ((algorithm == "trade_rebalance_funds") || (algorithm == "rebalance_funds") ||
        (algorithm == "rebalance"))
      return "trade_rebalance_funds";
    return std::string(algorithm);
  }

  std::set<std::string> get_valid_params_for_algorithm(std::string_view algorithm)
  {
    auto const normalized = normalize_algorithm(algorithm);

    // Common parameters
    std::set<std::string> params = {"samples", "window_size", "fee_buy", "fee_sell", "resolution",
        "currency", "quote_currency", "ohlc_mode", "upper_gap_percent", "lower_gap_percent",
        "rsi_multiplier"};

    if (normalized == "trade_sell_sliding_stop")
    {
      // Add gradient parameters for sliding stop
      params.insert("gradient_upper");
      params.insert("gradient_lower");
      return params;
    }

    if (normalized == "trade_rebalance_funds")
    {
      // No additional parameters beyond common ones
      return params;
    }

    return params;    // Default to all if unknown
  }
}    // namespace

int main(int argc, char* argv[])
{
  std::string endpoint;
  std::string algorithm = "trade_sell_sliding_stop";
  bool shutdown = false;
  int index = 0;
  int param_start = 1;

  // Parse arguments
  for (int i = 1; i < argc; ++i)
  {
    std::string arg = argv[i];

    if (arg == "--help" || arg == "-h")
    {
      std::cout << "Usage: backtester_test_client [OPTIONS] [key=value ...]\n"
                << "Options:\n"
                << "  --algorithm ALG      Algorithm: trade_sell_sliding_stop, "
                   "trade_rebalance_funds\n"
                << "  --endpoint ENDPOINT  ZeroMQ endpoint (default: ipc:///tmp/backtester.sock)\n"
                << "  --index IDX          Index for parallel instances, offsets socket/port "
                   "(default: 0)\n"
                << "  --shutdown           Tell server to shutdown and exit\n"
                << "  --help, -h          Show this help\n\n"
                << "Examples:\n"
                << "  backtester_test_client\n"
                << "  backtester_test_client --algorithm trade_rebalance_funds window_size=15\n"
                << "  backtester_test_client --index 1 samples=200\n"
                << "  backtester_test_client --endpoint tcp://localhost:5556 samples=200\n"
                << "  backtester_test_client --shutdown\n";
      return 0;
    }

    if (arg == "--shutdown")
    {
      shutdown = true;
      continue;
    }

    if (arg == "--algorithm" && i + 1 < argc)
    {
      algorithm = argv[++i];
      continue;
    }

    if (arg == "--index" && i + 1 < argc)
    {
      index = std::stoi(argv[++i]);
      continue;
    }

    if (arg == "--endpoint" && i + 1 < argc)
    {
      endpoint = argv[++i];
      continue;
    }

    if (arg[0] == '-')
    {
      std::cerr << "Unknown option: " << arg << "\n";
      return 1;
    }

    // Rest are parameters
    param_start = i;
    break;
  }

  // Helper function to compute endpoint based on index
  auto get_endpoint_for_index = [](std::string_view base, int idx) -> std::string {
    if (base.empty())
    {
      if (idx == 0)
        return "ipc:///tmp/backtester.sock";
      else
        return "ipc:///tmp/backtester-" + std::to_string(idx) + ".sock";
    }
    if (base.find("ipc://") == 0)
    {
      if (idx == 0) return std::string(base);
      auto sock_pos = base.rfind(".sock");
      if (sock_pos != std::string::npos)
        return std::string(base.substr(0, sock_pos)) + "-" + std::to_string(idx) + ".sock";
    }
    if (base.find("tcp://") == 0)
    {
      if (idx == 0) return std::string(base);
      auto colon_pos = base.rfind(':');
      if (colon_pos != std::string::npos)
      {
        try
        {
          auto port_str = base.substr(colon_pos + 1);
          int port = std::stoi(std::string(port_str));
          return std::string(base.substr(0, colon_pos)) + ":" + std::to_string(port + idx);
        }
        catch (...)
        {
        }
      }
    }
    return std::string(base);
  };

  endpoint = get_endpoint_for_index(endpoint, index);

  if (shutdown)
  {
    std::cout << "Connecting to backtester at " << endpoint << "...\n";
    zmq::context_t context(1);
    zmq::socket_t socket(context, zmq::socket_type::req);
    socket.connect(endpoint);

    nlohmann::json shutdown_msg{{"shutdown", true}};
    std::string json_str = shutdown_msg.dump();
    std::cout << "Sending shutdown signal...\n";

    zmq::message_t request(json_str.size());
    memcpy(request.data(), json_str.c_str(), json_str.size());
    socket.send(request, zmq::send_flags::none);

    zmq::message_t reply;
    auto result = socket.recv(reply, zmq::recv_flags::none);

    if (result)
    {
      std::string response(static_cast<char*>(reply.data()), reply.size());
      std::cout << "Server response: " << response << "\n";
    }

    return 0;
  }

  std::cout << "Algorithm: " << normalize_algorithm(algorithm) << "\n"
            << "Connecting to backtester at " << endpoint << "...\n";

  zmq::context_t context(1);
  zmq::socket_t socket(context, zmq::socket_type::req);
  socket.connect(endpoint);

  // Create test parameters with algorithm-aware defaults
  nlohmann::json params = {{"samples", 100}, {"window_size", 10}, {"fee_buy", 0.15},
      {"fee_sell", 0.15}, {"resolution", "4h"}, {"currency", "XRP"}, {"quote_currency", "USD"},
      {"ohlc_mode", "mid"}, {"upper_gap_percent", 1.0}, {"lower_gap_percent", 1.0},
      {"rsi_multiplier", 1.0}};

  // Add algorithm-specific defaults
  auto const normalized_alg = normalize_algorithm(algorithm);
  if (normalized_alg == "trade_sell_sliding_stop")
  {
    params["gradient_upper"] = 0.0;
    params["gradient_lower"] = 0.1;
  }

  // Get valid parameters for this algorithm
  auto const valid_params = get_valid_params_for_algorithm(algorithm);

  // Parse command-line parameter overrides
  for (int i = param_start; i < argc; ++i)
  {
    std::string arg = argv[i];
    auto eq = arg.find('=');
    if (eq != std::string::npos)
    {
      std::string key = arg.substr(0, eq);
      std::string value = arg.substr(eq + 1);

      if (valid_params.find(key) == valid_params.end())
      {
        std::cerr << "Error: '" << key << "' is not a valid parameter for algorithm '"
                  << normalized_alg << "'\n";
        std::cerr << "Valid parameters: ";
        bool first = true;
        for (auto const& p : valid_params)
        {
          if (!first) std::cerr << ", ";
          std::cerr << p;
          first = false;
        }
        std::cerr << "\n";
        return 1;
      }

      // Try to parse as number
      try
      {
        if (value.find('.') != std::string::npos) { params[key] = std::stod(value); }
        else { params[key] = std::stoi(value); }
      }
      catch (...)
      {
        // Keep as string
        params[key] = value;
      }
    }
  }

  // Filter parameters to only those valid for this algorithm
  nlohmann::json filtered_params;
  for (auto const& p : valid_params)
  {
    if (params.contains(p)) { filtered_params[p] = params[p]; }
  }
  filtered_params["algorithm"] = normalized_alg;

  std::string json_str = filtered_params.dump();
  std::cout << "Sending parameters:\n" << filtered_params.dump(2) << "\n\n";

  // Send request
  zmq::message_t request(json_str.size());
  memcpy(request.data(), json_str.c_str(), json_str.size());
  socket.send(request, zmq::send_flags::none);

  // Receive response
  zmq::message_t reply;
  auto result = socket.recv(reply, zmq::recv_flags::none);

  if (result)
  {
    std::string response(static_cast<char*>(reply.data()), reply.size());
    std::cout << "Response:\n" << response << "\n";
  }
  else
  {
    std::cerr << "Error: No response received\n";
    return 1;
  }

  return 0;
}
