#include "flex_pimpl_plugin/Tooling.hpp" // IWYU pragma: associated
#include "flex_pimpl_plugin/flex_pimpl_plugin_settings.hpp"

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
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RecordLayout.h>

#include <llvm/Support/raw_ostream.h>

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
#include <base/path_service.h>
#include <base/strings/string_number_conversions.h>

#include <any>
#include <string>
#include <vector>
#include <regex>
#include <iostream>
#include <fstream>
#include <sstream>

namespace plugin {

static const char kSkipPimplAttr[] = "skip_pimpl";

/// \todo refactor similar to https://github.com/jarro2783/cxxopts
/**
  * EXAMPLE INPUT:
      template<typename impl = ::FooImpl>
      class
      _injectPimplStorage()
      {};
  *
  * EXAMPLE OUTPUT (pseudocode):
      ReflectForPimplSettings
      {
        .impl = ClangNode(FooImpl)
      }
  **/
static ReflectForPimplSettings getReflectForPimplSettings(
    const clang_utils::SourceTransformOptions& sourceTransformOptions)
{
  VLOG(9)
    << "parsing pimpl reflection settings...";

  ReflectForPimplSettings result;

  clang::SourceManager &SM
    = sourceTransformOptions.rewriter.getSourceMgr();

  clang::PrintingPolicy printingPolicy(
    sourceTransformOptions.rewriter.getLangOpts());

  const clang::CXXRecordDecl *node =
    sourceTransformOptions.matchResult.Nodes.getNodeAs<
      clang::CXXRecordDecl>("bind_gen");

  CHECK(node->getDescribedClassTemplate())
    << "node "
    << node->getNameAsString()
    << " must be template";

  /// \brief Retrieves the class template that is described by this
  /// class declaration.
  ///
  /// Every class template is represented as a ClassTemplateDecl and a
  /// CXXRecordDecl. The former contains template properties (such as
  /// the template parameter lists) while the latter contains the
  /// actual description of the template's
  /// contents. ClassTemplateDecl::getTemplatedDecl() retrieves the
  /// CXXRecordDecl that from a ClassTemplateDecl, while
  /// getDescribedClassTemplate() retrieves the ClassTemplateDecl from
  /// a CXXRecordDecl.
  clang::TemplateDecl* templateDecl =
    node->getDescribedClassTemplate();
  DCHECK(templateDecl);

  /// \brief Stores a list of template parameters
  /// for a TemplateDecl and its derived classes.
  clang::TemplateParameterList* templateParameters =
    templateDecl->getTemplateParameters();
  DCHECK(templateParameters);

  CHECK(templateParameters->begin() != templateParameters->end())
    << "expected not empty template parameter list: "
    << node->getNameAsString();

  for(clang::NamedDecl *parameter_decl: *templateParameters) {
    CHECK(!parameter_decl->isParameterPack())
      << node->getNameAsString();

    /// \brief Declaration of a template type parameter.
    ///
    /// For example, "T" in
    /// \code
    /// template<typename T> class vector;
    /// \endcode
    if (clang::TemplateTypeParmDecl* template_type
        = clang::dyn_cast<clang::TemplateTypeParmDecl>(parameter_decl))
    {
      CHECK(template_type->wasDeclaredWithTypename())
        << node->getNameAsString();
      CHECK(template_type->hasDefaultArgument())
        << node->getNameAsString();

      if(parameter_decl->getNameAsString() == "impl") {
        result.implArgQualType =
          template_type->getDefaultArgument();

        result.implParameterQualType
          = clang_utils::extractTypeName(
              result.implArgQualType.getAsString(printingPolicy)
            );
        CHECK(!result.implParameterQualType.empty())
          << node->getNameAsString();
      } else if(parameter_decl->getNameAsString() == "interface") {
        result.interfaceArgQualType =
          template_type->getDefaultArgument();

        result.interfaceParameterQualType
          = clang_utils::extractTypeName(
              result.interfaceArgQualType.getAsString(printingPolicy)
            );
        CHECK(!result.interfaceParameterQualType.empty())
          << node->getNameAsString();
      } else {
        CHECK(false)
          << "(pimpl) unknown argument: "
          << parameter_decl->getNameAsString()
          << " with value: "
          << template_type->getDefaultArgument()
               .getAsString(printingPolicy);
      }

    } else {
      CHECK(false)
        << "expected default template parameter: "
        << node->getNameAsString();
    }
  } // for

  VLOG(9)
    << "parsed pimpl reflection settings...";

  return std::move(result);
}

pimplTooling::pimplTooling(
  const ::plugin::ToolPlugin::Events::RegisterAnnotationMethods& event
#if defined(CLING_IS_ON)
  , ::cling_utils::ClingInterpreter* clingInterpreter
#endif // CLING_IS_ON
) : clingInterpreter_(clingInterpreter)
{
  DCHECK(clingInterpreter_)
    << "clingInterpreter_";

  // load settings from C++ script interpreted by Cling
  /// \note skip on fail of settings loading,
  /// fallback to defaults
  {
    cling::Value clingResult;
    /**
     * EXAMPLE Cling script:
       namespace flex_pimpl_plugin {
         // Declaration must match plugin version.
         struct Settings {
           // output directory for generated files
           std::string outDir;
         };
         void loadSettings(Settings& settings)
         {
           settings.outDir
             = "${flextool_outdir}";
         }
       } // namespace flex_pimpl_plugin
     */
    cling::Interpreter::CompilationResult compilationResult
      = clingInterpreter_->callFunctionByName(
          // function name
          "flex_pimpl_plugin::loadSettings"
          // argument as void
          , static_cast<void*>(&settings_)
          // code to cast argument from void
          , "*(flex_pimpl_plugin::Settings*)"
          , clingResult);
    if(compilationResult
        != cling::Interpreter::Interpreter::kSuccess) {
      DCHECK(settings_.outDir.empty());
      DVLOG(9)
        << "failed to execute Cling script, "
           "skipping...";
    } else {
      DVLOG(9)
        << "settings_.outDir: "
        << settings_.outDir;
    }
    DCHECK(clingResult.hasValue()
      // we expect |void| as result of function call
      ? clingResult.isValid() && clingResult.isVoid()
      // skip on fail of settings loading
      : true);
  }

  DETACH_FROM_SEQUENCE(sequence_checker_);

  DCHECK(event.sourceTransformPipeline)
    << "event.sourceTransformPipeline";
  ::clang_utils::SourceTransformPipeline& sourceTransformPipeline
    = *event.sourceTransformPipeline;

  sourceTransformRules_
    = &sourceTransformPipeline.sourceTransformRules;

  if (!base::PathService::Get(base::DIR_EXE, &dir_exe_)) {
    NOTREACHED();
  }
  DCHECK(!dir_exe_.empty());

  outDir_ = dir_exe_.Append("generated");

  if(!settings_.outDir.empty()) {
     outDir_ = base::FilePath{settings_.outDir};
  }

  if(!base::PathExists(outDir_)) {
    base::File::Error dirError = base::File::FILE_OK;
    // Returns 'true' on successful creation,
    // or if the directory already exists
    const bool dirCreated
      = base::CreateDirectoryAndGetError(outDir_, &dirError);
    if (!dirCreated) {
      LOG(ERROR)
        << "failed to create directory: "
        << outDir_
        << " with error code "
        << dirError
        << " with error string "
        << base::File::ErrorToString(dirError);
    }
  }

  {
    // Returns an empty path on error.
    // On POSIX, this function fails if the path does not exist.
    outDir_ = base::MakeAbsoluteFilePath(outDir_);
    DCHECK(!outDir_.empty());
    VLOG(9)
      << "outDir_= "
      << outDir_;
  }
}

pimplTooling::~pimplTooling()
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

reflection::ClassInfoPtr
  pimplTooling::reflectFromCache(
    ReflectForPimplSettings& reflectForPimplSettings
){
  VLOG(9)
    << "trying to get cached reflection data for class: "
    << reflectForPimplSettings.implParameterQualType;

  reflection::ClassInfoPtr reflectedClass;

  auto it = reflectionCache_.find(
    reflectForPimplSettings.implParameterQualType);

  if(it != reflectionCache_.end()) {
    // fallback to cache
    reflectedClass = it->second;
  } else {
    CHECK(false)
      << "not in cache "
      << reflectForPimplSettings.implParameterQualType;
  }

  VLOG(9)
    << "retrieved cached reflection data "
       "from cache for class: "
    << reflectForPimplSettings.implParameterQualType;

  return reflectedClass;
}

reflection::ClassInfoPtr
  pimplTooling::reflectOrGetFromCache(
    reflection::AstReflector& reflector
    , reflection::NamespacesTree namespaces
    , clang::CXXRecordDecl* reflect_target
    , ReflectForPimplSettings& reflectForPimplSettings
){
  VLOG(9)
    << "trying to get reflection data for class: "
    << reflectForPimplSettings.implParameterQualType;

  DCHECK(reflect_target);

  reflection::ClassInfoPtr reflectedClass
    = reflector.ReflectClass(
        reflect_target
        , &namespaces
        , false // recursive
      );
  DCHECK(reflectedClass);

  if(reflectedClass->methods.empty()) {
    auto it = reflectionCache_.find(
      reflectForPimplSettings.implParameterQualType);

    if(it != reflectionCache_.end()) {
      // fallback to cache
      reflectedClass = it->second;
    } else {
      CHECK(false)
        << "not in cache "
        << reflectForPimplSettings.implParameterQualType;
    }

    VLOG(9)
      << "retrieved reflection data "
         "from cache for class: "
      << reflectForPimplSettings.implParameterQualType;
  } else {
    VLOG(9)
      << "retrieved reflection data "
         "for class: "
      << reflectForPimplSettings.implParameterQualType;
  }

  return reflectedClass;
}

/// \todo ability to change generator template
clang_utils::SourceTransformResult
  pimplTooling::injectPimplStorage(
    const clang_utils::SourceTransformOptions& sourceTransformOptions)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(9)
    << "injectPimplStorage called...";

