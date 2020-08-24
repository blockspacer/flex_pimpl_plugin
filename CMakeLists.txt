# see https://github.com/chromium/chromium/blob/9db0b5162b8648833c3a6b1af044cebd9d7d3855/base/BUILD.gn

cmake_minimum_required(VERSION 3.4)

# project() must be before checks, see https://stackoverflow.com/a/26437667/10904212
set(PROJECT_NAME "flex_pimpl_plugin")
set(PROJECT_VERSION 1.0.0.1)
project(${PROJECT_NAME}
  VERSION ${PROJECT_VERSION}
  LANGUAGES CXX C)

set(LIB_NAME ${PROJECT_NAME})

option(ENABLE_TESTS "Enable tests" OFF)

# path to /generated folder,
# auto-completion in IDE will not work
# if IDE can not find header file
# NOTE: don't forget to set ENABLE_TESTS in IDE settings
# and build type to Debug
set(GENERATED_FOLDER_FOR_IDE
  # default must be same as ${flextool_outdir}
  "${CMAKE_CURRENT_BINARY_DIR}"
  CACHE
  STRING "GENERATED_FOLDER_FOR_IDE" )

# used by https://docs.conan.io/en/latest/developing_packages/workspaces.html
get_filename_component(LOCAL_BUILD_ABSOLUTE_ROOT_PATH
  "${PACKAGE_flex_pimpl_plugin_SRC}"
  ABSOLUTE)
if(EXISTS "${LOCAL_BUILD_ABSOLUTE_ROOT_PATH}")
  # path to Find*.cmake file
  list(PREPEND CMAKE_MODULE_PATH "${LOCAL_BUILD_ABSOLUTE_ROOT_PATH}/cmake")
endif()

set(${PROJECT_NAME}_BUILD_SHARED_LIBS
  TRUE CACHE BOOL
  "Use .so/.dll")

if(${PROJECT_NAME}_BUILD_SHARED_LIBS)
  set(CORE_LIB_TYPE SHARED)
else()
  set(CORE_LIB_TYPE STATIC)
  message(FATAL_ERROR "ONLY SHARED PLUGINS SUPPORTED FOR NOW")
endif(${PROJECT_NAME}_BUILD_SHARED_LIBS)

set(ENABLE_CLING TRUE CACHE BOOL "ENABLE_CLING")
message(STATUS "ENABLE_CLING=${ENABLE_CLING}")

set(ENABLE_CLANG_FROM_CONAN FALSE CACHE BOOL "ENABLE_CLANG_FROM_CONAN")
message(STATUS "ENABLE_CLANG_FROM_CONAN=${ENABLE_CLANG_FROM_CONAN}")

if(ENABLE_CLANG_FROM_CONAN AND ENABLE_CLING)
  message(FATAL_ERROR
    "don't use both ENABLE_CLING and ENABLE_CLANG_FROM_CONAN at the same time. cling already provides clang libtooling")
endif()

set(CUSTOM_PLUGINS
    "${CMAKE_CURRENT_SOURCE_DIR}/custom_plugins.cmake"
    CACHE STRING
    "Path to custom plugins")
message(STATUS "CUSTOM_PLUGINS=${CUSTOM_PLUGINS}")
if(EXISTS ${CUSTOM_PLUGINS})
  include(${CUSTOM_PLUGINS})
endif()

# --------------------------- conan configuration ------------------------------

# Note: FetchContent is available since CMake 3.11
# see https://cmake.org/cmake/help/git-master/module/FetchContent.html
# By default, everything is downloaded into your build directory
# Once CMake successfully downloads our external content, it sets two variables that can be used in CMakeLists.txt to locate the new data:
# 1 <resource_name>_SOURCE_DIR
#   specifies the location of the downloaded sources,
# 2 <resource_name>_BINARY_DIR
#   specifies where is the default build directory for the downloaded sources.
include(FetchContent)

