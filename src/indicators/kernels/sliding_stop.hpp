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
    double limit_diff_;
    bool prev_active_;
    bool active_;
    direction dir_;

    //--------------------------------------
    // init/reset the minmax limiter
    sliding_limit(direction updown, double limit)
      : dir_(updown)
      , limit_diff_(limit)
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
        // if we drop outside of the allowed range, return fail
        return (diff < limit_diff_);
      }
      return active_;
    }
  };

}    // namespace indicators::kernels