  /// \todo remove code duplication:
  /// move SourceManager, PrintingPolicy, LangOptions
  /// into SourceTransformOptions
  clang::SourceManager &SM
    = sourceTransformOptions.rewriter.getSourceMgr();

  clang::PrintingPolicy printingPolicy(
    sourceTransformOptions.rewriter.getLangOpts());

  const clang::LangOptions& langOptions
    = sourceTransformOptions.rewriter.getLangOpts();

  const clang::CXXRecordDecl *node =
    sourceTransformOptions.matchResult.Nodes.getNodeAs<
      clang::CXXRecordDecl>("bind_gen");

  ReflectForPimplSettings reflectForPimplSettings
    = getReflectForPimplSettings(sourceTransformOptions);

  DCHECK(reflectForPimplSettings.implArgQualType
          ->getAsCXXRecordDecl());
  CHECK(reflectForPimplSettings.implArgQualType
          ->getAsCXXRecordDecl())
    << "must be CXXRecordDecl: "
    << reflectForPimplSettings.implParameterQualType;

  /// \todo support custom namespaces
  reflection::NamespacesTree m_namespaces;

  DCHECK(sourceTransformOptions.matchResult.Context);
  reflection::AstReflector reflector(
    sourceTransformOptions.matchResult.Context);

  DCHECK(reflectForPimplSettings.implArgQualType
          ->getAsCXXRecordDecl());
  reflection::ClassInfoPtr reflectedClass
    = reflectFromCache(
        reflectForPimplSettings);
  DCHECK(reflectedClass);

