#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>
//
#include "src/data/ohlc_data_resolutions.hpp"
#include "src/data/ohlc_dataset_view.hpp"

// ----------------------------------------------------------------------------
struct indicator_base
{
    virtual ~indicator_base();
    virtual const std::string getname() const = 0;
    virtual const int get_num_datasets() {return 1;}
    virtual const int get_num_params() { return 1;}
    virtual std::vector<candle_res> get_datasets() = 0;
    virtual std::vector<std::tuple<std::string, double>> get_default_values() = 0;
    virtual void generate(std::shared_ptr<ohlc_dataset_view> data) {}
};

// ----------------------------------------------------------------------------
struct indicator_moving_average
{
    const std::string name = "Moving Average";
    static const int num_datasets = 1;
    static const int num_params = 1;
    std::array<std::tuple<std::string, candle_res>, num_datasets> datasets = {
        std::make_tuple<>("Samples", ohlc_data_resolutions::minute15)
    };
    std::array<std::tuple<std::string, double>, num_params> params = {
        std::make_tuple<std::string, double>("Window length", 15.0)
    };

    void generate(std::shared_ptr<ohlc_dataset_view> data) {}
};

using indicator_types = std::variant<indicator_moving_average>;

const std::vector<indicator_types> available_indicators = {
    indicator_moving_average{},
//    {"Heikin Ashi", 1, 0, {}},
//    {"MA gradient", 1, 0, {}},
//    {"MA cross",    2, 0, {}},
//    {"MACD",        1, 3, {12, 26, 9}},
};
