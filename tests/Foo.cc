#include "pimpl_annotations.hpp"

#include "Foo.hpp.generated.hpp"

#include "FooImpl.hpp.generated.hpp"

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace example_interface {

/// \note Constructors, destructors and assignment operators of
/// the front-end class holding the PImpl instance as a member must be
/// defined in a place where PImpl is a complete type - usually
/// the .cpp file that includes PImpl's definition.
/// So, you cannot use the compiler-generated special member functions,
/// and it is necessary to define even empty destructors in the .cpp file.
Foo::Foo() {
}

Foo::~Foo() {
}

// will replace itself with generated code like:
// std::string foo() { return impl_; }
template<
  typename impl = example_impl::FooImpl
  , typename interface = Foo
>
class _injectPimplMethodCalls()
  PimplMethodCallsInjector
{};

} // namespace example_interface
