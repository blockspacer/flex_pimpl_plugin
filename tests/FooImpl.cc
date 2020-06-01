#include "pimpl_annotations.hpp"

#include "FooImpl.hpp.generated.hpp"

namespace example_impl {

int FooImpl::foo(int&& arg1, const int& arg2) const noexcept {
  return 1234;
}

const std::string FooImpl::baz() {
  return data_;
}

int FooImpl::bar(int a) {
  return 678;
}

FooImpl::FooImpl() {
}

FooImpl::~FooImpl() {
}

} // namespace example_impl
