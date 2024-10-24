#include <QByteArray>
#include <QString>
#include <vector>
//
#include "nlohmann/json.hpp"

using nljson = nlohmann::basic_json<>;

namespace util {

  using string_t = nljson::string_t;

  void to_json(nljson& j, QString const& qstr);
  void from_json(const nljson j, QString& qstr);

  void to_json(nljson& j, QByteArray const& qba);
  void from_json(nljson& j, QByteArray& qba);
}    // namespace util
