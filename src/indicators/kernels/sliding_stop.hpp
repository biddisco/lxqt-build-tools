#pragma once

#include <limits>

namespace indicators::kernels {

  //----------------------------------------------------------------------------
  struct sliding_limit
  {
    enum direction
    {
      up,
      down
    };
    //
    double minmax_;
    double limit_percent;
    bool prev_active_;
    bool active_;
    direction dir_;

    //--------------------------------------
    // init/reset the minmax limiter
    sliding_limit(direction updown, double limit_percent)
      : dir_(updown)
      , limit_percent(limit_percent)
      , active_(false)
    {
      minmax_ =
          (dir_ == up) ? std::numeric_limits<double>::min() : std::numeric_limits<double>::max();
    }

    //--------------------------------------
    void restart(double val)
    {
      minmax_ = val;
      active_ = true;
    }

    //--------------------------------------
    void stop() { active_ = false; }

    //--------------------------------------
    bool operator()(double val)
    {
      if (active_)
      {
        double diff;
        if (dir_ == up)
        {
          minmax_ = std::max(minmax_, val);
          diff = (minmax_ - val);
        }
        else
        {
          minmax_ = std::min(minmax_, val);
          diff = (val - minmax_);
        }
        double diff_percent = (100.0 * diff) / minmax_;
        // if percentage diff if less than threshold, all ok
        return (diff_percent < limit_percent);
      }
      return active_;
    }
  };

}    // namespace indicators::kernels