  CHECK(!reflectedClass->methods.empty())
    << "no methods in "
    << reflectedClass->name;


  int extra_size_bytes = 0;

  /**
   * parse arguments from annotation attribute
   * EXAMPLE:
      template<typename impl = FooImpl>
      class
        _injectPimplStorage(
          "sizePadding = 8"
        )
      PimplStorageInjector
      {};
    *
    * parsed argument name is:
        sizePadding
    * parsed argument value is:
        8
   **/
  {
    /// \todo refactor similar to https://github.com/jarro2783/cxxopts
    /// \note automatically convert argument string to desired type
    flexlib::args annotationArgs =
      sourceTransformOptions.func_with_args.parsed_func_.args_;
    for(const auto& arg : annotationArgs.as_vec_)
    {
      if(arg.name_.empty() && arg.value_.empty()) {
        continue;
      }

      if(arg.name_ == "sizePadding") {
        DCHECK(extra_size_bytes == 0); // 0 is default value
        bool convertedStringToInt
          = base::StringToInt(arg.value_, &extra_size_bytes);
        CHECK(convertedStringToInt)
          << "(pimpl) unable to convert to int argument: "
          << arg.name_
          << " with value: "
          << arg.value_;
      } else {
        CHECK(false)
          << "(pimpl) unknown argument: "
          << arg.name_
          << " with value: "
          << arg.value_;
      }
    }
  }

