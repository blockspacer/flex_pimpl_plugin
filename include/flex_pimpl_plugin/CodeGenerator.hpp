#pragma once

#include "flex_pimpl_plugin/flex_pimpl_plugin_settings.hpp"

#include <flexlib/clangUtils.hpp>
#include <flexlib/ToolPlugin.hpp>
#if defined(CLING_IS_ON)
#include "flexlib/ClingInterpreterModule.hpp"
#endif // CLING_IS_ON
#include <flexlib/per_plugin_settings.hpp>
#include <flexlib/reflect/ReflTypes.hpp>
#include <flexlib/reflect/ReflectAST.hpp>
#include <flexlib/reflect/ReflectionCache.hpp>
#include <flexlib/ToolPlugin.hpp>
#include <flexlib/core/errors/errors.hpp>
#include <flexlib/utils.hpp>
#include <flexlib/funcParser.hpp>
#include <flexlib/inputThread.hpp>
#include <flexlib/clangUtils.hpp>
#include <flexlib/clangPipeline.hpp>
#include <flexlib/annotation_parser.hpp>
#include <flexlib/annotation_match_handler.hpp>
#include <flexlib/matchers/annotation_matcher.hpp>
#include <flexlib/options/ctp/options.hpp>
#if defined(CLING_IS_ON)
#include "flexlib/ClingInterpreterModule.hpp"
#endif // CLING_IS_ON

#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/ASTContext.h>
#include <clang/Lex/Preprocessor.h>

#include <base/logging.h>
#include <base/sequenced_task_runner.h>

#include <any>
#include <string>
#include <vector>
#include <regex>
#include <iostream>
#include <fstream>

namespace plugin {

// input: vector<a, b, c>
// output: "a, b, c"
std::string expandTemplateNames(
  const std::vector<reflection::TemplateParamInfo>& params);

// input: vector<a, b, c>
// output: "a, b, c"
std::string methodParamDecls(
  const std::vector<reflection::MethodParamInfo>& params);

// used to prohibit Ctor/Dtor/etc. generation in typeclass
// based on provided interface
bool isPimplMethod(
  const reflection::MethodInfoPtr& methodInfo);

/// \note class name must not collide with
/// class names from other loaded plugins
class pimplCodeGenerator {
public:
  pimplCodeGenerator();

  ~pimplCodeGenerator();

private:
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(pimplCodeGenerator);
};

} // namespace plugin
