# Stuff qmake can tell us
get_target_property (QMAKE_EXECUTABLE Qt::qmake LOCATION)
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
query_qmake (QT_INSTALL_QML QT_QML_DIR)
query_qmake (QT_INSTALL_LIBS QT_LIBS_DIR)

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

  # Create script that runs macdeployqt
  configure_file("${CMAKE_SOURCE_DIR}/cmake/cpack/deploy_mac.cmake.in" "${PROJECT_BINARY_DIR}/deploy_mac.cmake" @ONLY)

  set(CPACK_GENERATOR "Bundle")
  set(CPACK_BINARY_DRAGNDROP ON)
  set(CPACK_BUNDLE_NAME "${APPLICATION_NAME}")
  set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/cmake/cpack/sdrangel_icon.icns")
  set(CPACK_BUNDLE_PLIST "${CMAKE_BINARY_DIR}/Info.plist")
  set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/cmake/cpack/sdrangel_icon.icns")
  set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}_${CPACK_MACOS_PACKAGE_ARCHITECTURE}_${CMAKE_SYSTEM_PROCESSOR}")
  set(CPACK_PRE_BUILD_SCRIPTS "${PROJECT_BINARY_DIR}/deploy_mac.cmake")

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
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libasound2, libfftw3-single3, libgcc1, libgl1-mesa-glx, libqt5core5a, libqt5gui5, libqt5websockets5, libqt5multimedia5, libqt5network5, libqt5opengl5, libqt5widgets5, libqt5multimedia5-plugins, libstdc++6, libusb-1.0-0, pulseaudio, libxml2, ffmpeg, libopus0, codec2, libairspy0, libhackrf0, librtlsdr0, libbladerf1, libmirisdr0, libiio0")

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