  std::string replacer;

  /**
   * generates code similar to:
   *  pimpl::FastPimpl<FooImpl, Size, Alignment> impl_;
   **/
  {
    DVLOG(9)
      << "running FastPimpl code generator for: "
      << reflectForPimplSettings.implParameterQualType;

    DCHECK(reflectedClass->decl);
    DCHECK(sourceTransformOptions.matchResult.Context);

    uint64_t typeSize
      = reflectedClass->ASTRecordSize + extra_size_bytes;

    // assume it could be a subclass.
    unsigned fieldAlign
      = reflectedClass->ASTRecordNonVirtualAlignment;

    replacer += "::pimpl::FastPimpl<";

    // usually it is "FooImpl"
    DCHECK(!reflectForPimplSettings.implParameterQualType.empty());
    replacer += reflectForPimplSettings.implParameterQualType;

    // sizeof(Foo::FooImpl)
    replacer += ", /*Size*/";
    replacer += std::to_string(typeSize);

    // alignof(Foo::FooImpl)
    replacer += ", /*Alignment*/";
    replacer += std::to_string(fieldAlign);

    replacer += ", ::pimpl::SizePolicy::AtLeast";

    replacer += ", ::pimpl::AlignPolicy::AtLeast";

    replacer += "> impl_;";
  }

  DVLOG(9)
    << "applying source code transformation for: "
    << reflectForPimplSettings.implParameterQualType;
  // remove annotation from source file
  clang_utils::replaceWith(
    sourceTransformOptions.rewriter
    , sourceTransformOptions.decl
    , sourceTransformOptions.matchResult
    , replacer);

  return clang_utils::SourceTransformResult{nullptr};
}

