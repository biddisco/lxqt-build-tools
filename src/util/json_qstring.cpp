#include <string>
//
#include <QByteArray>
#include <QString>
//
#include "nlohmann/json.hpp"
#include "util/json_qstring.hpp"

namespace util {

  void from_json(nljson const j, QString& qstr)
  {
    if (j.type() == nljson::value_t::string)
      qstr = QString::fromStdString(j.get<std::string>());
    else
      qstr = QString::fromStdString(j.dump());
  }

  void from_json(nljson& j, QByteArray& qba)
  {
    qba = QByteArray::fromStdString(j.get<std::string>());
  }

}    // namespace util
