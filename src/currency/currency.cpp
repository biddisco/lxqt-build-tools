#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
//
#include <QString>
//
#include "currency/currency.hpp"
#include "util/stringutils.hpp"

// ----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, currency_amount const& c)
{
  os << c.symbol_ << "(" << c.balance_ << ")";
  return os;
}