/// \todo ability to change generator template
clang_utils::SourceTransformResult
  pimplTooling::injectPimplMethodCalls(
    const clang_utils::SourceTransformOptions& sourceTransformOptions)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(9)
    << "injectPimplMethodCalls called...";

  clang::SourceManager &SM
    = sourceTransformOptions.rewriter.getSourceMgr();

  clang::PrintingPolicy printingPolicy(
    sourceTransformOptions.rewriter.getLangOpts());

  const clang::LangOptions& langOptions
    = sourceTransformOptions.rewriter.getLangOpts();

  const clang::CXXRecordDecl *node =
    sourceTransformOptions.matchResult.Nodes.getNodeAs<
      clang::CXXRecordDecl>("bind_gen");

  ReflectForPimplSettings reflectForPimplSettings
    = getReflectForPimplSettings(sourceTransformOptions);

  DCHECK(reflectForPimplSettings.implArgQualType
          ->getAsCXXRecordDecl());
  CHECK(reflectForPimplSettings.implArgQualType
          ->getAsCXXRecordDecl())
    << "must be CXXRecordDecl: "
    << reflectForPimplSettings.implParameterQualType;

  /// \todo support custom namespaces
  reflection::NamespacesTree m_namespaces;

  DCHECK(sourceTransformOptions.matchResult.Context);
  reflection::AstReflector reflector(
    sourceTransformOptions.matchResult.Context);

  DCHECK(reflectForPimplSettings.implArgQualType
          ->getAsCXXRecordDecl());
  reflection::ClassInfoPtr reflectedClass
    = reflectFromCache(
        reflectForPimplSettings);
  DCHECK(reflectedClass);

  CHECK(!reflectedClass->methods.empty())
    << "no methods in "
    << reflectedClass->name;

  bool without_method_body = false;

  /**
   * parse arguments from annotation attribute
   * EXAMPLE:
      template<typename impl = FooImpl>
      class
        my_annotation_attr(
          "without_method_body"
        )
      PimplMethodDeclsInjector
      {};
    *
    * parsed argument value is:
        without_method_body
   **/
  {
    /// \todo refactor similar to https://github.com/jarro2783/cxxopts
    flexlib::args annotationArgs =
      sourceTransformOptions.func_with_args.parsed_func_.args_;
    for(const auto& arg : annotationArgs.as_vec_)
    {
      if(arg.name_.empty() && arg.value_.empty()) {
        continue;
      }

      if(arg.value_ == "without_method_body") {
        DCHECK(!without_method_body);
        without_method_body = true;
      } else {
        CHECK(false)
          << "(pimpl) unknown argument: "
          << arg.name_
          << " with value: "
          << arg.value_;
      }
    }
  }

  std::string replacer;

  /**
   * generates code similar to:
   *  std::string foo(int arg1) { return impl->foo(arg1); };
   **/
  {
    DVLOG(9)
      << "running FastPimpl method call generator for: "
      << reflectForPimplSettings.implParameterQualType;

    for(const reflection::MethodInfoPtr& method
         : reflectedClass->methods)
    {
      DCHECK(method);

      const bool needPrint = isPimplMethod(method);
      if(!needPrint) {
        continue;
      }
      const std::string methodForwarding
         = clang_utils::printMethodForwarding(
             method
             , clang_utils::kSeparatorWhitespace
            // what method printer is allowed to print
             , MethodPrinter::Forwarding::Options::ALL
               & ~MethodPrinter::Forwarding::Options::VIRTUAL);
      const std::string methodTrailing
         = clang_utils::printMethodTrailing(
             method
             , clang_utils::kSeparatorWhitespace
             // what method printer is allowed to print
             , MethodPrinter::Trailing::Options::NOTHING
               | MethodPrinter::Trailing::Options::CONST
               | MethodPrinter::Trailing::Options::NOEXCEPT);
        if(method->isTemplate())
        {
          replacer += "template<";
          replacer += expandTemplateNames(method->tplParams);
          replacer += ">";
        } // method->isTemplate

        replacer += methodForwarding;
        replacer += " ";
        if(!reflectForPimplSettings.interfaceParameterQualType.empty()) {
          // usually it is `::Foo` part out of `::Foo::bar()`
          // where Foo is class that stores implementation
          replacer += reflectForPimplSettings.interfaceParameterQualType;
          replacer += "::";
          VLOG(9)
            << "creating methods of class: "
            << reflectForPimplSettings.interfaceParameterQualType;
        }
        replacer += method->name;
        replacer += "(";
        replacer += methodParamDecls(method->params);
        replacer += ")";
        replacer += " ";
        replacer += methodTrailing;

        if(!without_method_body) {
          replacer += "\n";
          replacer += "{";
          replacer += "\n";
          replacer += " return impl_->";
          replacer += method->name;
          replacer += "(";
          replacer
            += clang_utils::forwardMethodParamNames(
                 method->params);
          replacer += ")";
          replacer += ";";
          replacer += "\n";
          replacer += "}";
          replacer += "\n";
        } else {
          replacer += ";";
          replacer += "\n";
        }
    }
  }

  DVLOG(9)
    << "applying source code transformation for: "
    << reflectForPimplSettings.implParameterQualType;
  // remove annotation from source file
  clang_utils::replaceWith(
    sourceTransformOptions.rewriter
    , sourceTransformOptions.decl
    , sourceTransformOptions.matchResult
    , replacer);

  return clang_utils::SourceTransformResult{nullptr};
}

