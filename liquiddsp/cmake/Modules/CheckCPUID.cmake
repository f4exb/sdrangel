# example usage: CHECK_CPUID("mmx" HAVE_MMX [TRUE|FALSE])
macro(CHECK_CPUID CHARACTERISTIC VARIABLE)
    if (${ARGC} GREATER 2)
        set(_CPUID_FORCE ${ARGV2})
    else ()
        unset(_CPUID_FORCE)
    endif ()

    message("-- Checking for CPU characteristic ${CHARACTERISTIC}")

    if (DEFINED _CPUID_FORCE AND _CPUID_FORCE)
        set(_CPUID_CHARACTERISTIC_FOUND ${_CPUID_FORCE})
        set(_FORCED "(forced)")
    else ()
        if (CMAKE_CROSSCOMPILING)
            # When cross compiling, we need to test each characteristic.
            try_compile(CPUID_COMPILE_RESULT
                        ${CMAKE_CURRENT_BINARY_DIR}
                        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/cmcpuid.c
                        COMPILE_DEFINITIONS "-DCROSS_COMPILING" "-DTEST_${CHARACTERISTIC}"
                        OUTPUT_VARIABLE CPUID_COMPILE_OUTPUT
                        )
            set(_CPUID_CHARACTERISTIC_FOUND ${CPUID_COMPILE_RESULT})
        else ()
            if (NOT _CPUID_CHARACTERISTICS)
                try_run(CPUID_RUN_RESULT CPUID_COMPILE_RESULT
                        ${CMAKE_CURRENT_BINARY_DIR}
                        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/cmcpuid.c
                        RUN_OUTPUT_VARIABLE CPUID_CHARACTERISTICS
                        )
                set(_CPUID_CHARACTERISTICS "${CPUID_CHARACTERISTICS}" CACHE INTERNAL "CPU Characteristics")
            endif ()
            if (${_CPUID_CHARACTERISTICS} MATCHES "@${CHARACTERISTIC}@")
                set(_CPUID_CHARACTERISTIC_FOUND TRUE)
            else ()
                set(_CPUID_CHARACTERISTIC_FOUND FALSE)
            endif ()
        endif ()
        unset(_FORCED)
    endif ()

    if (_CPUID_CHARACTERISTIC_FOUND)
        message("-- Checking for CPU characteristic ${CHARACTERISTIC} - found ${_FORCED}")
    else ()
        message("-- Checking for CPU characteristic ${CHARACTERISTIC} - not found")
    endif ()
    set(${VARIABLE} ${_CPUID_CHARACTERISTIC_FOUND} CACHE INTERNAL "Check for CPU characteristic ${CHARACTERISTIC}" FORCE)
endmacro()

