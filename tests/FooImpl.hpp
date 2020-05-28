#pragma once

#include "example_datatypes.hpp"

#include <string>

namespace example_impl {

class FooImpl
{
 public:
  FooImpl();

  ~FooImpl();

  int foo(int&& arg1, const int& arg2) const noexcept;

  const std::string baz();

  _skipForPimpl()
  int bar(int a);

 private:
  std::string data_{"somedata"};
};

/// \note you must reflect PImpl implementation
/// before using it by code generator.
template<typename impl = FooImpl>
class _reflectForPimpl()
  PimplReflector
{};

} // namespace example_impl