set(FetchContent_cmake_utils "cmake_utils")
FetchContent_Declare(
  ${FetchContent_cmake_utils}
  PREFIX external_dependencies/${FetchContent_cmake_utils}
  GIT_REPOSITORY https://github.com/blockspacer/cmake_utils.git
  GIT_TAG origin/master
  # Disable warning about detached HEAD https://stackoverflow.com/a/36794768
  GIT_CONFIG        advice.detachedHead=false
  SOURCE_DIR        "${CMAKE_BINARY_DIR}/${FetchContent_cmake_utils}"
  BINARY_DIR        "${CMAKE_BINARY_DIR}/${FetchContent_cmake_utils}-build"
  CMAKE_ARGS        "-Wno-dev"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     ""
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
)
FetchContent_GetProperties(${FetchContent_cmake_utils})
if (NOT ${FetchContent_cmake_utils}_POPULATED)
  # Populate command can be used only once for every resource
  # during cmake configuration, which explains the conditional above.
  FetchContent_Populate(${FetchContent_cmake_utils})
endif()
message(STATUS "${FetchContent_cmake_utils}_SOURCE_DIR = ${${FetchContent_cmake_utils}_SOURCE_DIR}")
message(STATUS "${FetchContent_cmake_utils}_BINARY_DIR = ${${FetchContent_cmake_utils}_BINARY_DIR}")
include(${${FetchContent_cmake_utils}_SOURCE_DIR}/Utils.cmake)

set(FetchContent_conan_auto_install "conan_auto_install")
FetchContent_Declare(
  ${FetchContent_conan_auto_install}
  PREFIX external_dependencies/${FetchContent_conan_auto_install}
  GIT_REPOSITORY https://github.com/blockspacer/conan_auto_install.git
  GIT_TAG origin/master
  # Disable warning about detached HEAD https://stackoverflow.com/a/36794768
  GIT_CONFIG        advice.detachedHead=false
  SOURCE_DIR        "${CMAKE_BINARY_DIR}/${FetchContent_conan_auto_install}"
  BINARY_DIR        "${CMAKE_BINARY_DIR}/${FetchContent_conan_auto_install}-build"
  CMAKE_ARGS        "-Wno-dev"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     ""
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
)
FetchContent_GetProperties(${FetchContent_conan_auto_install})
if (NOT ${FetchContent_conan_auto_install}_POPULATED)
  # Populate command can be used only once for every resource
  # during cmake configuration, which explains the conditional above.
  FetchContent_Populate(${FetchContent_conan_auto_install})
endif()
message(STATUS "${FetchContent_conan_auto_install}_SOURCE_DIR = ${${FetchContent_conan_auto_install}_SOURCE_DIR}")
message(STATUS "${FetchContent_conan_auto_install}_BINARY_DIR = ${${FetchContent_conan_auto_install}_BINARY_DIR}")
include(${${FetchContent_conan_auto_install}_SOURCE_DIR}/conan-auto-install.cmake)
option(CONAN_AUTO_INSTALL "Let CMake call conan install automatically"
  OFF
)
if (CONAN_AUTO_INSTALL)
  set(CONAN_PROFILE
      "clang"
      CACHE STRING "Conan profile to use during installation")
  if (NOT CMAKE_BUILD_TYPE MATCHES "Debug" )
    set(conan_build_type "Release")
  else()
    set(conan_build_type "Debug")
  endif()
  # No --build=missing cause llvm requires long build
  conan_auto_install(
    CONAN_OPTIONS "--profile=${CONAN_PROFILE} -s build_type=${conan_build_type} -s cling_conan:build_type=Release -o openssl:shared=True"
    #FORCE
  )
endif()

