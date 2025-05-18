#include <QString>
#include <list>

// ----------------------------------------------------------------------------
struct control_field_data
{
  QString name;
  QString type;
  std::list<control_field_data> subfields;
};
