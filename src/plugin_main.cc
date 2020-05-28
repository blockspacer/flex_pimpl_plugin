#include <flex_pimpl_plugin/EventHandler.hpp>

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

#include <base/logging.h>
#include <base/cpu.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/debug/alias.h>
#include <base/debug/stack_trace.h>
#include <base/memory/ptr_util.h>
#include <base/sequenced_task_runner.h>
#include <base/strings/string_util.h>
#include <base/trace_event/trace_event.h>

#if !defined(CORRADE_DYNAMIC_PLUGIN)
#error "plugin must be shared library with CORRADE_DYNAMIC_PLUGIN=1"
#endif  // CORRADE_DYNAMIC_PLUGIN

namespace plugin {

/// \note class name must not collide with
/// class names from other loaded plugins
class Flexpimpl
  final
  : public ::plugin::ToolPlugin {
 public:
  explicit Flexpimpl(
    ::plugin::AbstractManager& manager
    , const std::string& plugin)
    : ::plugin::ToolPlugin{manager, plugin}
  {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  std::string title() const override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return metadata()->data().value("title");
  }

  std::string author() const override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return metadata()->data().value("author");
  }

  std::string description() const override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return metadata()->data().value("description");
  }

  bool load() override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::Flexpimpl::load()");

    DLOG(INFO)
      << "loaded plugin with title = "
      << title()
      << " and description = "
      << description().substr(0, 100)
      << "...";

    return true;
  }

  void disconnect_dispatcher(
    entt::dispatcher &event_dispatcher) override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::Flexpimpl::disconnect_dispatcher()");

    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::Init>()
        .disconnect<
          &FlexpimplEventHandler::Init>(&eventHandler_);

    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::StringCommand>()
        .disconnect<
          &FlexpimplEventHandler::StringCommand>(&eventHandler_);

    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::RegisterAnnotationMethods>()
        .disconnect<
          &FlexpimplEventHandler::RegisterAnnotationMethods>(&eventHandler_);

#if defined(CLING_IS_ON)
    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::RegisterClingInterpreter>()
        .disconnect<
          &FlexpimplEventHandler::RegisterClingInterpreter>(&eventHandler_);
#endif // CLING_IS_ON
  }

  void connect_to_dispatcher(
    entt::dispatcher &event_dispatcher) override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::Flexpimpl::connect_to_dispatcher()");

    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::Init>()
        .connect<
          &FlexpimplEventHandler::Init>(&eventHandler_);

    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::StringCommand>()
        .connect<
          &FlexpimplEventHandler::StringCommand>(&eventHandler_);

    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::RegisterAnnotationMethods>()
        .connect<
          &FlexpimplEventHandler::RegisterAnnotationMethods>(&eventHandler_);

#if defined(CLING_IS_ON)
    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::RegisterClingInterpreter>()
        .connect<
          &FlexpimplEventHandler::RegisterClingInterpreter>(&eventHandler_);
#endif // CLING_IS_ON
  }

  bool unload() override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::Flexpimpl::unload()");

    DLOG(INFO)
      << "unloaded plugin with title = "
      << title()
      << " and description = "
      << description().substr(0, 100)
      << "...";

    return true;
  }

private:
  FlexpimplEventHandler eventHandler_{};

  DISALLOW_COPY_AND_ASSIGN(Flexpimpl);
};

} // namespace plugin

REGISTER_PLUGIN(/*name*/ Flexpimpl
    , /*className*/ plugin::Flexpimpl
    // plugin interface version checks to avoid unexpected behavior
    , /*interface*/ "backend.ToolPlugin/1.0")
