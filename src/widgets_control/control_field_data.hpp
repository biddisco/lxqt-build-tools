#include <QString>
#include <list>

// ----------------------------------------------------------------------------
struct control_field_data
{
  QString name;
  QString type;
  std::list<control_field_data> subfields;
};

// ----------------------------------------------------------------------------
static QString to_qstring(std::list<control_field_data> fields, QString const& prefix = "")
{
  QString result;
  int N = fields.size();
  for (control_field_data const& field : fields)
  {
    QString fullName = prefix.isEmpty() ? field.name : prefix + "." + field.name;
    if (field.type == "object")
    {
      result += "{ " + field.name + ", " + field.type + ",\n" +
          to_qstring(field.subfields, fullName) + "\n}";
    }
    else { result += "{ " + field.name + ", " + field.type + ", {} }"; }
    if (--N != 0) result += "\n";
  }
  return result;
}
