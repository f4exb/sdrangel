if(NOT SOAPYSDR_FOUND)

  function(_SOAPY_SDR_GET_ABI_VERSION VERSION SOAPY_SDR_INCLUDE_DIR)
    file(READ "${SOAPY_SDR_INCLUDE_DIR}/SoapySDR/Version.h" version_h)
    string(REGEX MATCH "\\#define SOAPY_SDR_ABI_VERSION \"([0-9]+\\.[0-9]+(-[A-Za-z0-9]+)?)\"" SOAPY_SDR_ABI_VERSION_MATCHES "${version_h}")
    if(NOT SOAPY_SDR_ABI_VERSION_MATCHES)
      message(FATAL_ERROR "Failed to extract version number from Version.h")
    endif(NOT SOAPY_SDR_ABI_VERSION_MATCHES)
    set(${VERSION} "${CMAKE_MATCH_1}" PARENT_SCOPE)
  endfunction(_SOAPY_SDR_GET_ABI_VERSION)

  pkg_search_module (SOAPYSDR SoapySDR>=0.4.0)

  if(NOT SOAPYSDR_FOUND OR (DEFINED SOAPYSDR_DIR))

    find_path (SOAPYSDR_INCLUDE_DIRS
      NAMES SoapySDR/Version.h
      HINTS ${SOAPYSDR_DIR}/include
      PATHS /usr/include
            /usr/local/include
    )

    find_library (SOAPYSDR_LINK_LIBRARIES
      NAMES SoapySDR
      HINTS ${SOAPYSDR_DIR}/lib
            ${SOAPYSDR_DIR}/lib64
            ${CMAKE_INSTALL_PREFIX}/lib
            ${CMAKE_INSTALL_PREFIX}/lib64
      PATHS /usr/local/lib
            /usr/local/lib64
            /usr/lib
            /usr/lib64
    )

    if (SOAPYSDR_INCLUDE_DIRS AND SOAPYSDR_LINK_LIBRARIES)
      set(SOAPYSDR_FOUND TRUE)
      # get the root of SoapySDR; used on cpack
      string(REGEX REPLACE "/lib/.*${CMAKE_SHARED_LIBRARY_SUFFIX}" "" SOAPYSDR_LIBDIR ${SOAPYSDR_LINK_LIBRARIES})
      # get the soapy version
      _SOAPY_SDR_GET_ABI_VERSION(SOAPYSDR_VERSION ${SOAPYSDR_INCLUDE_DIRS})
    endif()

  endif(NOT SOAPYSDR_FOUND OR (DEFINED SOAPYSDR_DIR))

  if (SOAPYSDR_FOUND)
    message (STATUS "Found SoapySDR: version ${SOAPYSDR_VERSION}, ${SOAPYSDR_LIBDIR}, ${SOAPYSDR_INCLUDE_DIRS}, ${SOAPYSDR_LINK_LIBRARIES}")
  else()
    message (STATUS "SoapySDR not found")
  endif()

  mark_as_advanced(SOAPYSDR_INCLUDE_DIRS SOAPYSDR_LINK_LIBRARIES SOAPYSDR_LIBDIR SOAPYSDR_VERSION)

endif(NOT SOAPYSDR_FOUND)