if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake")
  list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_BINARY_DIR}/)
  include(${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake)
  include(${CMAKE_CURRENT_BINARY_DIR}/conan_paths.cmake OPTIONAL)
  conan_basic_setup(
    # prevent conan_basic_setup from resetting cmake variables
    TARGETS
    KEEP_RPATHS
    # see https://github.com/conan-io/conan/issues/6012
    NO_OUTPUT_DIRS
    )
else()
  message (FATAL_ERROR "must use conan")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# Generate clang compilation database
# see https://stackoverflow.com/a/31086619/10904212
#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(cmake_platform_detection REQUIRED)
run_cmake_platform_detection()

find_package(cmake_build_options REQUIRED)
setup_default_build_type(RELEASE)
setup_cmake_build_options(RELEASE DEBUG)

message(STATUS "Compiler ${CMAKE_CXX_COMPILER}, version: ${CMAKE_CXX_COMPILER_VERSION}")

set_project_version(0 0 1) # from Utils.cmake

check_cmake_build_type_selected() # from Utils.cmake

enable_colored_diagnostics() # from Utils.cmake

print_cmake_system_info() # from Utils.cmake

check_supported_os() # from Utils.cmake

find_package(cmake_helper_utils REQUIRED)

# prefer ASCII for folder names
force_latin_paths() # from cmake_helper_utils (conan package)

# out dirs (CMAKE_*_OUTPUT_DIRECTORY) must be not empty
validate_out_dirs() # from cmake_helper_utils (conan package)

# In-source builds not allowed
validate_out_source_build(WARNING) # from cmake_helper_utils (conan package)

# Keep symbols for JIT resolution
set(LLVM_NO_DEAD_STRIP 1)

if(NOT TARGET CONAN_PKG::chromium_base)
  message(FATAL_ERROR "Use chromium_base from conan")
endif()

if(NOT TARGET CONAN_PKG::chromium_build_util)
  message(FATAL_ERROR "Use chromium_build_util from conan")
endif()

find_package(chromium_build_util REQUIRED)
#
if(TARGET chromium_build_util::chromium_build_util-static)
  set(build_util_LIB "chromium_build_util::chromium_build_util-static")
else()
  message(FATAL_ERROR "not supported: using system provided chromium_build_util library")
endif()

find_package(chromium_base REQUIRED)
if(NOT TARGET ${base_LIB})
  message(FATAL_ERROR "not supported ${base_LIB}: using system provided chromium_base library")
endif()

# see https://doc.magnum.graphics/corrade/corrade-cmake.html#corrade-cmake-subproject
find_package(Corrade REQUIRED PluginManager)

if(NOT TARGET CONAN_PKG::corrade)
  message(FATAL_ERROR "Use corrade from conan")
endif()

find_package( X11 REQUIRED )
message(STATUS "X11_LIBRARIES = ${X11_LIBRARIES}")

find_package( EXPAT REQUIRED )
message(STATUS "EXPAT_LIBRARIES = ${EXPAT_LIBRARIES}")

find_package( ZLIB REQUIRED )
message(STATUS "ZLIB_LIBRARIES = ${ZLIB_LIBRARIES}")

#add_library( boost_outcome INTERFACE )
#target_include_directories( boost_outcome SYSTEM INTERFACE "submodules/boost.outcome/include" )
#get_target_property (BOOST_OUTCOME_IMPORTED_LOCATION boost_outcome INTERFACE_INCLUDE_DIRECTORIES)
#message( STATUS "boost_outcome=${BOOST_OUTCOME_IMPORTED_LOCATION}" )
#
#add_library(microsoft_gsl INTERFACE)
#target_include_directories(microsoft_gsl SYSTEM INTERFACE "submodules/GSL/include")
#get_target_property (microsoft_gsl_IMPORTED_LOCATION microsoft_gsl INTERFACE_INCLUDE_DIRECTORIES)
#message( STATUS "microsoft_gsl=${microsoft_gsl_IMPORTED_LOCATION}" )

message(STATUS "CMAKE_DL_LIBS = ${CMAKE_DL_LIBS}")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/)

set(flex_pimpl_plugin_include_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/flex_pimpl_plugin")
set(flex_pimpl_plugin_src_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PlatformChecks.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CommonOptions.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProjectFiles.cmake)

add_library(${LIB_NAME}-test-includes INTERFACE)

# $<INSTALL_INTERFACE:...> is exported using install(EXPORT)
# $<BUILD_INTERFACE:...> is exported using export(), or when the target is used by another target in the same buildsystem
macro(add_relative_include_dir TARGET VISIBILITY_BUILD VISIBILITY_INSTALL NEW_ELEM)
  target_include_directories(${TARGET}
    ${VISIBILITY_BUILD} "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${NEW_ELEM}>"
    ${VISIBILITY_INSTALL} "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}/${NEW_ELEM}>"
  )
  target_include_directories( ${LIB_NAME}-test-includes SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/${NEW_ELEM} )
endmacro(add_relative_include_dir)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

list(APPEND CMAKE_PROGRAM_PATH ${CONAN_BIN_DIRS})
# TODO: make better: link lib dirs
link_directories(${CONAN_LIB_DIRS})
#
find_package(basis REQUIRED)
if(${basis_HEADER_DIR} STREQUAL "")
  message(FATAL_ERROR "unable to find basis_HEADER_DIR")
endif()
#
if(NOT TARGET CONAN_PKG::flextool)
  message(FATAL_ERROR "Use flextool from conan")
endif()
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/HelperFlextool.cmake)
if(NOT flextool OR ${flextool} STREQUAL "")
  message(FATAL_ERROR "flextool not found ${flextool}")
