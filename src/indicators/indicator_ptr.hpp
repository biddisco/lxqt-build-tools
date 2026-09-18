#pragma once

#include <assert.h>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "debug/print.hpp"
#include "indicators/algorithm_base.hpp"
#include "indicators/indicator_base.hpp"

class indicator_plot;
class ohlc_price_plot;
class timebased_data_curve;
class QwtPlotCurve;
class QwtPlot;

namespace indicators {

  struct indicator_ptr
  {
    // ----------------------------------------------------------------------------
    indicators::indicator_base* indicator() const
    {
      return dynamic_cast<indicator_base*>(algorithm_.get());
    }

    // ----------------------------------------------------------------------------
    indicators::algorithm_base* ptr() const { return algorithm_.get(); }

    // ----------------------------------------------------------------------------
    indicator_ptr(shared_algorithm ap, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc_)
    {
      // if this pointer is an indicator_base type
      indicator_base* ip = dynamic_cast<indicator_base*>(ap.get());
      if (ip)
      {
        algorithm_ = ip->create(ip, hdf5_ohlc_);
        std::uint64_t N = indicator()->get_input(0).samples_;
        indicator()->execute_from(N);
      }
      else { algorithm_ = ap->create(ap.get()); }
    }

    // ----------------------------------------------------------------------------
    ~indicator_ptr()
    {
      // Explicitly unsubscribe from pub/sub callbacks BEFORE destroying algorithm_.
      // This prevents "empty/cleared std::function" errors if the dataset's pub/sub
      // is destroyed before indicator_base::~indicator_base() runs. After we unsubscribe,
      // clear callbacks_registered_ so indicator_base::~indicator_base() does not try
      // to unsubscribe again.
      if (auto* ip = indicator())
      {
        if (ip->callbacks_registered())
        {
          try
          {
            for (auto d : ip->get_inputs())
            {
              std::string id = ip->subscription_name();
              d.dataset_->new_data_subscribers_.unsubscribe(id);
            }
          }
          catch (std::exception const& e)
          {
            // Dataset may already be destroyed (dangling raw pointer) - best effort cleanup
            std::cout << "ERROR: Exception during indicator_ptr destruction unsubscribe, id: "
                      << ip->subscription_name() << " what: " << e.what() << std::endl;
          }
          ip->set_callbacks_registered(false);
        }
      }
    }

    // ----------------------------------------------------------------------------
    shared_algorithm algorithm_;
    indicator_plot* plot{nullptr};
    std::vector<QwtPlotCurve*> curves;
    /// The plot each curve belongs to. Parallel to curves; needed because
    /// some indicators create a separate plot per output (e.g. price overlay
    /// outputs), but indicator_ptr::plot only retains the last one. Without
    /// this, deletion passes the wrong plot to remove_indicator_plot and
    /// double-frees it.
    std::vector<QwtPlot*> curve_plots;
    bool visibility_{true};
  };
}    // namespace indicators
