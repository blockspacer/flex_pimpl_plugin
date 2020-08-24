# About

Plugin for [https://github.com/blockspacer/flextool](https://github.com/blockspacer/flextool)

Plugin provides support for pimpl pattern also known as 'Pointer to Implementation'.

According to this pattern C++ developer divides the class into two parts: public (interface) part and private (implementation).

Both parts are C++ classes, but only interface is visible to the class users, and implementation is hidden behind pointer.

Source file with public class implementation contains a lot of boilerplate code.

We use flextool (libtooling) to generate code and reduce the amount of boilerplate code.

Note that plugin output is valid C++ code: you can open generated files and debug them as usual.

If you do not know why to use C++ pimpl see [https://www.bfilipek.com/2018/01/pimpl.html](https://www.bfilipek.com/2018/01/pimpl.html)

See for details about flextool [https://blockspacer.github.io/flex_docs/](https://blockspacer.github.io/flex_docs/)

## How it works

See [https://blockspacer.github.io/flex_docs/tutorial/](https://blockspacer.github.io/flex_docs/tutorial/)

## Usage

See `flex_pimpl_plugin/tests` for example code.

i.e. `tests/Foo.hpp` and `tests/FooImpl.hpp`


```cpp
// will replace itself with generated code like:
// std::string foo() { return impl_->foo(); }
template<
  typename impl = FooImpl
  , typename interface = Foo
>
class _injectPimplMethodCalls()
  PimplMethodCallsInjector
{};

// Proxy method calls based on PImpl implementation.
// Will replace itself with generated code like:
// std::string foo();
// int bar(int&& arg1, const int& arg2) const noexcept;
template<typename impl = FooImpl>
class
  _injectPimplMethodCalls(
    "without_method_body"
  )
PimplMethodDeclsInjector
{};

// Storage used by PImpl.
// Will replace itself with generated code like:
// pimpl::FastPimpl<FooImpl, /*Size*/ 64, /*Alignment*/ 12> impl_;
/// \note sizePadding adds extra bytes
/// to aligned_storage used by fast PImpl
template<
  typename impl = FooImpl
>
class
  _injectPimplStorage(
    "sizePadding = 8"
  )
PimplStorageInjector
{};

/// \note you must reflect PImpl implementation
/// before using it by code generator.
template<typename impl = FooImpl>
class _reflectForPimpl()
  PimplReflector
{};
```

## How to skip injection of some methods from implementation

You can annotate methods with "skip_pimpl":

```cpp
#define _skipForPimpl() \
  __attribute__((annotate("skip_pimpl")))
```

Code generator processes `flextool_input_files` from `tests/CMakeLists.txt`

## Before installation

- [installation guide](https://blockspacer.github.io/flex_docs/download/)

## Installation

```bash
export CXX=clang++-6.0
export CC=clang-6.0

# NOTE: change `build_type=Debug` to `build_type=Release` in production
# NOTE: use --build=missing if you got error `ERROR: Missing prebuilt package`
CONAN_REVISIONS_ENABLED=1 \
CONAN_VERBOSE_TRACEBACK=1 \
CONAN_PRINT_RUN_COMMANDS=1 \
CONAN_LOGGING_LEVEL=10 \
GIT_SSL_NO_VERIFY=true \
    cmake -E time \
      conan create . conan/stable \
      -s build_type=Debug -s cling_conan:build_type=Release \
      --profile clang \
          -o flex_pimpl_plugin:enable_clang_from_conan=False \
          -e flex_pimpl_plugin:enable_tests=True
```

## CMake and conan integration

Example code can be found in `flex_pimpl_plugin/tests` directory.

## Development flow (for contributors)

Commands below may be used to build project locally, without system-wide installation.

```bash
export CXX=clang++-6.0
export CC=clang-6.0

cmake -E remove_directory build

cmake -E make_directory build

# NOTE: change `build_type=Debug` to `build_type=Release` in production
build_type=Debug

# install conan requirements
CONAN_REVISIONS_ENABLED=1 \
    CONAN_VERBOSE_TRACEBACK=1 \
    CONAN_PRINT_RUN_COMMANDS=1 \
    CONAN_LOGGING_LEVEL=10 \
    GIT_SSL_NO_VERIFY=true \
        cmake -E chdir build cmake -E time \
            conan install \
            -s build_type=${build_type} -s cling_conan:build_type=Release \
            --build=missing \
            --profile clang \
                -e enable_tests=True \
                ..

# optional: remove generated files (change paths to yours)
rm build/*generated*
rm build/generated/ -rf
rm build/bin/${build_type}/ -rf

# configure via cmake
cmake -E chdir build \
  cmake -E time cmake .. \
  -DENABLE_TESTS=TRUE \
  -DCONAN_AUTO_INSTALL=OFF \
  -DCMAKE_BUILD_TYPE=${build_type}

# build code
cmake -E chdir build \
  cmake -E time cmake --build . \
  --config ${build_type} \
  -- -j8

# run unit tests
cmake -E chdir build \
  cmake -E time cmake --build . \
  --config ${build_type} \
  --target flex_pimpl_plugin_run_all_tests
```

## For contibutors: conan editable mode

With the editable packages, you can tell Conan where to find the headers and the artifacts ready for consumption in your local working directory.
There is no need to run `conan create` or `conan export-pkg`.

See for details [https://docs.conan.io/en/latest/developing_packages/editable_packages.html](https://docs.conan.io/en/latest/developing_packages/editable_packages.html)

Build locally:

```bash
CONAN_REVISIONS_ENABLED=1 \
CONAN_VERBOSE_TRACEBACK=1 \
CONAN_PRINT_RUN_COMMANDS=1 \
CONAN_LOGGING_LEVEL=10 \
GIT_SSL_NO_VERIFY=true \
  cmake -E time \
    conan install . \
    --install-folder local_build \
    -s build_type=Debug -s cling_conan:build_type=Release \
    --profile clang \
      -o flex_pimpl_plugin:enable_clang_from_conan=False \
      -e flex_pimpl_plugin:enable_tests=True

CONAN_REVISIONS_ENABLED=1 \
CONAN_VERBOSE_TRACEBACK=1 \
CONAN_PRINT_RUN_COMMANDS=1 \
CONAN_LOGGING_LEVEL=10 \
GIT_SSL_NO_VERIFY=true \
  cmake -E time \
    conan source . --source-folder local_build

conan build . \
  --build-folder local_build

conan package . \
  --build-folder local_build \
  --package-folder local_build/package_dir
```

Set package to editable mode:

```bash
conan editable add local_build/package_dir \
  flex_pimpl_plugin/master@conan/stable
```

Note that `conanfile.py` modified to detect local builds via `self.in_local_cache`

After change source in folder local_build (run commands in source package folder):

```bash
conan build . \
  --build-folder local_build

conan package . \
  --build-folder local_build \
  --package-folder local_build/package_dir
```

Build your test project

In order to revert the editable mode just remove the link using:

```bash
conan editable remove \
  flex_pimpl_plugin/master@conan/stable
```

## Similar projects

- https://github.com/notetau/pimpl-cpp-generator
- https://github.com/sqjk/pimpl_ptr
- https://github.com/flexferrum/autoprogrammer/wiki/pImpl-implementation-generator
