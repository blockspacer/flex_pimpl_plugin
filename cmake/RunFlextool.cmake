message(STATUS "ARGUMENTS=${ARGUMENTS}")
# requires existing compile_commands.json so
# must be run before build, but after configure step
#add_custom_command(OUTPUT ${flextool_input_files}
execute_process(
  COMMAND ${flextool}
          ${ARGUMENTS}
          #TIMEOUT 7200 # sec
  RESULT_VARIABLE retcode
  ERROR_VARIABLE _ERROR_VARIABLE)
if(NOT "${retcode}" STREQUAL "0")
  message(FATAL_ERROR "Bad exit status ${retcode} ${_ERROR_VARIABLE}")
endif()
message(STATUS "flextool output: ${retcode} ${_ERROR_VARIABLE}")
