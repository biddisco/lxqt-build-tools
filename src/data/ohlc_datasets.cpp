// STL
#include <vector>
// Qt
#include <QVector>
#include <QDateTime>
#include <QLocale>
// Grox
#include "src/debug.hpp"
#include "src/plot/ohlc_chart_data.hpp"
#include "src/data/ohlc_datasets.hpp"

// ----------------------------------------------------------------------------
ohlc_datasets::ohlc_datasets(double res)
{
    // we do not destroy these in the destructor because they are given to the
    // plot curve object which deletes them when it is destroyed
    ohlc_samples_ = new ohlc_chart_data(res);
    ohlc_curve_   = new ohlc_chart_curve(ohlc_samples_);
    live_samples_ = new ohlc_chart_data(res);
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
uint64_t ohlc_datasets::merge_data(const QVector<QwtOHLCSample>& new_ohlc_samples_)
{
    uint64_t update = 0;
    if (ohlc_samples_->data().size() == 0)
    {
        ohlc_samples_->data() = new_ohlc_samples_;
        return ohlc_samples_->size();
    }
    else if (!new_ohlc_samples_.empty())
    {
        auto last_existing = ohlc_samples_->data().back().time;
        auto first_new = new_ohlc_samples_.front().time;

        DEBUG_ONLY("existing " << static_cast<uint64_t>(last_existing) << " new "
                  << static_cast<uint64_t>(first_new));
        // 1 minute candle OHLC data is stored in msecs
        if (first_new - last_existing != ohlc_chart_data::minute)
        {
            // How many missing samples are there?
            int N = (first_new - last_existing) / ohlc_chart_data::minute;
            if (N<=0) {
                throw std::runtime_error("Data OHLC time mismatch in merge");
            }
            auto prev = ohlc_samples_->data().back();
            // got from 1 to N to add N-1 samples with time offsets from 1
            for (int i=1; i<N; ++i) {
                QwtOHLCSample dummy(last_existing + i*ohlc_chart_data::minute,
                                    prev.close, prev.close, prev.close, prev.close, 0.0);
                ohlc_samples_->data().append(dummy);
            }
            update += (N-1);
        }
        // try again with dummy data inserted into gap
        last_existing = ohlc_samples_->data().back().time;
        (void)last_existing; //warning about unused value store
        DEBUG_ONLY("existing " << static_cast<uint64_t>(last_existing) << " new "
                  << static_cast<uint64_t>(first_new));

        DEBUG_ONLY("merging data");
        ohlc_samples_->data().append(new_ohlc_samples_);
        update += new_ohlc_samples_.size();
    }
    return update;
}

// ----------------------------------------------------------------------------
void ohlc_datasets::validate_ohlc(QVector<QwtOHLCSample> const &samples, double res)
{
    if (samples.empty()) return;
    //
    double init_time = samples.begin()->time;

    for (int64_t index=0; index<samples.size(); ++ index)
    {
        const QwtOHLCSample &s1 = samples.at(index);
        //
        double expected_time = init_time + (res*index);
        if (expected_time != s1.time)
        {
            std::cerr << "Validation error at index " << index << " " << expected_time << " and "
                      << s1.time << "dataset truncated " << std::endl;
            throw std::runtime_error("OHLC data integrity failure");
        }
    }
    DEBUG_ALWAYS("OHLC Data samples validated " << samples.size());
}

// ----------------------------------------------------------------------------
void update_QwtOHLCSample(QwtOHLCSample &ohlc, QwtOHLCSample const &other)
{
    ohlc.low    = std::min(ohlc.low, other.low);
    ohlc.high   = std::max(ohlc.high, other.high);
    ohlc.close  = other.close;
    ohlc.volume = ohlc.volume + other.volume;
}

// ----------------------------------------------------------------------------
uint64_t sample_index(double init, double time, double res)
{
    uint64_t i = static_cast<uint64_t>((time-init)/res);
    return std::max(uint64_t(0), i);
}

// ----------------------------------------------------------------------------
// resample from res2 to res1
ohlc_datasets *ohlc_datasets::resample(double res1, double res2)
{
    ohlc_datasets *result = new ohlc_datasets(res1);
    result->resample_update(res1, this, res2);
    return result;
}

// ----------------------------------------------------------------------------
// res1 is reolution of this dataset, res2 is (higher) resolution of other
ohlc_datasets *ohlc_datasets::resample_update(double res1, ohlc_datasets *other, double res2)
{
    if (other->ohlc_samples_->data().empty()) return this;

    // Get the final point of this dataset if present
    double T;
    if (!ohlc_samples_->data().empty()) {
        T = ohlc_samples_->data().back().time;
    }
    // otherwise, just use the first point of the other dataset
    else {
        T = other->ohlc_samples_->data().front().time;
        // insert a dummy sample we will overwrite
        ohlc_samples_->data().append(QwtOHLCSample());
    }

    // What index in the high res data maps to our time T
    auto init2 = other->ohlc_samples_->data().front().time;
    uint64_t that_sample = sample_index(init2, T, res2);

    // we will start a fresh candle from this time T
    QwtOHLCSample current_ohlc = other->ohlc_samples_->data()[that_sample];
    current_ohlc.time = res1*static_cast<uint64_t>(init2/res1);

    // iterate over all higher res samples for T onwards
    for (QVector<QwtOHLCSample>::const_iterator
         it=other->ohlc_samples_->data().begin() + that_sample;
         it<other->ohlc_samples_->data().end(); ++it)
    {
        double quantized_time = res1*static_cast<uint64_t>(it->time/res1);
        // if the time is not the same as our current candle, start a new one
        if (quantized_time != current_ohlc.time) {
            current_ohlc = *it;
            current_ohlc.time = quantized_time;
            ohlc_samples_->append(current_ohlc);
        }
        // overwrite the current candle with updated numbers
        else {
            update_QwtOHLCSample(current_ohlc, *it);
            ohlc_samples_->data().back() = current_ohlc;
        }
    }
    return this;
}
