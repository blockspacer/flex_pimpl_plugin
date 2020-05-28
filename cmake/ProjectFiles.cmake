include_guard( DIRECTORY )

list(APPEND flex_pimpl_plugin_SOURCES
  ${flex_pimpl_plugin_src_DIR}/plugin_main.cc
  ${flex_pimpl_plugin_include_DIR}/EventHandler.hpp
  ${flex_pimpl_plugin_src_DIR}/EventHandler.cc
  ${flex_pimpl_plugin_include_DIR}/Tooling.hpp
  ${flex_pimpl_plugin_include_DIR}/CodeGenerator.hpp
  #generated
  #${flex_pimpl_plugin_src_DIR}/Tooling.cc
  #generated
  #${flex_pimpl_plugin_src_DIR}/CodeGenerator.cc
)
