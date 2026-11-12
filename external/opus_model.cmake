# The weights of Opus's neural networks (Deep PLC, LACE, NoLACE, the bandwidth extension: libopus
# 1.5 and later) are not in its git repository. autogen.sh and autogen.bat fetch them with
# dnn/download_model.sh or .bat, the archive's SHA-256 being written in autogen.sh. This does the
# same without autogen.sh's autoreconf, which a CMake build does not use, and without wget, curl,
# PowerShell or tar on the path, and checks the hash on every platform. It runs in the Opus source
# directory as the external project's patch step:
#
#   cmake -P opus_model.cmake
#
# The weights are compiled into the library (dnn/*_data.c): nothing is installed or loaded at run
# time. Running it again is harmless: a verified archive is not downloaded twice.

file(STRINGS autogen.sh model_line REGEX "download_model\\.sh")
string(REGEX MATCH "download_model\\.sh[ \t]+\"?([0-9a-f]+)" match "${model_line}")
set(hash "${CMAKE_MATCH_1}")
string(LENGTH "${hash}" hash_length)

if (NOT hash_length EQUAL 64)
    message(FATAL_ERROR "Opus: no model hash in autogen.sh (found \"${model_line}\")")
endif ()

set(archive "opus_data-${hash}.tar.gz")
set(url "https://media.xiph.org/opus/models/${archive}")

if (EXISTS "${archive}")
    file(SHA256 "${archive}" existing)

    if (NOT existing STREQUAL hash)
        message(STATUS "Opus: ${archive} does not match its hash, downloading it again")
        file(REMOVE "${archive}")
    endif ()
endif ()

if (NOT EXISTS "${archive}")
    message(STATUS "Opus: downloading the neural network weights ${url}")
    file(DOWNLOAD "${url}" "${archive}" EXPECTED_HASH SHA256=${hash} TLS_VERIFY ON STATUS status)
    list(GET status 0 status_code)

    if (NOT status_code EQUAL 0)
        file(REMOVE "${archive}")
        list(GET status 1 status_text)
        message(FATAL_ERROR "Opus: downloading ${url} failed: ${status_text}")
    endif ()
endif ()

execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xzf "${archive}" RESULT_VARIABLE tar_result)

if (NOT tar_result EQUAL 0)
    message(FATAL_ERROR "Opus: extracting ${archive} failed (${tar_result})")
endif ()

message(STATUS "Opus: neural network weights in place (${archive})")
