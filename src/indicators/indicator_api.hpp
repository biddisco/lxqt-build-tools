#pragma once

#include <assert.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

/// paramlist_t - a container of objects that conform to the parameter API.
class paramlist_t
{
  /// parameter_ptr - shared_ptr to a vtable which invokes the parameter API
  struct parameter_ptr
  {
    /// invokes_parameter_API - vtable of functions which invoke the parameter API
    struct parameter_API_vtable
    {
      virtual ~parameter_API_vtable() = default;
      virtual void call_draw(std::ostream&, size_t) const = 0;
    };

    /// parameter_API_binding - templated binding of type to parameter API
    template <typename T>
    struct parameter_API_binding : parameter_API_vtable
    {
      parameter_API_binding(T x)
        : data_(std::move(x))
      {
      }

      void call_draw(std::ostream& out, size_t position) const { data_.draw(out, position); }

      T data_;
    };

    /// constructor - invokes a parameter_API_binding with inferred type
    template <typename T>
    parameter_ptr(T x)
      : binding(std::make_shared<parameter_API_binding<T>>(std::move(x)))
    {
    }

    std::shared_ptr<parameter_API_vtable const> binding;
  };

  std::vector<parameter_ptr> parameter_ptrs;

  public:
  void add(parameter_ptr p) { parameter_ptrs.emplace_back(std::move(p)); }
  parameter_ptr& operator[](int i) { return parameter_ptrs[i]; }

  void draw(std::ostream& out, size_t position) const
  {
    out << std::string(position, ' ') << "<document>" << std::endl;
    for (auto const& ptr : parameter_ptrs) ptr.binding->call_draw(out, position + 2);
    out << std::string(position, ' ') << "</document>" << std::endl;
  }
};

/// history_t - a container of documents.
class history_t
{
  std::vector<paramlist_t> documents;

  public:
  history_t(size_t size = 1)
    : documents(size)
  {
  }

  void commit()
  {
    assert(documents.size());
    documents.push_back(documents.back());
  }
  void undo()
  {
    assert(documents.size());
    documents.pop_back();
  }
  paramlist_t& current()
  {
    assert(documents.size());
    return documents.back();
  }
  paramlist_t const& current() const
  {
    assert(documents.size());
    return documents.back();
  }

  void draw(std::ostream& out, size_t position) const
  {
    out << std::string(position, ' ') << "<history>" << std::endl;
    for (auto const& doc : documents) doc.draw(out, position + 2);
    out << std::string(position, ' ') << "</history>" << std::endl;
  }
};
