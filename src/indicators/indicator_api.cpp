#include "indicators/indicator_api.hpp"

class my_string_t
{
  std::string str;
  //
  public:
  my_string_t(std::string str)
    : str(str)
  {
  }
  void draw(std::ostream& out, std::size_t position) const
  {
    out << std::string(position, ' ') << str << std::endl;
  }
};

class my_int_t
{
  int i;
  //
  public:
  my_int_t(int i)
    : i(i)
  {
  }
  void draw(std::ostream& out, std::size_t position) const
  {
    out << std::string(position, ' ') << i << std::endl;
  }
};

class my_class_t
{
  public:
  void draw(std::ostream& out, size_t position) const
  {
    out << std::string(position, ' ') << "my_class" << std::endl;
  }
};

/*
int main()
{
  history_t h;

  h.current().add(my_string_t("Hello"));
  h.current().add(my_int_t(0));
  h.current().add(my_class_t());

  h.draw(cout, 0);
  cout << "--------------------------" << endl;

  h.commit();

  h.current()[1] = my_string_t("World!");
  h.current().add(h);

  h.draw(cout, 0);
  cout << "--------------------------" << endl;

  h.undo();

  h.draw(cout, 0);
}
*/
