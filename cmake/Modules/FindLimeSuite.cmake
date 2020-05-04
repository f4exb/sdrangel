# Find Lime Suite
if (NOT LIMESUITE_FOUND)
    if (NOT LIMESUITE_INCLUDE_DIR)
        find_path (LIMESUITE_INCLUDE_DIR
            NAMES lime/LimeSuite.h
            HINTS ${LIMESUITE_DIR}/include
            PATHS /usr/include
                  /usr/local/include
        )
    endif()

    if (NOT LIMESUITE_LIBRARY)
        find_library (LIMESUITE_LIBRARY
            NAMES LimeSuite
            HINTS ${LIMESUITE_DIR}/lib
                  ${CMAKE_INSTALL_PREFIX}/lib
                  ${CMAKE_INSTALL_PREFIX}/lib64
            PATHS /usr/local/lib
                  /usr/local/lib64
                  /usr/lib
                  /usr/lib64
        )
    endif()

    if (LIMESUITE_INCLUDE_DIR AND LIMESUITE_LIBRARY)
        set(LIMESUITE_FOUND TRUE CACHE INTERNAL "")
        message (STATUS "Found Lime Suite: ${LIMESUITE_INCLUDE_DIR}, ${LIMESUITE_LIBRARY}")
    else (LIMESUITE_INCLUDE_DIR AND LIMESUITE_LIBRARY)
        set(LIMESUITE_FOUND FALSE CACHE INTERNAL "")
        message (STATUS "Could not find Lime Suite")
    endif (LIMESUITE_INCLUDE_DIR AND LIMESUITE_LIBRARY)

    mark_as_advanced(LIMESUITE_INCLUDE_DIR LIMESUITE_LIBRARY)
endif (NOT LIMESUITE_FOUND)