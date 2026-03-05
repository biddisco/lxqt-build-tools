#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
//
#include <QCoreApplication>
#include <QStandardPaths>
//
#include <boost/program_options.hpp>
#include <magic_enum/magic_enum.hpp>
// Grox
#include "config/config.hpp"
#include "currency/currency_pair.hpp"
#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_dataset_view.hpp"
#include "data/ohlc_utils.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/trade_rebalance_funds.hpp"
#include "indicators/trade_sell_sliding_stop.hpp"
#include "io/hdf5_ohlc_manager.hpp"

namespace {
  namespace po = boost::program_options;

  struct options
  {
    std::string algorithm{"trade_sell_sliding_stop"};
    std::string exchange{"bitstamp"};
    std::string base{"XRP"};
    std::string quote{"USD"};
    std::string resolution{"4h"};
    std::uint64_t samples{270};
    int window_size{7};
    std::string mode{"high"};
    double fee_buy{0.2};
    double fee_sell{0.2};
    double upper_gap_percent{1.0};
    double lower_gap_percent{1.0};
    double rsi_multiplier{1.0};
    double gradient_upper{0.0};
    double gradient_lower{0.1};
    bool show_events{false};
    std::string app_data_location;
    std::string hdf_file{"grox.hdf5"};
    bool show_help{false};
  };

  std::string normalize_algorithm(std::string_view algorithm)
  {
    if ((algorithm == "trade_sell_sliding_stop") || (algorithm == "sliding_stop"))
      return "trade_sell_sliding_stop";
    if ((algorithm == "trade_rebalance_funds") || (algorithm == "rebalance_funds") ||
        (algorithm == "rebalance"))
      return "trade_rebalance_funds";
    return std::string(algorithm);
  }

  bool is_supported_algorithm(std::string_view algorithm)
  {
    auto const normalized = normalize_algorithm(algorithm);
    return (normalized == "trade_sell_sliding_stop") || (normalized == "trade_rebalance_funds");
  }

  po::options_description make_core_options(options& out)
  {
    po::options_description desc("Core options");
    desc.add_options()("help,h", "Show this help")("algorithm",
        po::value<std::string>(&out.algorithm)->default_value(out.algorithm),
        "Algorithm key: "
        "trade_sell_sliding_stop|sliding_stop|trade_rebalance_funds|rebalance_funds|rebalance")(
        "exchange", po::value<std::string>(&out.exchange)->default_value(out.exchange),
        "Exchange dataset")("base", po::value<std::string>(&out.base)->default_value(out.base),
        "Base currency code")("quote", po::value<std::string>(&out.quote)->default_value(out.quote),
        "Quote currency code")("resolution",
        po::value<std::string>(&out.resolution)->default_value(out.resolution),
        "Candle resolution")("samples",
        po::value<std::uint64_t>(&out.samples)->default_value(out.samples),
        "Number of samples from start")(
        "show-events", po::bool_switch(&out.show_events), "Print each buy/sell event");
    return desc;
  }

  po::options_description make_storage_options(options& out)
  {
    po::options_description desc("Storage options");
    desc.add_options()("app-data-location",
        po::value<std::string>(&out.app_data_location)->default_value(out.app_data_location),
        "Base data directory (default: Qt GenericDataLocation + /grox)")("hdf-file",
        po::value<std::string>(&out.hdf_file)->default_value(out.hdf_file), "HDF5 filename");
    return desc;
  }

  bool add_algorithm_options(
      std::string_view algorithm, options& out, po::options_description& alg_desc)
  {
    auto const normalized = normalize_algorithm(algorithm);
    if (normalized == "trade_sell_sliding_stop")
    {
      alg_desc.add_options()("window-size",
          po::value<int>(&out.window_size)->default_value(out.window_size),
          "Moving window size")("mode", po::value<std::string>(&out.mode)->default_value(out.mode),
          "ohlc mode: open|close|mid_open_close|high|low|mid_high_low|volume|value")("fee-buy",
          po::value<double>(&out.fee_buy)->default_value(out.fee_buy), "Buy fee percent")(
          "fee-sell", po::value<double>(&out.fee_sell)->default_value(out.fee_sell),
          "Sell fee percent")("upper-gap-percent",
          po::value<double>(&out.upper_gap_percent)->default_value(out.upper_gap_percent),
          "Upper sliding gap in percent")("lower-gap-percent",
          po::value<double>(&out.lower_gap_percent)->default_value(out.lower_gap_percent),
          "Lower sliding gap in percent")("rsi-multiplier",
          po::value<double>(&out.rsi_multiplier)->default_value(out.rsi_multiplier),
          "RSI window multiplier")("gradient-upper",
          po::value<double>(&out.gradient_upper)->default_value(out.gradient_upper),
          "Gradient threshold upper")("gradient-lower",
          po::value<double>(&out.gradient_lower)->default_value(out.gradient_lower),
          "Gradient threshold lower");
      return true;
    }

    if (normalized == "trade_rebalance_funds")
    {
      alg_desc.add_options()("window-size",
          po::value<int>(&out.window_size)->default_value(out.window_size),
          "Moving window size")("mode", po::value<std::string>(&out.mode)->default_value(out.mode),
          "ohlc mode: open|close|mid_open_close|high|low|mid_high_low|volume|value")("fee-buy",
          po::value<double>(&out.fee_buy)->default_value(out.fee_buy), "Buy fee percent")(
          "fee-sell", po::value<double>(&out.fee_sell)->default_value(out.fee_sell),
          "Sell fee percent")("upper-gap-percent",
          po::value<double>(&out.upper_gap_percent)->default_value(out.upper_gap_percent),
          "Upper sliding gap in percent")("lower-gap-percent",
          po::value<double>(&out.lower_gap_percent)->default_value(out.lower_gap_percent),
          "Lower sliding gap in percent")("rsi-multiplier",
          po::value<double>(&out.rsi_multiplier)->default_value(out.rsi_multiplier),
          "RSI window multiplier");
      return true;
    }

    return false;
  }

