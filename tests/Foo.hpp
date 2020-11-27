#pragma once

#include "pimpl_annotations.hpp"

#include <basis/core/pimpl.hpp>

#include <string>

/// \note store implementation in separate namespace
/// for example purposes
namespace example_impl {

// forward declaration, the crux of the Pimpl pattern
class FooImpl;

} // namespace example_impl

namespace example_interface {

class Foo {
public:
  Foo();

  ~Foo();

  // Proxy method calls based on PImpl implementation.
  // Will replace itself with generated code like:
  // std::string foo();
  // int bar(int&& arg1, const int& arg2) const noexcept;
  template<typename impl = example_impl::FooImpl>
  class
    _injectPimplMethodCalls(
      "without_method_body"
    )
  PimplMethodDeclsInjector
  {};

private:

  // Storage used by PImpl.
  // Will replace itself with generated code like:
  // ::basis::pimpl::FastPimpl<FooImpl, /*Size*/ 64, /*Alignment*/ 12> impl_;
  /// \note sizePadding adds extra bytes
  /// to aligned_storage used by fast PImpl
  template<
    typename impl = example_impl::FooImpl
  >
  class
    _injectPimplStorage(
      "sizePadding = 8"
    )
  PimplStorageInjector
  {};
};

} // namespace example_interface
