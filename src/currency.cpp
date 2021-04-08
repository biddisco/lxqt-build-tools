#pragma once
//
#include <string>
#include <vector>
#include <algorithm>

#include <range/v3/algorithm.hpp>
#include "currency.hpp"

// ----------------------------------------------------------------------------
void add_currency(const currency &curr, std::vector<currency> &c_list)
{
    auto it = ranges::find_if(c_list, [&curr](const currency &c) {
        return c.type_ == curr.type_;
    });
    if (it==c_list.end()) {
        c_list.reserve(5);
        c_list.push_back(curr);
    }
    else {
        // copy the new currency info, but keep the old widget if it exists
        auto temp = it->widget_;
        *it = curr;
        if (temp!=nullptr) {
            it->widget_ = temp;
        }
    }
}
