#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include <QApplication>
#include <QTimer>

#include "indicators/python_plugin.hpp"
#include "widgets_control/indicator_json.hpp"
#include "widgets_control/indicator_widget.hpp"

namespace fs = std::filesystem;

// ----------------------------------------------------------------------------
void auto_close_active_modal_dialog(int delay_ms = 1200)
{
  QTimer::singleShot(delay_ms, []() { QApplication::closeAllWindows(); });
}

// ----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  QApplication app(argc, argv);

  auto& registry = indicators::python::python_indicator_registry::instance();
  if (!registry.initialize())
  {
    std::cerr << "Failed to initialize Python indicator registry\n";
    return EXIT_FAILURE;
  }

  fs::path indicator_path = fs::path(__FILE__).parent_path().parent_path().parent_path() /
      "indicators" / "python" / "indicator_example.py";

  if (!fs::exists(indicator_path))
  {
    std::cerr << "Python indicator module not found: " << indicator_path << "\n";
    return EXIT_FAILURE;
  }

  if (!registry.load_module(indicator_path))
  {
    std::cerr << "Failed to load Python indicator module: " << indicator_path << "\n";
    return EXIT_FAILURE;
  }

  auto alg = std::make_shared<indicators::python::python_indicator_wrapper>(
      "Python SMA", "Simple Moving Average", "SimplePythonMovingAverage", &registry);

  alg->init_params();

  auto const& params = alg->get_params();
  std::cout << "Extracted " << params.size() << " Python params for widget form\n";

  auto layout = get_json_layout_indicator(alg);
  auto values = get_json_values_indicator(alg);

  std::cout << "Layout JSON: " << layout.dump(2) << "\n";
  std::cout << "Values JSON: " << values.dump(2) << "\n";

  indicator_widget widget(alg, values);
  auto_close_active_modal_dialog();
  int result = widget.execute_as_dialog();

  std::cout << "indicator_widget dialog result=" << result << "\n";
  return EXIT_SUCCESS;
}
