#include "flex_pimpl_plugin/CodeGenerator.hpp" // IWYU pragma: associated

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

#include <base/cpu.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/debug/alias.h>
#include <base/debug/stack_trace.h>
#include <base/memory/ptr_util.h>
#include <base/sequenced_task_runner.h>
#include <base/strings/string_util.h>
#include <base/trace_event/trace_event.h>
#include <base/logging.h>
#include <base/files/file_util.h>

#include <any>
#include <string>
#include <vector>
#include <regex>
#include <iostream>
#include <fstream>

namespace plugin {

// extern
const char kStructPrefix[] = "struct ";

// extern
const char kRecordPrefix[] = "record ";

// extern
const char kSeparatorWhitespace[] = " ";

// extern
const char kSeparatorCommaAndWhitespace[] = ", ";

std::string expandTemplateNames(
  const std::vector<reflection::TemplateParamInfo>& params)
{
  std::string out;
  size_t paramIter = 0;
  const size_t methodParamsSize = params.size();
  for(const auto& param: params) {
    out += param.tplDeclName;
    paramIter++;
    if(paramIter != methodParamsSize) {
      out += kSeparatorCommaAndWhitespace;
    } // paramIter != methodParamsSize
  } // params endfor
  return out;
}

std::string methodParamDecls(
  const std::vector<reflection::MethodParamInfo>& params)
{
  std::string out;
  size_t paramIter = 0;
  const size_t methodParamsSize = params.size();
  for(const auto& param: params) {
    out += param.fullDecl;
    paramIter++;
    if(paramIter != methodParamsSize) {
      out += kSeparatorCommaAndWhitespace;
    } // paramIter != methodParamsSize
  } // params endfor
  return out;
}

bool isPimplMethod(
  const reflection::MethodInfoPtr& methodInfo)
{
  // only normal member functions should have
  // wrappers generated for them
  return
    /**
     * isImplicit - Indicates whether the declaration
     * was implicitly generated by the implementation.
     * https://clang.llvm.org/doxygen/classclang_1_1Decl.html
     **/
      !methodInfo->isImplicit
    /**
     * logic to detect |isOperator|:
         bool isOperator
           = decl->isOverloadedOperator()
             || (!decl->hasUserDeclaredCopyAssignment()
               && decl->hasCopyAssignmentWithConstParam())
             || (!decl->hasUserDeclaredMoveConstructor()
               && decl->hasMoveAssignment())
     **/
      && !methodInfo->isOperator
      && !methodInfo->isCtor
      && !methodInfo->isDtor;
}

std::string forwardMethodParamNames(
  const std::vector<reflection::MethodParamInfo>& params)
{
  std::string out;
  size_t paramIter = 0;
  const size_t methodParamsSize = params.size();
  for(const auto& param: params) {
    reflection::TypeInfoPtr pType = param.type;
    if(pType->canBeMoved() || pType->getIsRVReference()) {
      out += "std::move(";
    }
    out += param.name;
    if(pType->canBeMoved() || pType->getIsRVReference()) {
      out += ")";
    }
    paramIter++;
    if(paramIter != methodParamsSize) {
      out += kSeparatorCommaAndWhitespace;
    } // paramIter != methodParamsSize
  } // params endfor
  return out;
}

std::string printMethodForwarding(
  const reflection::MethodInfoPtr& methodInfo
  , const std::string& separator
  // what method printer is allowed to print
  // |options| is a bitmask of |MethodPrinter::Options|
  , int options
){
  DCHECK(methodInfo);

  std::string result;

  const bool allowExplicit
    = (options & MethodPrinter::Forwarding::Options::EXPLICIT);

  const bool allowVirtual
    = (options & MethodPrinter::Forwarding::Options::VIRTUAL);

  const bool allowConstexpr
    = (options & MethodPrinter::Forwarding::Options::CONSTEXPR);

  const bool allowStatic
    = (options & MethodPrinter::Forwarding::Options::STATIC);

  const bool allowReturnType
    = (options & MethodPrinter::Forwarding::Options::RETURN_TYPE);

  if(allowExplicit
     && methodInfo->isExplicitCtor)
  {
    result += "explicit";
    result += separator;
  } else {
    DVLOG(20)
      << (methodInfo->isExplicitCtor
          ? "method is explicit, "
          "but explicit is not printed "
          "due to provided options"
          : "method is not explicit");
  }

  if(allowVirtual
     && methodInfo->isVirtual)
  {
    result += "virtual";
    result += separator;
  } else {
    DVLOG(20)
      << (methodInfo->isVirtual
          ? "method is virtual, "
          "but virtual is not printed "
          "due to provided options"
          : "method is not virtual");
  }

  if(allowConstexpr
     && methodInfo->isConstexpr)
  {
    result += "constexpr";
    result += separator;
  } else {
    DVLOG(20)
      << (methodInfo->isConstexpr
          ? "method is constexpr, "
          "but constexpr is not printed "
          "due to provided options"
          : "method is not constexpr");
  }

  if(allowStatic
     && methodInfo->isStatic)
  {
    result += "static";
    result += separator;
  } else {
    DVLOG(20)
      << (methodInfo->isStatic
          ? "method is static, "
          "but static is not printed "
          "due to provided options"
          : "method is not static");
  }

  DCHECK(allowReturnType ? !methodInfo->isCtor : true)
    << "constructor can not have return type, method: "
    << methodInfo->name;

  DCHECK(allowReturnType ? !methodInfo->isDtor : true)
    << "destructor can not have return type, method: "
    << methodInfo->name;

  if(allowReturnType
     && methodInfo->returnType)
  {
    DCHECK(!methodInfo->returnType->getPrintedName().empty());
    result += methodInfo->returnType->getPrintedName();
    result += separator;
  } else {
    DVLOG(20)
      << (methodInfo->returnType
          ? "method has return type, "
          "but return type is not printed "
          "due to provided options"
          : "method without return type");
  }

  if(result.empty()) {
    VLOG(9)
      << "printMethodForwarding returned nothing for method: "
      << methodInfo->name;
  }

  return result;
}

std::string printMethodTrailing(
  const reflection::MethodInfoPtr& methodInfo
  , const std::string& separator
  // what method printer is allowed to print
  // |options| is a bitmask of |MethodPrinter::Trailing::Options|
  , int options
){
  DCHECK(methodInfo);

  std::string result;

  const bool allowConst
    = (options & MethodPrinter::Trailing::Options::CONST);

  const bool allowNoexcept
    = (options & MethodPrinter::Trailing::Options::NOEXCEPT);

  const bool allowPure
    = (options & MethodPrinter::Trailing::Options::PURE);

  const bool allowDeleted
    = (options & MethodPrinter::Trailing::Options::DELETED);

  const bool allowDefault
    = (options & MethodPrinter::Trailing::Options::DEFAULT);

  const bool allowBody
    = (options & MethodPrinter::Trailing::Options::BODY);

  if(allowConst
     && methodInfo->isConst)
  {
    result += "const";
    result += separator;
  } else {
    DVLOG(20)
      << (methodInfo->isConst
          ? "method is const, "
          "but const is not printed "
          "due to provided options"
          : "method is not const");
  }

  if(allowNoexcept
     && methodInfo->isNoExcept)
  {
    result += "noexcept";
    result += separator;
  } else {
    DVLOG(20)
      << (methodInfo->isNoExcept
          ? "method is noexcept, "
          "but noexcept is not printed "
          "due to provided options"
          : "method is not noexcept");
  }

  if(allowPure
     && methodInfo->isPure)
  {
    result += "= 0";
    result += separator;
  } else {
    DVLOG(20)
      << (methodInfo->isPure
          ? "method is pure (= 0), "
          "but (= 0) is not printed "
          "due to provided options"
          : "method is not pure (= 0)");
  }

  if(allowDeleted
     && methodInfo->isDeleted)
  {
    result += "= delete";
    result += separator;
  } else {
    DVLOG(20)
      << (methodInfo->isDeleted
          ? "method is deleted (= delete), "
          "but (= delete) is not printed "
          "due to provided options"
          : "method is not deleted (= delete)");
  }

  if(allowDefault
     && methodInfo->isDefault)
  {
    result += "= default";
    result += separator;
  } else {
    DVLOG(20)
      << (methodInfo->isDefault
          ? "method is default (= default), "
          "but (= default) is not printed "
          "due to provided options"
          : "method is not default (= default)");
  }

  const bool canHaveBody =
     methodInfo->isDefined
     && methodInfo->isClassScopeInlined;

  if(allowBody
     && canHaveBody)
  {
    DCHECK(!methodInfo->body.empty());
    result += methodInfo->body;
    DVLOG(20)
      << "created body for method"
      << methodInfo->name;
  }
  else if(allowBody)
  {
    // no body example: methodType methodName(methodArgs);
    result += ";";
    DVLOG(20)
      << "created empty body for method"
      << methodInfo->name;
  } else {
    DVLOG(20)
      << (canHaveBody
          ? "method can have body, "
          "but body is not printed "
          "due to provided options"
          : "method can't have body");
  }

  if(result.empty()) {
    VLOG(9)
      << "printMethodTrailing returned nothing for method: "
      << methodInfo->name;
  }

  return result;
}

// Before:
// struct only_for_code_generation::Spell SpellTraits
// After:
// only_for_code_generation::Spell SpellTraits
std::string exatractTypeName(
  const std::string& input)
{
  {
    DCHECK(base::size(kStructPrefix));
    if(base::StartsWith(input, kStructPrefix
         , base::CompareCase::INSENSITIVE_ASCII))
    {
      return input.substr(base::size(kStructPrefix) - 1
                     , std::string::npos);
    }
  }

  {
    DCHECK(base::size(kRecordPrefix));
    const std::string prefix = "record ";
    if(base::StartsWith(input, kRecordPrefix
         , base::CompareCase::INSENSITIVE_ASCII))
    {
      return input.substr(base::size(kRecordPrefix) - 1
                     , std::string::npos);
    }
  }

  return input;
}

pimplCodeGenerator::pimplCodeGenerator()
{
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

pimplCodeGenerator::~pimplCodeGenerator()
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

} // namespace plugin
