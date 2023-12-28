#include <string>
//
#include <QByteArray>
#include <QString>
//
#include "nljson.hpp"
#include "nlohmann/json.hpp"

namespace util {

  void to_json(nljson& j, const QString& qstr)
  {
    j = nljson{qstr.toStdString()};
  }

  void from_json(const nljson j, QString& qstr)
  {
    if (j.type() == nljson::value_t::string)
      qstr = QString::fromStdString(j.get<std::string>());
    else
      qstr = QString::fromStdString(j.dump());
  }

  void to_json(nljson& j, const QByteArray& qba)
  {
    j = nljson{qba.toStdString()};
  }

  void from_json(nljson& j, QByteArray& qba)
  {
    qba = QByteArray::fromStdString(j.get<std::string>());
  }

}    // namespace util
