# Stuff qmake can tell us
get_target_property (QMAKE_EXECUTABLE Qt5::qmake LOCATION)
function (QUERY_QMAKE VAR RESULT)
  exec_program (${QMAKE_EXECUTABLE} ARGS "-query ${VAR}" RETURN_VALUE return_code OUTPUT_VARIABLE output)
  if (NOT return_code)
    file (TO_CMAKE_PATH "${output}" output)
    set (${RESULT} ${output} PARENT_SCOPE)
  endif (NOT return_code)
  message (STATUS "Asking qmake for ${RESULT} and got ${output}")
endfunction (QUERY_QMAKE)

query_qmake (QT_INSTALL_PLUGINS QT_PLUGINS_DIR)
query_qmake (QT_INSTALL_IMPORTS QT_IMPORTS_DIR)

if(APPLE AND BUNDLE AND BUILD_GUI)

  find_program (SW_VERS_PROGRAM sw_vers)
  if (SW_VERS_PROGRAM)
    execute_process (
      COMMAND ${SW_VERS_PROGRAM} -productVersion
      OUTPUT_VARIABLE CPACK_MACOS_PACKAGE_ARCHITECTURE
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
  else (SW_VERS_PROGRAM)
    set (CPACK_MACOS_PACKAGE_ARCHITECTURE noarch)
  endif (SW_VERS_PROGRAM)

  set(CPACK_GENERATOR "Bundle")
  set(CPACK_BINARY_DRAGNDROP ON)
  set(CPACK_BUNDLE_NAME "${APPLICATION_NAME}")
  set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/cmake/cpack/sdrangel_icon.icns")
  set(CPACK_BUNDLE_PLIST "${CMAKE_BINARY_DIR}/Info.plist")
  set(CPACK_BUNDLE_STARTUP_COMMAND "${CMAKE_SOURCE_DIR}/cmake/cpack/sdrangel.sh")
  set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/cmake/cpack/sdrangel_icon.icns")
  set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}_${CPACK_MACOS_PACKAGE_ARCHITECTURE}_${CMAKE_SYSTEM_PROCESSOR}")

  # Copy Qt Plugins; fixup_bundle doesn't do that
  install (
    DIRECTORY
    "${QT_PLUGINS_DIR}/platforms"
    "${QT_PLUGINS_DIR}/audio"
    "${QT_PLUGINS_DIR}/accessible"
    "${QT_PLUGINS_DIR}/imageformats"
    "${QT_PLUGINS_DIR}/mediaservice"
    "${QT_PLUGINS_DIR}/playlistformats"
    "${QT_PLUGINS_DIR}/styles"
    "${QT_PLUGINS_DIR}/iconengines"
    DESTINATION "../PlugIns"
    CONFIGURATIONS Release MinSizeRel
    COMPONENT runtime
    FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}"
    PATTERN "*minimal*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
    PATTERN "*offscreen*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
    PATTERN "*quick*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
    PATTERN "*_debug${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
    )

  # add plugins path for Mac Bundle
  install (CODE "
  get_filename_component (the_qt_conf \"\${CMAKE_INSTALL_PREFIX}/qt.conf\" REALPATH)
  file (WRITE \"\${the_qt_conf}\"
\"
[Paths]
Plugins = ../PlugIns
\")"
    )

  # copy SoapySDR Modules
  # probably libmirsdrapi-rsp.dylib can't be re-distribuited
  # we remove the file at the end of fixup_bundle
  if(ENABLE_SOAPYSDR AND SOAPYSDR_FOUND)
    set(SOAPY_SDR_MOD_PATH "${SOAPYSDR_ROOT}/lib/SoapySDR/modules${SOAPYSDR_ABI_VERSION}")
    file(GLOB SOAPY_MODS ${SOAPY_SDR_MOD_PATH}/*.so)
    foreach(SOAPY_MOD_FILE ${SOAPY_MODS})
      install( FILES "${SOAPY_MOD_FILE}"
        DESTINATION "../Frameworks/SoapySDR/modules${SOAPYSDR_ABI_VERSION}"
        COMPONENT Runtime
        )
    endforeach()
  endif()

  install(CODE "
  file(GLOB_RECURSE SDRANGEL_PLUGINS
    \"\${CMAKE_INSTALL_PREFIX}/${INSTALL_PLUGINS_DIR}/*${CMAKE_SHARED_LIBRARY_SUFFIX}\")
  file(GLOB_RECURSE SDRANGEL_PLUGINSSRV
  \"\${CMAKE_INSTALL_PREFIX}/${INSTALL_PLUGINSSRV_DIR}/*${CMAKE_SHARED_LIBRARY_SUFFIX}\")
  file(GLOB_RECURSE QTPLUGINS \"\${CMAKE_INSTALL_PREFIX}/../PlugIns/*${CMAKE_SHARED_LIBRARY_SUFFIX}\")
  file(GLOB_RECURSE SOAPYSDR_PLUGINS \"\${CMAKE_INSTALL_PREFIX}/../Frameworks/SoapySDR/modules${SOAPY_SDR_ABI_VERSION}/*.so\")

  set(BU_CHMOD_BUNDLE_ITEMS ON)
  include(BundleUtilities)
  fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${CMAKE_PROJECT_NAME}\" \"\${SDRANGEL_PLUGINS};\${SDRANGEL_PLUGINSSRV};\${QTPLUGINS};\${SOAPYSDR_PLUGINS}\" \"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}\")

  # remove non distribuitable file
  if(ENABLE_MIRSDRAPI AND OFF)
    message(STATUS \"Remove libmirsdrapi-rsp.dylib - not distribuitable\")
    file(REMOVE \"\${CMAKE_INSTALL_PREFIX}/../Frameworks/libmirsdrapi-rsp.dylib\")
  endif()

  # remove unused library
  file(GLOB SDRANGEL_REMOVE_LIB \"\${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}/*${CMAKE_SHARED_LIBRARY_SUFFIX}\")
  foreach(file \${SDRANGEL_REMOVE_LIB})
    file(REMOVE \"\${file}\")
  endforeach()
  " COMPONENT Runtime)

  # TODO: code/dmg signature
  #   see https://github.com/cjcliffe/CubicSDR

# NOTE: only as reference; remove in future
elseif(LINUX AND BUNDLE)

  # NOTE: moved to debhelper tool (debian/ folder)
  #   useful for the moment to maintain dependencies list per distro
  find_program (DPKG_BUILDER dpkg-buildpackage DOC "Debian package builder")
  if (DPKG_BUILDER)
    #
    # Derive the correct filename for a Debian package because the DEB
    # generator doesn't do this correctly at present.
    #
    find_program (DPKG_PROGRAM dpkg DOC "dpkg program of Debian-based systems")
    if (DPKG_PROGRAM)
      execute_process (
        COMMAND ${DPKG_PROGRAM} --print-architecture
        OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    else (DPKG_PROGRAM)
      set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE noarch)
    endif (DPKG_PROGRAM)

    # TODO:
    #   - perseus package
    #   - CHANGELOG see https://github.com/xbmc/xbmc/pull/9987/commits/0152e8f50c0f11c8bddcfdc65e1596851fe8b310
    #   - verify dpkg info
    #   - Boost seems used through Qt

    # needs dpkg
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}_${LSB_CODENAME}_${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${APPLICATION_MAINTAINER}")
    set(CPACK_DEBIAN_PACKAGE_SECTION "hamradio")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libasound2, libfftw3-single3, libgcc1, libgl1-mesa-glx, libqt5core5a, libqt5gui5, libqt5multimedia5, libqt5network5, libqt5opengl5, libqt5widgets5, libqt5multimedia5-plugins, libstdc++6, libusb-1.0-0, pulseaudio, libxml2, ffmpeg, libopus0, codec2, libairspy0, libhackrf0, librtlsdr0, libbladerf1, libmirisdr0, libiio0")

    if ("${LSB_CODENAME}" STREQUAL "buster") # Debian 10
      set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, libavcodec58, libavformat58, libairspyhf1, libopencv-imgproc3.2, libopencv-highgui3.2, limesuite, soapysdr-tools")
    elseif("${LSB_CODENAME}" STREQUAL "stretch") # Debian 9
      set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, libavcodec57, libavformat57, libopencv-imgproc2.45v5, libopencv-highgu2.4-deb0, limesuite, soapysdr-tools")
    elseif("${LSB_CODENAME}" STREQUAL "bionic")  # Ubuntu 18.04
      set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, libavcodec57, libavformat57, libairspyhf0, libopencv-imgproc3.2, libopencv-highgui3.2, limesuite, soapysdr-tools")
    elseif("${LSB_CODENAME}" STREQUAL "xenial")  # Ubuntu 16.04
      set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, libavcodec-ffmpeg56, libavformat-ffmpeg56, libopencv-imgproc2.4v5, libopencv-highgui2.4v5")
    endif()

    # needed before cmake 2.8.3
    set(CPACK_SET_DESTDIR true)

  endif (DPKG_BUILDER)

  # NOTE: moved to distro tool
  # for fedora see https://docs.fedoraproject.org/en-US/quick-docs/creating-rpm-packages/
  #                https://docs.fedoraproject.org/en-US/packaging-guidelines/CMake/
  find_program (RPMBUILDER rpmbuild DOC "RPM package builder")
  if (RPMBUILDER)
    list(APPEND CPACK_GENERATOR "RPM")
    set(CPACK_RPM_PACKAGE_RELEASE "1")
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0")
    # TODO OpenSUSE and Fedora dependencies
    set(CPACK_RPM_PACKAGE_REQUIRES "libusb fftw qt5 pulseaudio libxml2")
    set (CPACK_RPM_PACKAGE_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
  endif (RPMBUILDER)

elseif((WIN32 OR MINGW) AND BUNDLE)

  #set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)

  list(APPEND CPACK_GENERATOR "NSIS")
  set(CPACK_NSIS_PACKAGE_NAME "${APPLICATION_NAME}")
  set(CPACK_NSIS_CONTACT "${APPLICATION_MAINTAINER}")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MODIFY_PATH ON)

endif(APPLE AND BUNDLE AND BUILD_GUI)

message(STATUS "CPack generators: ${CPACK_GENERATOR}")
include(CPack)