elseif(WIN32 OR MINGW)

  # unfortunately some libraries are dependencies of dll so we copy by hand
  # TODO check if we can use fixup_bundle() and have libraries on root path

  # demoddatv.dll
  file(GLOB FFMPEG_DLLS "${FFMPEG_LIBRARIES}/*${CMAKE_SHARED_LIBRARY_SUFFIX}")
  foreach(ffmpeg_dll ${FFMPEG_DLLS})
    get_filename_component(ffmpeg_dll_name "${ffmpeg_dll}"  NAME)
    add_custom_target(copy_ffmpeg_${ffmpeg_dll_name} ALL
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ffmpeg_dll}" "${SDRANGEL_BINARY_BIN_DIR}/"
    )
  endforeach(ffmpeg_dll)

  # modatv.dll
  file(GLOB OPENCV_DLLS "${OpenCV_LIBS_BIN_DIR}/opencv_world*${CMAKE_SHARED_LIBRARY_SUFFIX}")
  foreach(opencv_dll ${OPENCV_DLLS})
    get_filename_component(opencv_dll_name "${opencv_dll}"  NAME)
    add_custom_target(copy_ffmpeg_${opencv_dll_name} ALL
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${opencv_dll}" "${SDRANGEL_BINARY_BIN_DIR}/"
    )
  endforeach(opencv_dll)

  if(ENABLE_SOAPYSDR AND SOAPYSDR_FOUND)
    # Soapy support libraries
    file(MAKE_DIRECTORY ${SDRANGEL_BINARY_LIB_DIR}/SoapySDR)
    file(MAKE_DIRECTORY ${SDRANGEL_BINARY_LIB_DIR}/SoapySDR/modules0.7)
    file(GLOB SOAPYSUPPORT_DLLS "${SOAPYSUPPORT_DIR}/lib/SoapySDR/modules0.7/*${CMAKE_SHARED_LIBRARY_SUFFIX}")
    foreach(soapysupport_dll ${SOAPYSUPPORT_DLLS})
      get_filename_component(soapysupport_dll_name "${soapysupport_dll}"  NAME)
      add_custom_target(copy_soapysupport_${soapysupport_dll_name} ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${soapysupport_dll}" "${SDRANGEL_BINARY_LIB_DIR}/SoapySDR/modules0.7/"
      )
    endforeach(soapysupport_dll)
  endif()

  file(GLOB CODEC2_DLLS "${CODEC2_DLL_DIR}/*${CMAKE_SHARED_LIBRARY_SUFFIX}")
  foreach(codec2_dll ${CODEC2_DLLS})
    get_filename_component(codec2_dll_name "${codec2_dll}"  NAME)
    add_custom_target(copy_codec2_${codec2_dll_name} ALL
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${codec2_dll}" "${SDRANGEL_BINARY_BIN_DIR}/"
    )
  endforeach(codec2_dll)

  file(GLOB HIDAPI_DLLS "${HIDAPI_DLL_DIR}/*${CMAKE_SHARED_LIBRARY_SUFFIX}")
  foreach(hidapi_dll ${HIDAPI_DLLS})
    get_filename_component(hidapi_dll_name "${hidapi_dll}"  NAME)
    add_custom_target(copy_hidapi_${hidapi_dll_name} ALL
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${hidapi_dll}" "${SDRANGEL_BINARY_BIN_DIR}/"
    )
  endforeach(hidapi_dll)

  if(ENABLE_USRP AND UHD_FOUND)
    file(GLOB UHD_DLLS "${UHD_DLL_DIR}/*${CMAKE_SHARED_LIBRARY_SUFFIX}")
    foreach(uhd_dll ${UHD_DLLS})
      get_filename_component(uhd_dll_name "${uhd_dll}"  NAME)
      add_custom_target(copy_uhd_${uhd_dll_name} ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${uhd_dll}" "${SDRANGEL_BINARY_BIN_DIR}/"
      )
    endforeach(uhd_dll)
  endif()

  if(ENABLE_SDRPLAY AND SDRPLAY_FOUND)
    file(GLOB SDRPLAY_DLLS "${SDRPLAY_DLL_DIR}/*${CMAKE_SHARED_LIBRARY_SUFFIX}")
    foreach(sdrplay_dll ${SDRPLAY_DLLS})
      get_filename_component(sdrplay_dll_name "${sdrplay_dll}"  NAME)
      add_custom_target(copy_sdrplay_${sdrplay_dll_name} ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${sdrplay_dll}" "${SDRANGEL_BINARY_BIN_DIR}/"
      )
    endforeach(sdrplay_dll)
  endif()

  file(GLOB OPENSSL_DLLS "${OPENSSL_DLL_DIR}/*${CMAKE_SHARED_LIBRARY_SUFFIX}")
  foreach(openssl_dll ${OPENSSL_DLLS})
    get_filename_component(openssl_dll_name "${openssl_dll}"  NAME)
    add_custom_target(copy_openssl_${openssl_dll_name} ALL
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${openssl_dll}" "${SDRANGEL_BINARY_BIN_DIR}/"
    )
  endforeach(openssl_dll)

  # TODO we need a way to fixup_bundle() on the build bin/ directory without call install
  if(BUILD_GUI)
    install(CODE "
      include(BundleUtilities)
      # BundleUtilities.cmake verify_app fails unless we ignore QtWebEngineProcess.exe, as it fails if there are any " external " prerequisites
      # Should we ignore it? It appears to work OK if we do. Is there a better way?
      fixup_bundle(\"${SDRANGEL_BINARY_BIN_DIR}/sdrangel${CMAKE_EXECUTABLE_SUFFIX}\" \"\" \"${WINDOWS_FIXUP_BUNDLE_LIB_DIRS}\" IGNORE_ITEM \"QtWebEngineProcess.exe\")
    " COMPONENT Runtime)
  elseif(BUILD_SERVER)
    install(CODE "
      include(BundleUtilities)
      fixup_bundle(\"${SDRANGEL_BINARY_BIN_DIR}/sdrangelsrv${CMAKE_EXECUTABLE_SUFFIX}\" \"\" \"${WINDOWS_FIXUP_BUNDLE_LIB_DIRS}\")
    " COMPONENT Runtime)
  endif(BUILD_GUI)

  if(BUNDLE)
    if(BUILD_GUI)
      install(CODE "
        include(BundleUtilities)
        fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/sdrangel${CMAKE_EXECUTABLE_SUFFIX}\" \"\" \"${WINDOWS_FIXUP_BUNDLE_LIB_DIRS}\")
      " COMPONENT Runtime)
      install(FILES "${FFTW3F_WISDOM}" DESTINATION "${INSTALL_BIN_DIR}")
    elseif(BUILD_SERVER)
      install(CODE "
        include(BundleUtilities)
        fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/sdrangelsrv${CMAKE_EXECUTABLE_SUFFIX}\" \"\" \"${WINDOWS_FIXUP_BUNDLE_LIB_DIRS}\")
      " COMPONENT Runtime)
    endif(BUILD_GUI)

    # unfortunately some libraries are dependencies of dll so we copy by hand
    # TODO check if we can use fixup_bundle() and have libraries on root path
    # inputkiwisdr.dll
    # demoddatv.dll
    install(DIRECTORY "${FFMPEG_LIBRARIES}/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}")
    # modatv.dll
    install(DIRECTORY "${OpenCV_LIBS_BIN_DIR}/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "opencv_world*${CMAKE_SHARED_LIBRARY_SUFFIX}")
    # libusb
    get_filename_component(GFN_LIBUSB_DIR "${LIBUSB_LIBRARIES}" DIRECTORY)
    install(FILES "${GFN_LIBUSB_DIR}/libusb-1.0${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    # codec2
    install(DIRECTORY "${CODEC2_DLL_DIR}/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}")
    # hidapi
    install(DIRECTORY "${HIDAPI_DLL_DIR}/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}")
    # uhd
    install(DIRECTORY "${UHD_DLL_DIR}/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}")
    # sdrplay
    install(DIRECTORY "${SDRPLAY_DLL_DIR}/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}")
    # OpenSSL
    install(DIRECTORY "${OPENSSL_DLL_DIR}/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}")


    install(CODE "
      # remove *.lib files
      # TODO find a better way
      file(GLOB NSIS_REMOVE_LIB \"\${CMAKE_INSTALL_PREFIX}/*.lib\")
      foreach(file \${NSIS_REMOVE_LIB})
        file(REMOVE \"\${file}\")
      endforeach()
      file(GLOB NSIS_REMOVE_LIB \"\${CMAKE_INSTALL_PREFIX}/${INSTALL_PLUGINS_DIR}/*.lib\")
      foreach(file \${NSIS_REMOVE_LIB})
        file(REMOVE \"\${file}\")
      endforeach()
      file(GLOB NSIS_REMOVE_LIB \"\${CMAKE_INSTALL_PREFIX}/${INSTALL_PLUGINSSRV_DIR}/*.lib\")
      foreach(file \${NSIS_REMOVE_LIB})
        file(REMOVE \"\${file}\")
      endforeach()
    " COMPONENT Runtime)

    list(APPEND CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_MONOLITHIC_INSTALL ON)
    set(CPACK_NSIS_HELP_LINK ${CPACK_PACKAGE_CONTACT})
    set(CPACK_NSIS_URL_INFO_ABOUT ${CPACK_PACKAGE_CONTACT})
    set(CPACK_NSIS_CONTACT ${APPLICATION_MAINTAINER})
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/cmake/cpack/sdrangel_icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/cmake/cpack/sdrangel_icon.ico")
    set(CPACK_NSIS_PACKAGE_NAME "${APPLICATION_NAME}")
    set(CPACK_NSIS_CONTACT "${APPLICATION_MAINTAINER}")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY ${APPLICATION_NAME})
    set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY ${APPLICATION_NAME})
    list(APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
      ExecWait '$INSTDIR\\\\${VCREDIST_FILE} /passive /norestart'
      Delete '$INSTDIR\\\\${VCREDIST_FILE}'
    ")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
      "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\SDRangel.lnk' '$INSTDIR\\\\sdrangel.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
      "Delete '$SMPROGRAMS\\\\$START_MENU\\\\SDRangel.lnk'")
  endif(BUNDLE)

endif(APPLE AND BUNDLE AND BUILD_GUI)

message(STATUS "CPack generators: ${CPACK_GENERATOR}")
include(CPack)
