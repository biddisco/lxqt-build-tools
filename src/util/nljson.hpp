#include <QByteArray>
#include <QString>
#include <vector>
//
#include "nlohmann/json.hpp"

//#include "nlohmann/../../tests/thirdparty/fifo_map/fifo_map.hpp"

// // This is a workaround solution for the "nlohmann::json" library to preserve
// // the insertion order of JSON objects instead of sorting them by name.
// // It is based on a custom map implementation by Niels Lohmann itself
// // (nlohmann::fifo_map). The actual solution (by GitHub user "Daniel599")
// // can be found here: https://github.com/nlohmann/json/issues/485#issuecomment-333652309
// template<class K, class V, class dummyCompare, class A>
// using nljson_fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;
using nljson = nlohmann::basic_json<>;    // nljson_fifo_map>;

namespace util {

  using string_t = nljson::string_t;

  void to_json(nljson& j, const QString& qstr);
  void from_json(const nljson j, QString& qstr);

  void to_json(nljson& j, const QByteArray& qba);
  void from_json(nljson& j, QByteArray& qba);
}    // namespace util
