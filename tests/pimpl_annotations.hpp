#pragma once

// Storage used by PImpl.
// Will replace itself with generated code like:
// ::basis::pimpl::FastPimpl<FooImpl, /*Size*/ 64, /*Alignment*/ 12> impl_;
/// \note sizePadding adds extra bytes
/// to aligned_storage used by fast PImpl
#define _injectPimplStorage(settings) \
  __attribute__((annotate("{gen};{funccall};inject_pimpl_storage(" settings ")")))

/**
 * generates code similar to:
 *  std::string foo(int arg1) { return impl->foo(arg1); };
 * EXAMPLE:
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
 * EXAMPLE:
  // will replace itself with generated code like:
  // std::string foo() { return impl_; }
  template<
    typename impl = example_impl::FooImpl
    , typename interface = Foo
  >
  class _injectPimplMethodCalls()
    PimplMethodCallsInjector
  {};
 **/
#define _injectPimplMethodCalls(settings) \
  __attribute__((annotate("{gen};{funccall};inject_pimpl_method_calls(" settings ")")))

/// \note you must reflect PImpl implementation
/// before using it by code generator.
#define _reflectForPimpl(settings) \
  __attribute__((annotate("{gen};{funccall};reflect_for_pimpl(" settings ")")))

// skip injection of some methods from implementation
#define _skipForPimpl() \
  __attribute__((annotate("skip_pimpl")))
