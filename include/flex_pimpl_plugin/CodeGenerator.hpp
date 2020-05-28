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

namespace MethodPrinter {

namespace Forwarding {

/// \note change only part after `1 <<`
/// 1 << 0, // 00001 == 1
/// 1 << 1, // 00010 == 2
/// 1 << 2, // 00100 == 4
/// 1 << 3, // 01000 == 8
/// 1 << 4, // 10000 == 16
enum Options
{
  NOTHING = 0
  , EXPLICIT
      = 1 << 1
  , VIRTUAL
      = 1 << 2
  , CONSTEXPR
      = 1 << 3
  , STATIC
      = 1 << 4
  , RETURN_TYPE
      = 1 << 5
  , ALL
      = MethodPrinter::Forwarding::Options::EXPLICIT
        | MethodPrinter::Forwarding::Options::VIRTUAL
        | MethodPrinter::Forwarding::Options::CONSTEXPR
        | MethodPrinter::Forwarding::Options::STATIC
        | MethodPrinter::Forwarding::Options::RETURN_TYPE
};

} // namespace Forwarding

namespace Trailing {

/// \note change only part after `1 <<`
/// 1 << 0, // 00001 == 1
/// 1 << 1, // 00010 == 2
/// 1 << 2, // 00100 == 4
/// 1 << 3, // 01000 == 8
/// 1 << 4, // 10000 == 16
enum Options
{
  NOTHING = 0
  , CONST
      = 1 << 1
  , NOEXCEPT
      = 1 << 2
  , PURE
      = 1 << 3
  , DELETED
      = 1 << 4
  , DEFAULT
      = 1 << 5
  , BODY
      = 1 << 6
  , ALL
      = MethodPrinter::Trailing::Options::CONST
        | MethodPrinter::Trailing::Options::NOEXCEPT
        | MethodPrinter::Trailing::Options::PURE
        | MethodPrinter::Trailing::Options::DELETED
        | MethodPrinter::Trailing::Options::DEFAULT
        | MethodPrinter::Trailing::Options::BODY
};

} // namespace Trailing

} // namespace MethodPrinter

namespace plugin {

extern const char kStructPrefix[];

extern const char kRecordPrefix[];

extern const char kSeparatorWhitespace[];

extern const char kSeparatorCommaAndWhitespace[];

/// \note prints up to return type
/// (without method name, arguments or body)
/// \note order matters:
/// explicit virtual constexpr static returnType
///   methodName(...) {}
/// \note to disallow some options you can pass
/// something like:
/// (MethodPrinter::Options::ALL
///  & ~MethodPrinter::Options::EXPLICIT
///  & ~MethodPrinter::Options::VIRTUAL)
/// \note to allow only some options you can pass
/// something like:
/// MethodPrinter::Options::NOTHING
/// | MethodPrinter::Options::CONST
/// | MethodPrinter::Options::NOEXCEPT);
std::string printMethodForwarding(
  const reflection::MethodInfoPtr& methodInfo
  , const std::string& separator = kSeparatorWhitespace
  // what method printer is allowed to print
  // |options| is a bitmask of |MethodPrinter::Options|
  , int options = MethodPrinter::Forwarding::Options::ALL
);

// input: vector<a, b, c>
// output: "a, b, c"
std::string expandTemplateNames(
  const std::vector<reflection::TemplateParamInfo>& params);

// input: vector<a, b, c>
// output: "a, b, c"
std::string methodParamDecls(
  const std::vector<reflection::MethodParamInfo>& params);

/// \note order matters:
/// methodName(...)
/// const noexcept override final [=0] [=deleted] [=default]
/// {}
/// \note to disallow some options you can pass
/// something like:
/// (MethodPrinter::Options::ALL
///  & ~MethodPrinter::Options::EXPLICIT
///  & ~MethodPrinter::Options::VIRTUAL)
/// \note to allow only some options you can pass
/// something like:
/// MethodPrinter::Options::NOTHING
/// | MethodPrinter::Options::CONST
/// | MethodPrinter::Options::NOEXCEPT);
std::string printMethodTrailing(
  const reflection::MethodInfoPtr& methodInfo
  , const std::string& separator = kSeparatorWhitespace
  // what method printer is allowed to print
  // |options| is a bitmask of |MethodPrinter::Trailing::Options|
  , int options = MethodPrinter::Trailing::Options::ALL
);

std::string forwardMethodParamNames(
  const std::vector<reflection::MethodParamInfo>& params);

// exatracts `SomeType` out of:
// struct SomeType{};
// OR
// class SomeType{};
// Note that `class` in clang LibTooling is `record`
std::string exatractTypeName(
  const std::string& input);

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
