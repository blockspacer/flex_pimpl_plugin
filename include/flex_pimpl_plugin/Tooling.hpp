#pragma once

#include "flex_pimpl_plugin/CodeGenerator.hpp"

#include <flexlib/reflect/ReflectAST.hpp>
#include <flexlib/reflect/ReflTypes.hpp>
#include <flexlib/reflect/TypeInfo.hpp>
#include <flexlib/clangUtils.hpp>
#include <flexlib/ToolPlugin.hpp>
#if defined(CLING_IS_ON)
#include "flexlib/ClingInterpreterModule.hpp"
#endif // CLING_IS_ON

#include <base/logging.h>
#include <base/sequenced_task_runner.h>
#include <base/files/file_path.h>

namespace flex_pimpl_plugin {

// Declaration must match plugin version.
struct Settings {
  // output directory for generated files
  std::string outDir;
};

} // namespace flex_pimpl_plugin

namespace plugin {

// interface - class that stores implementation
// impl - implementation (PImpl pattern)
struct ReflectForPimplSettings {
  // example: namespace::IntSummable
  std::string implParameterQualType;

  clang::QualType implArgQualType;

  // example: namespace::IntSummable
  std::string interfaceParameterQualType;

  clang::QualType interfaceArgQualType;
};

/// \note class name must not collide with
/// class names from other loaded plugins
class pimplTooling {
public:
  pimplTooling(
    const ::plugin::ToolPlugin::Events::RegisterAnnotationMethods& event
#if defined(CLING_IS_ON)
    , ::cling_utils::ClingInterpreter* clingInterpreter
#endif // CLING_IS_ON
  );

  ~pimplTooling();

  clang_utils::SourceTransformResult
    injectPimplStorage(
      const clang_utils::SourceTransformOptions& sourceTransformOptions);

  clang_utils::SourceTransformResult
    injectPimplMethodCalls(
      const clang_utils::SourceTransformOptions& sourceTransformOptions);

  clang_utils::SourceTransformResult
    reflectForPimpl(
      const clang_utils::SourceTransformOptions& sourceTransformOptions);

private:
  reflection::ClassInfoPtr
    reflectOrGetFromCache(
      reflection::AstReflector& reflector
      , reflection::NamespacesTree namespaces
      , clang::CXXRecordDecl* reflect_target
      , ReflectForPimplSettings& reflectForPimplSettings);

  reflection::ClassInfoPtr
    reflectFromCache(
      ReflectForPimplSettings& reflectForPimplSettings);

private:
  ::clang_utils::SourceTransformRules* sourceTransformRules_;

#if defined(CLING_IS_ON)
  ::cling_utils::ClingInterpreter* clingInterpreter_;
#endif // CLING_IS_ON

  SEQUENCE_CHECKER(sequence_checker_);

  // output directory for generated files
  base::FilePath outDir_;

  base::FilePath dir_exe_;

  pimplCodeGenerator pimplCodeGenerator_{};

  flex_pimpl_plugin::Settings settings_{};

  std::map<
    std::string
    , reflection::ClassInfoPtr
  > reflectionCache_{};

  DISALLOW_COPY_AND_ASSIGN(pimplTooling);
};

} // namespace plugin
