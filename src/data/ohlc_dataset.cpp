// STL
#include <vector>
// Qt
#include <QVector>
// Qwt
#include <QwtOHLCSample>
// Grox
#include "src/debug.hpp"
#include "src/plot/ohlc_chart_data.hpp"
#include "src/data/ohlc_dataset.hpp"

// ----------------------------------------------------------------------------
ohlc_dataset::ohlc_dataset(double res)
    : resolution_(res)
{
    // we do not destroy these in the destructor because they are given to the
    // plot curve object which deletes them when it is destroyed
    ohlc_samples = new ohlc_chart_data();
    live_samples = new ohlc_chart_data();
}

// ----------------------------------------------------------------------------
uint64_t ohlc_dataset::merge_data(const QVector<QwtOHLCSample>& new_ohlc_samples,
    const std::vector<double>& new_ohlc_volumes)
{
    uint64_t update = 0;
    if (ohlc_samples->data().size() == 0)
    {
        ohlc_samples->data() = new_ohlc_samples;
        ohlc_volumes = new_ohlc_volumes;
    }
    else if (!new_ohlc_samples.empty())
    {
        auto last_existing = ohlc_samples->data().back().time;
        auto first_new = new_ohlc_samples.front().time;

        DEBUG_ONLY("existing " << static_cast<uint64_t>(last_existing) << " new "
                  << static_cast<uint64_t>(first_new));
        // 1 minute candle OHLC data is stored in msecs
        if (first_new - last_existing == (60 * 1000))
        {
            DEBUG_ONLY("merging data");
            ohlc_samples->data().append(new_ohlc_samples);
            ohlc_volumes.insert(
                ohlc_volumes.end(), new_ohlc_volumes.begin(), new_ohlc_volumes.end());
            if (size_t(ohlc_samples->data().size()) != ohlc_volumes.size())
            {
                throw std::runtime_error("Data merge problem");
            }
            update = new_ohlc_samples.size();
        }
        else
        {
            throw std::runtime_error("Data OHLC time mismatch in merge");
        }
    }
    return update;
}

// ----------------------------------------------------------------------------
void ohlc_dataset::validate_ohlc(QVector<QwtOHLCSample> const &samples, double res)
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
