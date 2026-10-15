# Run at build time, with the parent custom target's terminal attached. Keep stdout/stderr
# inherited so the last checkpoint is visible even if the process never exits.
foreach(required TOOLSDUMP TOOLS_JSON TOOLSDUMP_TIMEOUT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing ${required} for MCP tool list generation")
    endif()
endforeach()

if(NOT TOOLSDUMP_TIMEOUT MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "TOOLSDUMP_TIMEOUT must be a positive integer")
endif()

message(STATUS "mcpbundle: starting ${TOOLSDUMP}")
message(STATUS "mcpbundle: output ${TOOLS_JSON}; timeout ${TOOLSDUMP_TIMEOUT}s")
get_filename_component(toolsdump_working_directory "${TOOLS_JSON}" DIRECTORY)
execute_process(
    COMMAND "${TOOLSDUMP}" "${TOOLS_JSON}"
    WORKING_DIRECTORY "${toolsdump_working_directory}"
    TIMEOUT "${TOOLSDUMP_TIMEOUT}"
    RESULT_VARIABLE toolsdump_result
)

if(NOT "${toolsdump_result}" STREQUAL "0")
    message(FATAL_ERROR
        "MCP tool list generation failed: ${toolsdump_result}. "
        "See the last toolsdump checkpoint above; no 'entered main' checkpoint points to "
        "executable/DLL startup, and 'initializing MainCore' points to runtime service initialization.")
endif()

if(NOT EXISTS "${TOOLS_JSON}")
    message(FATAL_ERROR "MCP tool list generator exited successfully without creating ${TOOLS_JSON}")
endif()
message(STATUS "mcpbundle: tool list generation complete")