endif()
#
set(cling_includes
  ${CONAN_CLING_CONAN_ROOT}/include
)
message(STATUS "cling_includes=${cling_includes}")
set(clang_includes
  ${CONAN_CLING_CONAN_ROOT}/lib/clang/5.0.0/include
)
message(STATUS "clang_includes=${clang_includes}")
set(corrade_includes
  ${CONAN_CORRADE_ROOT}/include
)
message(STATUS "corrade_includes=${corrade_includes}")
set(entt_includes
  ${CONAN_ENTT_ROOT}/include
)
message(STATUS "entt_includes=${entt_includes}")
#
find_package(flex_support_headers REQUIRED)
if(${flex_support_headers_HEADER_FILE} STREQUAL "")
  message(FATAL_ERROR "unable to find flex_support_headers_HEADER_FILE")
endif()
#
find_package(flexlib REQUIRED)
if(${flexlib_HEADER_DIR} STREQUAL "")
  message(FATAL_ERROR "unable to find flexlib_HEADER_DIR")
endif()
#
set(chromium_base_headers
  ${CONAN_CHROMIUM_BASE_ROOT}/include
)
message(STATUS "chromium_base_headers=${chromium_base_headers}")
set(chromium_build_util_headers
  ${CONAN_CHROMIUM_BUILD_UTIL_ROOT}/include/chromium
)
message(STATUS "chromium_build_util_headers=${chromium_build_util_headers}")
set(flextool_outdir ${CMAKE_CURRENT_BINARY_DIR})
message(STATUS "flextool_outdir=${flextool_outdir}")

set(pimpl_TEMPLATE_CPP
  # usage: --extra-arg=-DTEST_PATH="${TEST_PATH}"
  ${CMAKE_CURRENT_SOURCE_DIR}/codegen/cxtpl/pimpl/pimpl_gen_cpp.cxtpl
)

set(pimpl_TEMPLATE_HPP
  # usage: --extra-arg=-DTEST_PATH="${TEST_PATH}"
  ${CMAKE_CURRENT_SOURCE_DIR}/codegen/cxtpl/pimpl/pimpl_gen_hpp.cxtpl
)

set(pimpl_INSTANCE_TEMPLATE_CPP
  # usage: --extra-arg=-DTEST_PATH="${TEST_PATH}"
  ${CMAKE_CURRENT_SOURCE_DIR}/codegen/cxtpl/pimpl_instance/pimpl_instance_gen_hpp.cxtpl
)

set(pimpl_COMBO_TEMPLATE_CPP
  # usage: --extra-arg=-DTEST_PATH="${TEST_PATH}"
  ${CMAKE_CURRENT_SOURCE_DIR}/codegen/cxtpl/pimpl_combo/pimpl_combo_gen_cpp.cxtpl
)

set(pimpl_COMBO_TEMPLATE_HPP
  # usage: --extra-arg=-DTEST_PATH="${TEST_PATH}"
  ${CMAKE_CURRENT_SOURCE_DIR}/codegen/cxtpl/pimpl_combo/pimpl_combo_gen_hpp.cxtpl
)

set(flex_pimpl_plugin_generated_sources
  ${CMAKE_CURRENT_BINARY_DIR}/ForCodegen.cc.generated.cc
)

# Set GENERATED properties of your generated source file.
# So cmake won't complain about missing source file.
set_source_files_properties(
  ${flex_pimpl_plugin_generated_sources}
  PROPERTIES GENERATED 1)

# Configured file will be loaded by Cling C++ interpreter.
# Loaded by Cling file can change global variables via `extern`,
# for example, it can change global settings map.
# We can use ${flextool_outdir} to change
# output directory for files generated by plugin.
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/flex_pimpl_plugin_settings.cc.in
  ${CMAKE_CURRENT_BINARY_DIR}/flex_pimpl_plugin_settings.cc
)
set(flex_pimpl_plugin_settings
  ${CMAKE_CURRENT_BINARY_DIR}/flex_pimpl_plugin_settings.cc
)

