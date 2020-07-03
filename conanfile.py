from conans import ConanFile, CMake, tools, python_requires
import traceback
import os
import shutil
from conans.tools import OSInfo

# if you using python less than 3 use from distutils import strtobool
from distutils.util import strtobool

# conan runs the methods in this order:
# config_options(),
# configure(),
# requirements(),
# package_id(),
# build_requirements(),
# build_id(),
# system_requirements(),
# source(),
# imports(),
# build(),
# package(),
# package_info()

conan_build_helper = python_requires("conan_build_helper/[~=0.0]@conan/stable")

class flex_pimpl_plugin_conan_project(conan_build_helper.CMakePackage):
    name = "flex_pimpl_plugin"

    version = "master"
    url = "https://CHANGE_ME"
    license = "MIT" # CHANGE_ME
    author = "CHANGE_ME <>"

    description = "flex_pimpl_plugin: C++ compile-time programming (serialization, reflection, code modification, enum to string, better enum, enum to json, extend or parse language, etc.)."
    topics = ('c++')

    options = {
        "shared": [True, False],
        "use_system_boost": [True, False],
        "enable_clang_from_conan": [True, False],
        "enable_sanitizers": [True, False]
    }

    default_options = (
        #"*:shared=False",
        "shared=True",
        "enable_clang_from_conan=False",
        "use_system_boost=False",
        "enable_sanitizers=False",
        # boost
        "boost:without_atomic=True",
        "boost:without_chrono=True",
        "boost:without_container=True",
        "boost:without_context=True",
        "boost:without_coroutine=True",
        "boost:without_graph=True",
        "boost:without_graph_parallel=True",
        "boost:without_log=True",
        "boost:without_math=True",
        "boost:without_mpi=True",
        "boost:without_serialization=True",
        "boost:without_test=True",
        "boost:without_timer=True",
        "boost:without_type_erasure=True",
        "boost:without_wave=True",
        # llvm
        "llvm:shared=False",
        "compiler-rt:shared=False",
        "clang:shared=False",
        "llvm_headers:shared=False",
        "clang_headers:shared=False",
        "clang_executables:shared=False",
        "llvm_demangle:shared=False",
        "llvm_support:shared=False",
        "llvm_binary_format:shared=False",
        "llvm_core:shared=False",
        "llvm_mc:shared=False",
        "llvm_bit_reader:shared=False",
        "llvm_mc_parser:shared=False",
        "llvm_object:shared=False",
        "llvm_profile_data:shared=False",
        "llvm_analysis:shared=False",
        "llvm_transform_utils:shared=False",
        "llvm_instcombine:shared=False",
        "llvm_bit_writer:shared=False",
        "llvm_target:shared=False",
        "llvm_scalar_opts:shared=False",
        "llvm_option:shared=False",
        "llvm_debuginfo_codeview:shared=False",
        "llvm_codegen:shared=False",
        "llvm_x86_utils:shared=False",
        "llvm_x86_asm_printer:shared=False",
        "llvm_mc_disassembler:shared=False",
        "llvm_debuginfo_msf:shared=False",
        "llvm_global_isel:shared=False",
        "llvm_asm_printer:shared=False",
        "llvm_x86_info:shared=False",
        "llvm_x86_asm_parser:shared=False",
        "llvm_x86_desc:shared=False",
        "llvm_selection_dag:shared=False",
        "clang_lex:shared=False",
        "clang_basic:shared=False",
        "llvm_x86_codegen:shared=False",
        "clang_analysis:shared=False",
        "clang_ast:shared=False",
        # flexlib
        "flexlib:shared=False",
        "flexlib:enable_clang_from_conan=False",
        # flextool
        #"flextool:shared=False",
        "flextool:enable_clang_from_conan=False",
        # FakeIt
        "FakeIt:integration=catch",
        # openssl
        "openssl:shared=True",
        # flex_reflect_plugin
        "flex_reflect_plugin:shared=True",
        # chromium_base
        "chromium_base:use_alloc_shim=True",
        # chromium_tcmalloc
        "chromium_tcmalloc:use_alloc_shim=True",
    )

    # Custom attributes for Bincrafters recipe conventions
    _source_subfolder = "."
    _build_subfolder = "."

    # NOTE: no cmake_find_package due to custom FindXXX.cmake
    #generators = "cmake", "cmake_paths", "virtualenv"
    generators = "cmake", "cmake_paths"

    # Packages the license for the conanfile.py
    #exports = ["LICENSE.md"]

    # If the source code is going to be in the same repo as the Conan recipe,
    # there is no need to define a `source` method. The source folder can be
    # defined like this
    exports_sources = ("LICENSE", "*.md", "include/*", "src/*",
                       "cmake/*", "CMakeLists.txt", "tests/*", "benchmarks/*",
                       "scripts/*", "tools/*", "codegen/*", "assets/*", "conf/*",
                       "docs/*", "licenses/*", "patches/*", "resources/*",
                       "submodules/*", "thirdparty/*", "third-party/*",
                       "third_party/*", "flex_pimpl_plugin/*")

    #settings = "os", "compiler", "build_type", "arch"
    settings = "os_build", "os", "arch", "compiler", "build_type", "arch_build"

    #def source(self):
    #  url = "https://github.com/....."
    #  self.run("git clone %s ......." % url)

    def build_requirements(self):
        self.build_requires("cmake_platform_detection/master@conan/stable")
        self.build_requires("cmake_build_options/master@conan/stable")
        self.build_requires("cmake_helper_utils/master@conan/stable")

    def requirements(self):

      if self._is_tests_enabled():
          self.requires("catch2/[>=2.1.0]@bincrafters/stable")
          self.requires("gtest/[>=1.8.0]@bincrafters/stable")
          self.requires("FakeIt/[>=2.0.4]@gasuketsu/stable")

      if not self.options.use_system_boost:
          self.requires("boost/1.71.0@dev/stable")

      self.requires("chromium_build_util/master@conan/stable")

      if self.options.enable_clang_from_conan:
        self.requires("llvm_support/6.0.1@Manu343726/testing")
        self.requires("libclang/6.0.1@Manu343726/testing")
        self.requires("clang_tooling/6.0.1@Manu343726/testing")
        self.requires("clang_tooling_core/6.0.1@Manu343726/testing")
        self.requires("clang_analysis/6.0.1@Manu343726/testing")
        self.requires("clang_ast/6.0.1@Manu343726/testing")
        self.requires("llvm/6.0.1@Manu343726/testing")
      else:
        self.requires("cling_conan/master@conan/stable")

      self.requires("chromium_base/master@conan/stable")

      self.requires("basis/master@conan/stable")

      self.requires("corrade/2020.06@magnum/stable")

      self.requires("flextool/master@conan/stable")

      # \note dispatcher must be thread-safe,
      # so use entt after patch https://github.com/skypjack/entt/issues/449
      # see https://github.com/skypjack/entt/commit/74f3df83dbc9fc4b43b8cfb9d71ba02234bd5c4a
      self.requires("entt/3.3.2")

      self.requires("flex_reflect_plugin/master@conan/stable")

      self.requires("flex_squarets_plugin/master@conan/stable")

    def _configure_cmake(self):
        cmake = CMake(self)
        cmake.parallel = True
        cmake.verbose = True

        cmake.definitions["CONAN_AUTO_INSTALL"] = 'OFF'

        self.output.info(self.options)
        if self.options.shared:
          self.output.info('Enabled flex_pimpl_plugin_BUILD_SHARED_LIBS')
          cmake.definitions["flex_pimpl_plugin_BUILD_SHARED_LIBS"] = "ON"
        else:
          self.output.error('Disabled flex_pimpl_plugin_BUILD_SHARED_LIBS')

        self.add_cmake_option(cmake, "ENABLE_TESTS", self._is_tests_enabled())

        self.add_cmake_option(cmake, "ENABLE_SANITIZERS", self.options.enable_sanitizers)

        cmake.configure(build_folder=self._build_subfolder)

        if self.settings.compiler == 'gcc':
            cmake.definitions["CMAKE_C_COMPILER"] = "gcc-{}".format(
                self.settings.compiler.version)
            cmake.definitions["CMAKE_CXX_COMPILER"] = "g++-{}".format(
                self.settings.compiler.version)

        if not self.options.enable_clang_from_conan:
          cmake.definitions["ENABLE_CLING"] = 'ON'

        cmake.definitions["ENABLE_SANITIZERS"] = 'ON'
        if not self.options.enable_sanitizers:
            cmake.definitions["ENABLE_SANITIZERS"] = 'OFF'

        cmake.definitions["CMAKE_TOOLCHAIN_FILE"] = 'conan_paths.cmake'

        return cmake

    def package(self):
        self.copy(pattern="LICENSE", dst="licenses", src=self._source_subfolder)
        cmake = self._configure_cmake()
        cmake.install()
        # Local build
        # see https://docs.conan.io/en/latest/developing_packages/editable_packages.html
        if not self.in_local_cache:
            self.copy("conanfile.py", dst=".", keep_path=False)

    def build(self):
        cmake = self._configure_cmake()
        if self.settings.compiler == 'gcc':
            cmake.definitions["CMAKE_C_COMPILER"] = "gcc-{}".format(
                self.settings.compiler.version)
            cmake.definitions["CMAKE_CXX_COMPILER"] = "g++-{}".format(
                self.settings.compiler.version)

        #cmake.definitions["CMAKE_TOOLCHAIN_FILE"] = 'conan_paths.cmake'

        # The CMakeLists.txt file must be in `source_folder`
        cmake.configure(source_folder=".")

        cpu_count = tools.cpu_count()
        self.output.info('Detected %s CPUs' % (cpu_count))

        # -j flag for parallel builds
        cmake.build(args=["--", "-j%s" % cpu_count])

        if self._is_tests_enabled():
          self.output.info('Running tests')
          cmake.build(args=["--target", "flex_pimpl_plugin_run_all_tests", "--", "-j%s" % cpu_count])
          #self.run('ctest --parallel %s' % (cpu_count))
          # TODO: use cmake.test()

    # Importing files copies files from the local store to your project.
    def imports(self):
        dest = os.getenv("CONAN_IMPORT_PATH", "bin")
        self.copy("license*", dst=dest, ignore_case=True)
        self.copy("*.dll", dst=dest, src="bin")
        self.copy("*.so*", dst=dest, src="bin")
        self.copy("*.pdb", dst=dest, src="lib")
        self.copy("*.dylib*", dst=dest, src="lib")
        self.copy("*.lib*", dst=dest, src="lib")
        self.copy("*.a*", dst=dest, src="lib")

    # package_info() method specifies the list of
    # the necessary libraries, defines and flags
    # for different build configurations for the consumers of the package.
    # This is necessary as there is no possible way to extract this information
    # from the CMake install automatically.
    # For instance, you need to specify the lib directories, etc.
    def package_info(self):
        #self.cpp_info.libs = ["flex_pimpl_plugin"]

        self.cpp_info.includedirs = ["include"]
        self.cpp_info.libs = tools.collect_libs(self)
        self.cpp_info.libdirs = ["lib"]
        self.cpp_info.bindirs = ["bin"]
        self.env_info.LD_LIBRARY_PATH.append(
            os.path.join(self.package_folder, "lib"))
        self.env_info.PATH.append(os.path.join(self.package_folder, "bin"))
        for libpath in self.deps_cpp_info.lib_paths:
            self.env_info.LD_LIBRARY_PATH.append(libpath)

        self.env_info.flex_pimpl_plugin_ROOT = self.package_folder
        #flex_pimpl_plugin = "flex_pimpl_plugin.dll" if self.settings.os_build == "Windows" else "flex_pimpl_plugin.so"
        #self.env_info.flex_pimpl_plugin_LIB = os.path.normpath(os.path.join(self.package_folder, "lib", flex_pimpl_plugin))

        #self.cpp_info.includedirs.append(os.getcwd())
        #self.cpp_info.includedirs.append(
        #  os.path.join("base", "third_party", "flex_pimpl_plugin"))
        #self.cpp_info.includedirs.append(
        #  os.path.join("base", "third_party", "flex_pimpl_plugin", "compat"))

        #if self.settings.os == "Linux":
        #  self.cpp_info.defines.append('HAVE_CONFIG_H=1')

        # in linux we need to link also with these libs
        #if self.settings.os == "Linux":
        #    self.cpp_info.libs.extend(["pthread", "dl", "rt"])

        #self.cpp_info.libs = tools.collect_libs(self)
        #self.cpp_info.defines.append('PDFLIB_DLL')
