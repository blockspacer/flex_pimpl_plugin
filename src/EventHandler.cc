#include <flex_pimpl_plugin/EventHandler.hpp> // IWYU pragma: associated

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

#include <base/cpu.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/debug/alias.h>
#include <base/debug/stack_trace.h>
#include <base/memory/ptr_util.h>
#include <base/sequenced_task_runner.h>
#include <base/strings/string_util.h>
#include <base/trace_event/trace_event.h>

namespace plugin {

namespace {

static const std::string kPluginDebugLogName = "(Flexpimpl plugin)";

static const std::string kVersion = "v0.0.1";

static const std::string kVersionCommand = "/version";

#if !defined(APPLICATION_BUILD_TYPE)
#define APPLICATION_BUILD_TYPE "local build"
#endif

} // namespace

FlexpimplEventHandler::FlexpimplEventHandler()
{
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FlexpimplEventHandler::~FlexpimplEventHandler()
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FlexpimplEventHandler::StringCommand(
  const ::plugin::ToolPlugin::Events::StringCommand& event)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("toplevel",
               "plugin::FlexpimplEventHandler::handle_event(StringCommand)");

  if(event.split_parts.size() == 1)
  {
    if(event.split_parts[0] == kVersionCommand) {
      LOG(INFO)
        << kPluginDebugLogName
        << " application version: "
        << kVersion;
      LOG(INFO)
        << kPluginDebugLogName
        << " application build type: "
        << APPLICATION_BUILD_TYPE;
    }
  }
}

void FlexpimplEventHandler::RegisterAnnotationMethods(
  const ::plugin::ToolPlugin::Events::RegisterAnnotationMethods& event)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("toplevel",
               "plugin::FlexpimplEventHandler::handle_event(RegisterAnnotationMethods)");

#if defined(CLING_IS_ON)
  DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

  tooling_ = std::make_unique<pimplTooling>(
    event
#if defined(CLING_IS_ON)
    , clingInterpreter_
#endif // CLING_IS_ON
  );

  DCHECK(event.sourceTransformPipeline);
  ::clang_utils::SourceTransformPipeline& sourceTransformPipeline
    = *event.sourceTransformPipeline;

  ::clang_utils::SourceTransformRules& sourceTransformRules
    = sourceTransformPipeline.sourceTransformRules;

  {
    VLOG(9)
      << "registered source transform rule:"
         " inject_pimpl_storage";
    CHECK(tooling_);
    sourceTransformRules["inject_pimpl_storage"] =
      base::BindRepeating(
        &pimplTooling::injectPimplStorage
        , base::Unretained(tooling_.get()));
  }

  {
    VLOG(9)
      << "registered source transform rule:"
         " inject_pimpl_method_calls";
    CHECK(tooling_);
    sourceTransformRules["inject_pimpl_method_calls"] =
      base::BindRepeating(
        &pimplTooling::injectPimplMethodCalls
        , base::Unretained(tooling_.get()));
  }

  {
    VLOG(9)
      << "registered source transform rule:"
         " reflect_for_pimpl";
    CHECK(tooling_);
    sourceTransformRules["reflect_for_pimpl"] =
      base::BindRepeating(
        &pimplTooling::reflectForPimpl
        , base::Unretained(tooling_.get()));
  }
}

void FlexpimplEventHandler::Init(
  const ::plugin::ToolPlugin::Events::Init& event)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("toplevel",
               "plugin::FlexpimplEventHandler::handle_event(Init)");
  DVLOG(9)
    << "event.argc: "
    << event.argc;
}

#if defined(CLING_IS_ON)
void FlexpimplEventHandler::RegisterClingInterpreter(
  const ::plugin::ToolPlugin::Events::RegisterClingInterpreter& event)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("toplevel",
               "plugin::FlexpimplEventHandler::handle_event(RegisterClingInterpreter)");

  DCHECK(event.clingInterpreter);
  clingInterpreter_ = event.clingInterpreter;
}
#endif // CLING_IS_ON

} // namespace plugin
