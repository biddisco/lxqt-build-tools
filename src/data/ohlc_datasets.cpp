// STL
#include <vector>
// Qt
#include <QVector>
// Qwt
#include <QwtOHLCSample>
// Grox
#include "src/debug.hpp"
#include "src/plot/ohlc_chart_data.hpp"
#include "src/data/ohlc_datasets.hpp"

// ----------------------------------------------------------------------------
ohlc_datasets::ohlc_datasets(double res)
    : resolution_(res)
{
    // we do not destroy these in the destructor because they are given to the
    // plot curve object which deletes them when it is destroyed
    ohlc_samples_ = new ohlc_chart_data();
    ohlc_curve_   = new ohlc_chart_curve(ohlc_samples_);
    live_samples_ = new ohlc_chart_data();
    live_curve_   = new ohlc_chart_curve(live_samples_);
}

// ----------------------------------------------------------------------------
ohlc_datasets::~ohlc_datasets()
{
    // curve is deleted by plot,
    // samples are deleted by curve
    //    delete ohlc_curve_;
    //    delete ohlc_samples_;
    //    delete live_curve_;
    //    delete live_samples_;
}

// ----------------------------------------------------------------------------
uint64_t ohlc_datasets::merge_data(const QVector<QwtOHLCSample>& new_ohlc_samples_,
    const std::vector<double>& new_ohlc_volumes_)
{
    uint64_t update = 0;
    if (ohlc_samples_->data().size() == 0)
    {
        ohlc_samples_->data() = new_ohlc_samples_;
        ohlc_volumes_ = new_ohlc_volumes_;
    }
    else if (!new_ohlc_samples_.empty())
    {
        auto last_existing = ohlc_samples_->data().back().time;
        auto first_new = new_ohlc_samples_.front().time;

        DEBUG_ONLY("existing " << static_cast<uint64_t>(last_existing) << " new "
                  << static_cast<uint64_t>(first_new));
        // 1 minute candle OHLC data is stored in msecs
        if (first_new - last_existing == ohlc_chart_data::minute)
        {
            DEBUG_ONLY("merging data");
            ohlc_samples_->data().append(new_ohlc_samples_);
            ohlc_volumes_.insert(
                ohlc_volumes_.end(), new_ohlc_volumes_.begin(), new_ohlc_volumes_.end());
            if (size_t(ohlc_samples_->data().size()) != ohlc_volumes_.size())
            {
                throw std::runtime_error("Data merge problem");
            }
            update = new_ohlc_samples_.size();
        }
        else
        {
            throw std::runtime_error("Data OHLC time mismatch in merge");
        }
    }
    return update;
}

// ----------------------------------------------------------------------------
void ohlc_datasets::validate_ohlc(QVector<QwtOHLCSample> const &samples, double res)
{
    using cit = QVector<QwtOHLCSample>::const_iterator;
    cit li = samples.begin();
    bool valid = true;
    uint64_t index = 0;
    for (cit i = samples.begin() + 1; i != samples.end(); ++i)
    {
        uint64_t t1 = static_cast<uint64_t>(li->time);
        uint64_t t2 = static_cast<uint64_t>(i->time);
        if (t2 - t1 != res)
        {
            std::cerr << "Validation error at index " << index << " " << t1 << " and "
                      << t2 << "dataset truncated " << std::endl;
            valid = false;
            break;
        }
        li = i;
        index++;
    }
}

// ----------------------------------------------------------------------------
void update_QwtOHLCSample(QwtOHLCSample &ohlc, QwtOHLCSample const &other)
{
    ohlc.low  = std::min(ohlc.low, other.low);
    ohlc.high = std::max(ohlc.high, other.high);
    ohlc.close = other.close;
}

// ----------------------------------------------------------------------------
ohlc_datasets *ohlc_datasets::resample(double res)
{
    if (ohlc_samples_->data().empty()) return nullptr;
    //
    ohlc_datasets *result = new ohlc_datasets(res);
    //
    QwtOHLCSample current_ohlc;
    current_ohlc.time = 0;
    for (QVector<QwtOHLCSample>::const_iterator it=ohlc_samples_->data().begin(); it!=ohlc_samples_->data().end(); ++it) {
        double quantized_time = res*static_cast<uint64_t>(it->time/res);
        if (quantized_time != current_ohlc.time) {
            current_ohlc = *it;
            current_ohlc.time = quantized_time;
            result->ohlc_samples_->append(current_ohlc);
        }
        else {
            update_QwtOHLCSample(current_ohlc, *it);
            result->ohlc_samples_->data().back() = current_ohlc;
        }
    }
    return result;
}
