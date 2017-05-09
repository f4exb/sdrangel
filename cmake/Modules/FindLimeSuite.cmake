# Find Lime Suite

if (NOT LIMESUITE_INCLUDE_DIR)
    FIND_PATH (LIMESUITE_INCLUDE_DIR
        NAMES lime/LimeSuite.h
        PATHS
        /usr/include
        /usr/local/include
    )
endif()

if (NOT LIMESUITE_LIBRARY)
    FIND_LIBRARY (LIMESUITE_LIBRARY
        NAMES LimeSuite
        HINTS ${CMAKE_INSTALL_PREFIX}/lib
              ${CMAKE_INSTALL_PREFIX}/lib64
        PATHS /usr/local/lib
              /usr/local/lib64
              /usr/lib
              /usr/lib64
    )
endif()

IF (LIMESUITE_INCLUDE_DIR AND LIMESUITE_LIBRARY)
    SET(LIMESUITE_FOUND TRUE)
ENDIF (LIMESUITE_INCLUDE_DIR AND LIMESUITE_LIBRARY)

IF (LIMESUITE_FOUND)
    MESSAGE (STATUS "Found Lime Suite: ${LIMESUITE_INCLUDE_DIR}, ${LIMESUITE_LIBRARY}")
ELSE (LIMESUITE_FOUND)
    MESSAGE (STATUS "Could not find Lime Suite")
ENDIF (LIMESUITE_FOUND)

mark_as_advanced(LIMESUITE_INCLUDE_DIR LIMESUITE_LIBRARY)
