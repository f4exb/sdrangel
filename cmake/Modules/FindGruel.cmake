INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_GRUEL gruel)

FIND_PATH(
    GRUEL_INCLUDE_DIRS
    NAMES gruel/attributes.h
    HINTS $ENV{GRUEL_DIR}/include
        ${PC_GRUEL_INCLUDEDIR}
        ${CMAKE_INSTALL_PREFIX}/include
    PATHS /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    GRUEL_LIBRARIES
    NAMES gruel
    HINTS $ENV{GRUEL_DIR}/lib
        ${PC_GRUEL_LIBDIR}
        ${CMAKE_INSTALL_PREFIX}/lib
        ${CMAKE_INSTALL_PREFIX}/lib64
    PATHS /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GRUEL DEFAULT_MSG GRUEL_LIBRARIES GRUEL_INCLUDE_DIRS)
MARK_AS_ADVANCED(GRUEL_LIBRARIES GRUEL_INCLUDE_DIRS)
