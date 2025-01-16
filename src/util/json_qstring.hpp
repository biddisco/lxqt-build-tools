#pragma once

#include <QByteArray>
#include <QString>
#include <vector>
//
#include "nlohmann/json.hpp"

using nljson = nlohmann::basic_json<>;

namespace util {

  using string_t = nljson::string_t;

  void from_json(nljson const j, QString& qstr);

  void from_json(nljson& j, QByteArray& qba);
}    // namespace util
