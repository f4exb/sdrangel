if(NOT SOAPYSDR_FOUND)

  function(_SOAPY_SDR_GET_ABI_VERSION VERSION SOAPY_SDR_INCLUDE_DIR)
    file(READ "${SOAPY_SDR_INCLUDE_DIR}/SoapySDR/Version.h" version_h)
    string(REGEX MATCH "\\#define SOAPY_SDR_ABI_VERSION \"([0-9]+\\.[0-9]+(-[A-Za-z0-9]+)?)\"" SOAPY_SDR_ABI_VERSION_MATCHES "${version_h}")
    if(NOT SOAPY_SDR_ABI_VERSION_MATCHES)
      message(FATAL_ERROR "Failed to extract version number from Version.h")
    endif(NOT SOAPY_SDR_ABI_VERSION_MATCHES)
    set(${VERSION} "${CMAKE_MATCH_1}" PARENT_SCOPE)
  endfunction(_SOAPY_SDR_GET_ABI_VERSION)

  pkg_search_module (LIBSOAPYSDR_PKG soapysdr>=0.4.0 SoapySDR>=0.4.0)

  if(NOT LIBSOAPYSDR_PKG_FOUND OR (DEFINED SOAPYSDR_DIR))

    find_path (SOAPYSDR_INCLUDE_DIR
      NAMES SoapySDR/Version.h
      HINTS ${SOAPYSDR_DIR}/include
            ${LIBSOAPYSDR_PKG_INCLUDE_DIRS}
      PATHS /usr/include
            /usr/local/include
      )

    find_library (SOAPYSDR_LIBRARY
      NAMES SoapySDR
      HINTS ${SOAPYSDR_DIR}/lib
            ${SOAPYSDR_DIR}/lib64
            ${CMAKE_INSTALL_PREFIX}/lib
            ${CMAKE_INSTALL_PREFIX}/lib64
            ${LIBSOAPYSDR_PKG_LIBRARY_DIRS}
      PATHS /usr/local/lib
            /usr/local/lib64
            /usr/lib
            /usr/lib64
      )

    if (SOAPYSDR_INCLUDE_DIR AND SOAPYSDR_LIBRARY)
      set(SOAPYSDR_FOUND TRUE)
      # get the root of SoapySDR; used on cpack
      string(REGEX REPLACE "/lib/.*${CMAKE_SHARED_LIBRARY_SUFFIX}" "" SOAPYSDR_ROOT ${SOAPYSDR_LIBRARY})
      # get the soapy version; to using FindPkgConfig because we can use SOAPYSDR_DIR
      _SOAPY_SDR_GET_ABI_VERSION(SOAPYSDR_ABI_VERSION ${SOAPYSDR_INCLUDE_DIR})
      message (STATUS "Found SoapySDR: ${SOAPYSDR_INCLUDE_DIR}, ${SOAPYSDR_LIBRARY}")
    else()
      message (STATUS "SoapySDR not found")
    endif()

    mark_as_advanced(SOAPYSDR_INCLUDE_DIR SOAPYSDR_LIBRARY SOAPYSDR_ROOT SOAPYSDR_ABI_VERSION)

  endif(NOT LIBSOAPYSDR_PKG_FOUND OR (DEFINED SOAPYSDR_DIR))

endif(NOT SOAPYSDR_FOUND)