clang_utils::SourceTransformResult
  pimplTooling::reflectForPimpl(
    const clang_utils::SourceTransformOptions& sourceTransformOptions)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(9)
    << "reflectForPimpl called...";

  clang::SourceManager &SM
    = sourceTransformOptions.rewriter.getSourceMgr();

  clang::PrintingPolicy printingPolicy(
    sourceTransformOptions.rewriter.getLangOpts());

  const clang::LangOptions& langOptions
    = sourceTransformOptions.rewriter.getLangOpts();

  const clang::CXXRecordDecl *node =
    sourceTransformOptions.matchResult.Nodes.getNodeAs<
      clang::CXXRecordDecl>("bind_gen");

  ReflectForPimplSettings reflectForPimplSettings
    = getReflectForPimplSettings(sourceTransformOptions);

  DCHECK(reflectForPimplSettings.implArgQualType
          ->getAsCXXRecordDecl());
  CHECK(reflectForPimplSettings.implArgQualType
          ->getAsCXXRecordDecl())
    << "must be CXXRecordDecl: "
    << reflectForPimplSettings.implParameterQualType;

  /// \todo support custom namespaces
  reflection::NamespacesTree m_namespaces;

  DCHECK(sourceTransformOptions.matchResult.Context);
  reflection::AstReflector reflector(
    sourceTransformOptions.matchResult.Context);

  DCHECK(reflectForPimplSettings.implArgQualType
          ->getAsCXXRecordDecl());
  reflection::ClassInfoPtr reflectedClass
    = reflector.ReflectClass(
        reflectForPimplSettings.implArgQualType
          ->getAsCXXRecordDecl()
        , &m_namespaces
        , false // recursive
      );
  DCHECK(reflectedClass);

  DVLOG(9)
    << "got reflection data for: "
    << reflectForPimplSettings.implParameterQualType;

  CHECK(!reflectedClass->methods.empty())
    << "no methods in "
    << reflectedClass->name;

  std::vector<reflection::MethodInfoPtr> cleaned_methods;
  /// \todo move to separate function
  // remove methods that have special annotation
  for(reflection::MethodInfoPtr& method : reflectedClass->methods)
  {
    DCHECK(method);

    DVLOG(10)
      << "found method: "
      << method->name
      << " from "
      << reflectedClass->name;

    if(!method->decl) {
      DVLOG(10)
        << "skipped method without decl: "
        << method->name
        << " from "
        << reflectedClass->name;
      continue;
    }

    if (clang::AnnotateAttr* annotate
         = method->decl->getAttr<clang::AnnotateAttr>())
    {
      const std::string annotationCode
        = annotate->getAnnotation().str();
      DVLOG(9)
        << "found annotated method: "
        << method->name
        << " from "
        << reflectedClass->name
        << " with annotation: "
        << annotationCode;

      base::StringPiece annotationCodeStringPiece{annotationCode};

      const bool isStarts =
        annotationCodeStringPiece
          .starts_with(kSkipPimplAttr);
      if(isStarts) {
        DVLOG(9)
          << "skipped method: "
          << method->name
          << " from "
          << reflectedClass->name
          << " because it was annotated with: "
          << kSkipPimplAttr;
        continue;
      }
    }

    cleaned_methods.push_back(method);
  }
  reflectedClass->methods = std::move(cleaned_methods);

  if(reflectedClass->methods.empty()) {
    DVLOG(9)
      << "all methods from "
      << reflectedClass->name
      << " were skipped";
  }

  {
    auto it = reflectionCache_.find(
      reflectForPimplSettings.implParameterQualType);

    if(it != reflectionCache_.end()) {
      CHECK(false)
        << "already in cache "
        << reflectedClass->name;
    }

    reflectionCache_[reflectForPimplSettings.implParameterQualType]
      = std::move(reflectedClass);

    VLOG(9)
      << "populated reflection cache with key: "
      << reflectForPimplSettings.implParameterQualType;
  }
  DVLOG(9)
    << "applying source code transformation for: "
    << reflectForPimplSettings.implParameterQualType;
  // remove annotation from source file
  clang_utils::replaceWith(
    sourceTransformOptions.rewriter
    , sourceTransformOptions.decl
    , sourceTransformOptions.matchResult
    , "" // replace with empty string
  );

  return clang_utils::SourceTransformResult{nullptr};
}

} // namespace plugin
