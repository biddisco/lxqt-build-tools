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
class timebased_data_curve;
class QwtPlotCurve;

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
      // is destroyed before indicator_base::~indicator_base() runs (which also unsubscribes,
      // but as a safety net we do it here too). If unsubscribe fails due to dataset destruction,
      // we catch and ignore to allow graceful shutdown even with destroyed data sources.
      if (auto* ip = indicator())
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
          // Dataset may already be destroyed during widget shutdown, that's okay
        }
      }
    }

    // ----------------------------------------------------------------------------
    shared_algorithm algorithm_;
    indicator_plot* plot{nullptr};
    std::vector<QwtPlotCurve*> curves;
    bool visibility_{true};
  };
}    // namespace indicators