  void print_help(char const* argv0, std::string_view algorithm)
  {
    options help_opts;
    help_opts.algorithm = normalize_algorithm(algorithm);
    auto core = make_core_options(help_opts);
    auto storage = make_storage_options(help_opts);
    po::options_description algorithm_options("Algorithm options");
    add_algorithm_options(help_opts.algorithm, help_opts, algorithm_options);

    std::cout << "Usage: " << argv0 << " [options]\n"
              << "\n"
              << "Backtest one trading algorithm over local HDF5 OHLC data.\n\n";

    std::cout << core << "\n";
    if (is_supported_algorithm(help_opts.algorithm)) { std::cout << algorithm_options << "\n"; }
    else
    {
      std::cout << "Supported algorithms: trade_sell_sliding_stop, sliding_stop, "
                   "trade_rebalance_funds, rebalance_funds, rebalance\n\n";
    }
    std::cout << storage;
  }

  std::optional<candle_res> parse_resolution(std::string_view res)
  {
    for (auto const& r : ohlc_data_resolutions::available_resolutions())
    {
      if (res == r.name_) return r;
    }
    return std::nullopt;
  }

  bool parse_args(int argc, char** argv, options& out)
  {
    try
    {
      auto core_probe = make_core_options(out);
      po::variables_map vm_probe;
      auto parsed_probe =
          po::command_line_parser(argc, argv).options(core_probe).allow_unregistered().run();
      po::store(parsed_probe, vm_probe);
      po::notify(vm_probe);

      out.algorithm = normalize_algorithm(out.algorithm);

      auto core = make_core_options(out);
      auto storage = make_storage_options(out);
      po::options_description algorithm_options("Algorithm options");
      bool const known_algorithm = add_algorithm_options(out.algorithm, out, algorithm_options);

      po::options_description all("Allowed options");
      all.add(core).add(storage);
      if (known_algorithm) all.add(algorithm_options);

      po::variables_map vm;
      auto parsed = po::command_line_parser(argc, argv).options(all).run();
      po::store(parsed, vm);
      po::notify(vm);

      out.algorithm = normalize_algorithm(out.algorithm);

      if (vm.count("help"))
      {
        out.show_help = true;
        print_help(argv[0], out.algorithm);
        return false;
      }

      if (!known_algorithm)
      {
        std::cerr << "Unknown algorithm: " << out.algorithm << "\n\n";
        print_help(argv[0], out.algorithm);
        return false;
      }

      return true;
    }
    catch (std::exception const& e)
    {
      std::cerr << "Argument parsing failed: " << e.what() << "\n\n";
      print_help(argv[0], out.algorithm);
      return false;
    }
  }

