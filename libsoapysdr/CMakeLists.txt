project(soapysdr)

########################################################################
# gather version information
# packagers may specify -DSOAPY_SDR_EXTVER="foo" to replace the git hash
########################################################################
file(READ "${LIBSOAPYSDRSRC}/Changelog.txt" changelog_txt)
string(REGEX MATCH "Release ([0-9]+\\.[0-9]+\\.[0-9]+) \\(" CHANGELOG_MATCH "${changelog_txt}")
if(NOT CHANGELOG_MATCH)
    message(FATAL_ERROR "Failed to extract version number from Changelog.txt")
endif(NOT CHANGELOG_MATCH)
set(SOAPY_SDR_LIBVER "${CMAKE_MATCH_1}")

if (NOT SOAPY_SDR_EXTVER)
    include(${LIBSOAPYSDRSRC}/cmake/Modules/GetGitRevisionDescription.cmake)
    get_git_head_revision(GITREFSPEC GITHASH)
    if (GITHASH)
        string(SUBSTRING "${GITHASH}" 0 8 GITHASH)
        set(SOAPY_SDR_EXTVER "g${GITHASH}")
    else (GITHASH)
        set(SOAPY_SDR_EXTVER "unknown")
    endif (GITHASH)
endif()

set(SOAPY_SDR_VERSION "${SOAPY_SDR_LIBVER}-${SOAPY_SDR_EXTVER}")

#SOAPY_SDR_ROOT is compiled into the library to locate the install base.
#By default, the SOAPY_SDR_ROOT is set to the CMAKE_INSTALL_PREFIX.
#However users may overload this by specifying -DSOAPY_SDR_ROOT=<path>.
set(SOAPY_SDR_ROOT "${CMAKE_INSTALL_PREFIX}" CACHE PATH
    "Installation root for SoapySDR::getRootPath()")
file(TO_CMAKE_PATH "${SOAPY_SDR_ROOT}" SOAPY_SDR_ROOT)

#SOAPY_SDR_ROOT_ENV is the name of the environment variable
#which tells SoapySDR where to find the root installation.
#By default, the environment variable SOAPY_SDR_ROOT is used.
#Example: set -DSOAPY_SDR_ROOT_ENV=SNAP for snappy packages.
set(SOAPY_SDR_ROOT_ENV "SOAPY_SDR_ROOT" CACHE STRING
    "Environment variable for SoapySDR::getRootPath()")

########################################################################
# select the release build type by default to get optimization flags
########################################################################
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

########################################################################
# rpath setup - http://www.cmake.org/Wiki/CMake_RPATH_handling
########################################################################
# use, i.e. don't skip the full RPATH for the build tree
option(CMAKE_SKIP_BUILD_RPATH "skip rpath build" FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
option(CMAKE_BUILD_WITH_INSTALL_RPATH "build with install rpath" FALSE)

# the RPATH to be used when installing, but only if it's not a system directory
option(CMAKE_AUTOSET_INSTALL_RPATH TRUE)
if(CMAKE_AUTOSET_INSTALL_RPATH)
LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}" isSystemDir)
IF("${isSystemDir}" STREQUAL "-1")
    SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}")
ENDIF("${isSystemDir}" STREQUAL "-1")
endif(CMAKE_AUTOSET_INSTALL_RPATH)

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
option(CMAKE_INSTALL_RPATH_USE_LINK_PATH "build with automatic rpath" TRUE)

########################################################################

file(GLOB soapysdr_SOURCES
	${LIBSOAPYSDRSRC}/lib/*.cpp
)

file(GLOB soapysdr_HEADERS
	${LIBSOAPYSDRSRC}/include/SoapySDR/*.hpp
	${LIBSOAPYSDRSRC}/include/SoapySDR/*.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBSOAPYSDRSRC}/include
    ${LIBSOAPYSDRSRC}/lib
)

add_definitions(-DQT_SHARED)

message( STATUS "soapysdr_SOURCES: ${soapysdr_SOURCES}" )

add_library(soapysdr SHARED
    ${soapysdr_SOURCES}
)

install(TARGETS soapysdr DESTINATION lib)
