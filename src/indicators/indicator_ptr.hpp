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
        indicator()->execute(N);
      }
      else { algorithm_ = ap->create(ap.get()); }
    }

    // ----------------------------------------------------------------------------
    ~indicator_ptr() { std::cout << "~indicator_ptr " << algorithm_->get_name() << std::endl; }

    // ----------------------------------------------------------------------------
    shared_algorithm algorithm_;
    indicator_plot* plot{nullptr};
    std::vector<QwtPlotCurve*> curves;
    bool visibility_{true};
  };
}    // namespace indicators