  std::string default_app_data_path()
  {
    return QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)
               .first()
               .toStdString() +
        std::string("/grox");
  }

  void init_data_storage(options const& opts)
  {
    global_settings.appDataLocation =
        opts.app_data_location.empty() ? default_app_data_path() : opts.app_data_location;
    std::filesystem::create_directories(global_settings.appDataLocation);
    global_settings.hdfFileName = opts.hdf_file;
    global_settings.data_manager_ = std::make_shared<hdf5_ohlc_manager>();
    global_settings.data_manager_->init(
        global_settings.appDataLocation, global_settings.hdfFileName);
  }

  template <typename T>
  bool set_named_param(indicators::param_list& params, std::string_view name, T const& value)
  {
    for (auto& p : params)
    {
      if (auto* typed = std::get_if<indicators::param<T>>(&p))
      {
        if (typed->name_.toStdString() == name)
        {
          typed->put(value);
          return true;
        }
      }
    }
    return false;
  }

  template <typename Algorithm>
  int run_algorithm(options const& opts)
  {
    auto res = parse_resolution(opts.resolution);
    if (!res)
    {
      std::cerr << "Unsupported resolution: " << opts.resolution << "\n";
      return 2;
    }

    auto mode = magic_enum::enum_cast<ohlc_modes>(opts.mode);
    if (!mode)
    {
      std::cerr << "Unsupported mode: " << opts.mode << "\n";
      return 2;
    }

    currency_pair cp{{opts.base}, {opts.quote}};
    auto view = std::make_shared<ohlc_dataset_view>(opts.exchange, cp);
    auto* dataset = view->get_dataset(res->res_);
    if (dataset == nullptr)
    {
      std::cerr << "Dataset not found for resolution " << opts.resolution << "\n";
      return 2;
    }

    auto const& data = dataset->data();
    if (data.empty())
    {
      std::cerr << "Dataset is empty\n";
      return 2;
    }

    auto const count =
        std::min<std::uint64_t>(opts.samples, static_cast<std::uint64_t>(data.size()));
    if (count == 0)
    {
      std::cerr << "No samples selected\n";
      return 2;
    }

    Algorithm alg_template;
    alg_template.init_params();
    auto params = alg_template.get_params();

    set_named_param<candle_data>(params, "Samples", {*res, count});
    set_named_param<int>(params, "Window size", opts.window_size);
    set_named_param<ohlc_modes>(params, "mode", *mode);
    set_named_param<double>(params, "Percentage fee Buy", opts.fee_buy);
    set_named_param<double>(params, "Percentage fee Sell", opts.fee_sell);
    set_named_param<double>(params, "Sliding Gap Upper %", opts.upper_gap_percent);
    set_named_param<double>(params, "Sliding Gap Lower %", opts.lower_gap_percent);
    set_named_param<double>(params, "RSI length multiplier", opts.rsi_multiplier);
    set_named_param<double>(params, "Gradient Threshold Upper", opts.gradient_upper);
    set_named_param<double>(params, "Gradient Threshold Lower", opts.gradient_lower);

    alg_template.set_params(params);

    auto alg = std::dynamic_pointer_cast<Algorithm>(alg_template.create(&alg_template, view));
    if (!alg)
    {
      std::cerr << "Failed to create trade algorithm instance for " << opts.algorithm << "\n";
      return 2;
    }

    std::uint64_t buys = 0;
    std::uint64_t sells = 0;
    indicators::buy_sell_point last = {
        indicators::buy_sell_event_type::empty, 0.0, 0.0, 0.0, 0.0, 0.0};

    for (std::uint64_t i = 0; i < count; ++i)
    {
      auto const& ohlc = data[static_cast<int>(i)];
      auto point = (*alg)(ohlc);

      if (point.event_type_ == indicators::buy_sell_event_type::buy)
      {
        ++buys;
        if (opts.show_events)
        {
          std::cout << "buy  t=" << ohlc.time << " p=" << point.event_price_
                    << " tokens=" << point.tokens_ << " value=" << point.value_ << "\n";
        }
      }
      else if (point.event_type_ == indicators::buy_sell_event_type::sell)
      {
        ++sells;
        if (opts.show_events)
        {
          std::cout << "sell t=" << ohlc.time << " p=" << point.event_price_
                    << " tokens=" << point.tokens_ << " value=" << point.value_ << "\n";
        }
      }
      last = point;
    }

    std::cout << std::fixed << std::setprecision(8) << "algorithm=" << opts.algorithm
              << " exchange=" << opts.exchange << " pair=" << opts.base << "/" << opts.quote
              << " resolution=" << opts.resolution << " samples=" << count << " buys=" << buys
              << " sells=" << sells << " final_value=" << last.value_
              << " final_tokens=" << last.tokens_ << " final_cash=" << last.cash_ << "\n";

    return 0;
  }
}    // namespace

int main(int argc, char** argv)
{
  QCoreApplication app(argc, argv);

  options opts;
  if (!parse_args(argc, argv, opts)) { return opts.show_help ? 0 : 1; }

  try
  {
    init_data_storage(opts);

    if ((opts.algorithm == "trade_sell_sliding_stop") || (opts.algorithm == "sliding_stop"))
    {
      return run_algorithm<indicators::trade_sell_sliding_stop>(opts);
    }

    if ((opts.algorithm == "trade_rebalance_funds") || (opts.algorithm == "rebalance_funds") ||
        (opts.algorithm == "rebalance"))
    {
      return run_algorithm<indicators::trade_rebalance_funds>(opts);
    }

    std::cerr << "Unknown algorithm: " << opts.algorithm
              << " (supported: trade_sell_sliding_stop, sliding_stop, "
                 "trade_rebalance_funds, rebalance_funds, rebalance)\n";
    return 2;
  }
  catch (std::exception const& e)
  {
    std::cerr << "Backtester failed: " << e.what() << "\n";
    return 2;
  }
}