set(flextool_input_files
  ${flex_pimpl_plugin_src_DIR}/ForCodegen.cc
)

# contains generated code that is under version control
# path to flex_pimpl_plugin/pregenerated/IntSummable.pimpl.generated.hpp
set(pregenerated_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
#
find_package(flex_squarets_plugin REQUIRED)
if(${flex_squarets_plugin_FILE} STREQUAL "")
  message(FATAL_ERROR "unable to find flex_squarets_plugin_FILE")
endif()
#
find_package(flex_reflect_plugin REQUIRED)
if(${flex_reflect_plugin_FILE} STREQUAL "")
  message(FATAL_ERROR "unable to find flex_reflect_plugin_FILE")
endif()

message(STATUS "flex_pimpl_plugin_SOURCES=${flex_pimpl_plugin_SOURCES}")
# NOTE: cling does not support static libs, so we use SHARED
# https://github.com/root-project/cling/issues/280
# https://gitlab.kitware.com/cmake/cmake/issues/16473
add_library(${LIB_NAME} ${CORE_LIB_TYPE}
  ${flex_pimpl_plugin_SOURCES}
  ${flex_pimpl_plugin_generated_sources}
)

add_relative_include_dir(${LIB_NAME} PUBLIC PUBLIC "include")

target_include_directories( ${LIB_NAME}-test-includes SYSTEM INTERFACE
  # path to tests/example_datatypes.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

list(APPEND USED_3DPARTY_LIBS
  ${base_LIB}
  ${build_util_LIB}
)

list(APPEND USED_3DPARTY_LIBS
  ${basis_LIB}
  ${flexlib_LIB}
  CONAN_PKG::boost
  CONAN_PKG::openssl
  CONAN_PKG::corrade
  Corrade::PluginManager
  ${USED_BOOST_LIBS}
)

set_property(TARGET ${LIB_NAME} PROPERTY CXX_STANDARD 20)

if(TARGET_EMSCRIPTEN)
  # use PROPERTY CXX_STANDARD 17
else()
  target_compile_features(${LIB_NAME}
    PUBLIC cxx_auto_type
    PRIVATE cxx_variadic_templates)
endif()

set(USED_SYSTEM_LIBS
    Threads::Threads # pthread, https://cmake.org/cmake/help/v3.13/module/FindThreads.html
    ${X11_LIBRARIES} # https://cmake.org/cmake/help/v3.13/module/FindX11.html
    ${CMAKE_DL_LIBS} # https://cmake.org/cmake/help/v3.13/variable/CMAKE_DL_LIBS.html
    stdc++fs # C++17 std::filesystem
    CACHE INTERNAL "USED_SYSTEM_LIBS")

target_link_libraries(${LIB_NAME} PUBLIC
  ${USED_3DPARTY_LIBS}
  ${USED_SYSTEM_LIBS}
)

target_link_libraries(${LIB_NAME} PRIVATE
  CONAN_PKG::entt
)

set(DEBUG_LIBRARY_SUFFIX "")
set_target_properties(${LIB_NAME}
  PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" # TODO: /lib
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" # TODO: /lib
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" # TODO: /bin
    OUTPUT_NAME "${LIB_NAME}$<$<CONFIG:Debug>:${DEBUG_LIBRARY_SUFFIX}>"
    # Plugins don't have any prefix (e.g. 'lib' on Linux)
    PREFIX ""
    ENABLE_EXPORTS 1
    CXX_STANDARD 20
    CXX_EXTENSIONS OFF
    CMAKE_CXX_STANDARD_REQUIRED ON
)

# POSITION_INDEPENDENT_CODE for -fPIC
set_property(TARGET ${LIB_NAME}
  PROPERTY POSITION_INDEPENDENT_CODE ON)

if(ENABLE_CLING)
  find_package(Cling REQUIRED)

  list(APPEND CLING_DEFINITIONS CLING_IS_ON=1)
  target_link_libraries(${LIB_NAME} PUBLIC#PRIVATE
    CONAN_PKG::cling_conan
  )

  get_target_property (cling_conan_IMPORTED_LOCATION CONAN_PKG::cling_conan INTERFACE_INCLUDE_DIRECTORIES)
  message( STATUS "cling_conan=${cling_conan_IMPORTED_LOCATION}" )
  target_include_directories( ${LIB_NAME} PUBLIC ${cling_conan_IMPORTED_LOCATION} )

  if(MSVC)
    set_target_properties(${LIB_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS 1)
    set_property(
      TARGET ${LIB_NAME}
      APPEND_STRING
      PROPERTY LINK_FLAGS
               "/EXPORT:?setValueNoAlloc@internal@runtime@cling@@YAXPEAX00D_K@Z
                /EXPORT:?setValueNoAlloc@internal@runtime@cling@@YAXPEAX00DM@Z
                /EXPORT:cling_runtime_internal_throwIfInvalidPointer")
  endif()

  target_compile_definitions(${LIB_NAME} PUBLIC CLING_IS_ON=1)
endif(ENABLE_CLING)

if(ENABLE_CLANG_FROM_CONAN)
  target_link_libraries( ${LIB_NAME} PUBLIC#PRIVATE
    CONAN_PKG::libclang
    CONAN_PKG::clang_tooling
    CONAN_PKG::clang_tooling_core
    CONAN_PKG::llvm_support
  )
endif(ENABLE_CLANG_FROM_CONAN)

target_compile_options(${LIB_NAME} PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:-Wall>)

target_compile_options(${LIB_NAME} PRIVATE
  -fno-rtti)

target_compile_definitions(${LIB_NAME} PRIVATE
  ${flex_pimpl_plugin_PRIVATE_DEFINES}
  # https://stackoverflow.com/a/30877725
  BOOST_SYSTEM_NO_DEPRECATED BOOST_ERROR_CODE_HEADER_ONLY
  # fixes IDE support and allows
  # to print paths to TEMPLATE files in executable
  pimpl_TEMPLATE_CPP="${pimpl_TEMPLATE_CPP}"
  pimpl_TEMPLATE_HPP="${pimpl_TEMPLATE_HPP}"
  pimpl_INSTANCE_TEMPLATE_CPP="${pimpl_INSTANCE_TEMPLATE_CPP}"
  pimpl_COMBO_TEMPLATE_CPP="${pimpl_COMBO_TEMPLATE_CPP}"
  pimpl_COMBO_TEMPLATE_HPP="${pimpl_COMBO_TEMPLATE_HPP}"
  # see https://github.com/mosra/corrade/blob/af9d4216f07307a2dff471664eed1e50e180568b/modules/UseCorrade.cmake#L568
  CORRADE_DYNAMIC_PLUGIN=1
)

target_compile_definitions(${LIB_NAME} PUBLIC
  ${flex_pimpl_plugin_PUBLIC_DEFINES}
  ${CLING_DEFINITIONS}
)

# TODO: DISABLE_DOCTEST
target_compile_definitions(${LIB_NAME} PUBLIC
  DISABLE_DOCTEST=1
)

# from cmake_helper_utils (conan package)
get_all_compile_definitions(collected_defines
  ${LIB_NAME}
)

# from cmake_helper_utils (conan package)
get_all_include_directories(collected_includes
  ${LIB_NAME}
)

# you can link with library via --extra-arg=-l
# example: --extra-arg=-l${flexlib_file}
message(STATUS "flextool=${flextool}")
set(FULL_CMD
  # NOTE: can be run under gdb
  #gdb
  #  -ex "run"
  #  -ex "set pagination off"
  #  -ex "bt"
  #  -ex "set confirm off"
  #  -ex "quit"
  #  --args
  ${flextool}
    --vmodule=*=200 --enable-logging=stderr --log-level=100
    --indir=${CMAKE_CURRENT_SOURCE_DIR}
    --outdir=${flextool_outdir}
    --load_plugin=${flex_squarets_plugin_FILE}
    --load_plugin=${flex_reflect_plugin_FILE}
    --extra-arg=-I${cling_includes}
    --extra-arg=-I${clang_includes}
    #--extra-arg=-I${corrade_includes}
    #--extra-arg=-I${entt_includes}
    --extra-arg=-DCLING_IS_ON=1
    # custom define for basis
    --extra-arg=-DDISABLE_DOCTEST=1
    --extra-arg=-DDOCTEST_CONFIG_DISABLE=1
    #--extra-arg=-I${CMAKE_CURRENT_SOURCE_DIR}/include
    --extra-arg=-I${chromium_base_headers}
    # TODO: (hack) we want `base/logging.h` from `chromium_base`, not from `chromium_tcmalloc`
    # so we included `chromium_base` manually BEFORE `collected_includes`
    # Bug reproduced only in conan workspace mode (seems `collected_includes` does not respect PRIVATE or PUBLIC).
    --extra-arg=-I${chromium_base_HEADER_DIR}
    #--extra-arg=-I${chromium_build_util_headers}
    #--extra-arg=-I${basis_HEADER_DIR}
    #--extra-arg=-I${flexlib_HEADER_DIR}
    # NOTE: generator expression, expands during build time
    # if the ${ITEM} is non-empty, then append it
    $<$<BOOL:${collected_defines}>:--extra-arg=-D$<JOIN:${collected_defines}, --extra-arg=-D>>
    # NOTE: generator expression, expands during build time
    # if the ${ITEM} is non-empty, then append it
    $<$<BOOL:${collected_includes}>:--extra-arg=-I$<JOIN:${collected_includes}, --extra-arg=-I>>
    --extra-arg=-I${flextool_outdir}
    # path to tests/example_datatypes.hpp
    --extra-arg=-I${CMAKE_CURRENT_SOURCE_DIR}/tests
    --extra-arg=-Wno-undefined-inline
    --extra-arg=-Dpimpl_TEMPLATE_CPP="${pimpl_TEMPLATE_CPP}"
    --extra-arg=-Dpimpl_TEMPLATE_HPP="${pimpl_TEMPLATE_HPP}"
    --extra-arg=-Dpimpl_INSTANCE_TEMPLATE_CPP="${pimpl_INSTANCE_TEMPLATE_CPP}"
    --extra-arg=-Dpimpl_COMBO_TEMPLATE_CPP="${pimpl_COMBO_TEMPLATE_CPP}"
    --extra-arg=-Dpimpl_COMBO_TEMPLATE_HPP="${pimpl_COMBO_TEMPLATE_HPP}"
    ${flextool_extra_args}
    ${flextool_input_files}
    # cling_scripts must be ending argument
    --cling_scripts=${flex_support_headers_HEADER_FILE}
    --cling_scripts=${flex_pimpl_plugin_settings}
)
add_custom_command(
  OUTPUT
    ${flex_pimpl_plugin_generated_sources}
  COMMAND
    ${CMAKE_COMMAND} -E echo " Removing ${flex_pimpl_plugin_generated_sources}."
  COMMAND
    ${CMAKE_COMMAND} -E remove ${flex_pimpl_plugin_generated_sources}
  COMMAND
    ${CMAKE_COMMAND} -E echo " Creating ${flex_pimpl_plugin_generated_sources}."
  COMMAND
    "${CMAKE_COMMAND}"
    -E
    echo
    "executing command: ${FULL_CMD}"
  COMMAND
    "${FULL_CMD}"
  # code generator COMMAND will only be launched
  # if some of DEPENDS files were changed.
  DEPENDS
    ${flextool_input_files}
  # NOTE: uses COMMAND_EXPAND_LISTS
  # to support generator expressions
  # see https://cmake.org/cmake/help/v3.13/command/add_custom_target.html
  COMMAND_EXPAND_LISTS
  COMMENT "(flex_pimpl_plugin) running ${flextool}"
  VERBATIM # to support \t for example
)

# install and export steps
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Exports.cmake)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/conf/${LIB_NAME}.conf")
  # for builds with conan workspace
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/conf/${LIB_NAME}.conf
            ${CMAKE_BINARY_DIR}/${LIB_NAME}.conf
  )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/conf/${LIB_NAME}.conf")
  # for builds without conan
  # allows to run tests without `cmake install` step
  # copies files required to start test executable
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/conf/${LIB_NAME}.conf
            ${CMAKE_CURRENT_BINARY_DIR}/${LIB_NAME}.conf
  )
endif()

if(ENABLE_TESTS)
  add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/tests )
endif()
