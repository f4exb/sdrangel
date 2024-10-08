# Many idea are exposed at
#  https://stackoverflow.com/questions/15175318/cmake-how-to-build-external-projects-and-include-their-targets
#  https://crascit.com/2015/07/25/cmake-gtest/
#
# in essence, we need a way to build external libraries before use it;
# we choose add_dependencies() to the target.
# other viable solution is to use execute_process() during config
# but make the configuration dirty

# which version/tag/checkout to use
set(CODEC2_TAG "v1.0.3")
set(CM256CC_TAG c0e92b92aca3d1d36c990b642b937c64d363c559)
set(MBELIB_TAG fe83b32c6a60cdd7bce8cecf3c7a0b9ec87a7667)
set(SERIALDV_TAG "v1.1.4")
set(DSDCC_TAG "v1.9.5")
set(LIMESUITE_TAG 70e3859a55d8d5353963a5318013c8454594769f)
set(BLADERF_TAG "2021.02")
set(LIBIIO_TAG 826563e41b5ce9890b75506f672017de8d76d52d)
set(AIRSPYHF_TAG "1af81c0ca18944b8c9897c3c98dc0a991815b686")
set(PERSEUS_TAG afefa23e3140ac79d845acb68cf0beeb86d09028)
set(MIRISDR_TAG "v2.0.0")
set(LIBSIGMF_TAG "new-namespaces")

set(PTHREADS4W_TAG "Version-2-11-0-release")
set(RTLSDR_TAG "v2.0.1")
set(RTLSDR_UDEV OFF)
set(SOAPYSDR_TAG "soapy-sdr-0.7.1")
set(SOAPYSDR_SDRPLAY_TAG "soapy-sdrplay-0.2.0")
set(SOAPYSDR_AUDIO_TAG "soapy-audio-0.1.1")
set(SOAPYSDR_REMOTE_TAG "soapy-remote-0.5.1")
set(AIRSPY_TAG "37c768ce9997b32e7328eb48972a7fda0a1f8554")
set(HACKRF_TAG "v2022.09.1")
set(LIBXML2_TAG "v2.10.4")
set(UHD_TAG "v4.6.0.0")
if (APPLE AND (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL x86_64))
    # UHD fails to load with 1.80+
    set(BOOST_TAG "1.78.0")
    set(BOOST_TAG2 "1_78_0")
else()
    set(BOOST_TAG "1.83.0")
    set(BOOST_TAG2 "1_83_0")
endif()

# For some external project macros
include(ExternalProject)

# default build destination in windows to avoid Release/Debug folder
if (WIN32)
    # the build will be output on the same folder of sdrangel
    set(COMMON_CMAKE_ARGS
            -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${SDRANGEL_BINARY_BIN_DIR}
            -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=${SDRANGEL_BINARY_BIN_DIR}
            -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${SDRANGEL_BINARY_BIN_DIR}
            -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO=${SDRANGEL_BINARY_BIN_DIR}
            -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${SDRANGEL_BINARY_LIB_DIR}
            -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=${SDRANGEL_BINARY_LIB_DIR}
            -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=${SDRANGEL_BINARY_LIB_DIR}
            -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO=${SDRANGEL_BINARY_LIB_DIR}
            -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${SDRANGEL_BINARY_LIB_DIR}
            -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG=${SDRANGEL_BINARY_LIB_DIR}
            -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE=${SDRANGEL_BINARY_LIB_DIR}
            -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO=${SDRANGEL_BINARY_LIB_DIR}
            )
elseif (LINUX)
    set(COMMON_CMAKE_ARGS
            -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${EXTERNAL_BUILD_LIBRARIES}/bin
            -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=${EXTERNAL_BUILD_LIBRARIES}/bin
            -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${EXTERNAL_BUILD_LIBRARIES}/bin
            -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO=${EXTERNAL_BUILD_LIBRARIES}/bin
            -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}
            -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}
            -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}
            -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO=${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}
            -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}
            -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG=${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}
            -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE=${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}
            -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO=${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}
            )
endif ()

if(CMAKE_MSVC_RUNTIME_LIBRARY)
  list(APPEND COMMON_CMAKE_ARGS -DCMAKE_POLICY_DEFAULT_CMP0091:STRING=NEW)
  list(APPEND COMMON_CMAKE_ARGS -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY})
endif()

if(CMAKE_BUILD_TYPE)
  list(APPEND COMMON_CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
endif()

if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
  list(APPEND COMMON_CMAKE_ARGS -DCMAKE_POLICY_DEFAULT_CMP0069:STRING=NEW)
  list(APPEND COMMON_CMAKE_ARGS -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=${CMAKE_INTERPROCEDURAL_OPTIMIZATION})
endif()

list(APPEND COMMON_CMAKE_ARGS -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER})
list(APPEND COMMON_CMAKE_ARGS -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER})
if (CMAKE_LINKER)
  list(APPEND COMMON_CMAKE_ARGS -DCMAKE_LINKER=${CMAKE_LINKER})
endif()
if (CMAKE_AR)
  list(APPEND COMMON_CMAKE_ARGS -DCMAKE_AR=${CMAKE_AR})
endif()

list(APPEND COMMON_CMAKE_ARGS -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD})
list(APPEND COMMON_CMAKE_ARGS -DCMAKE_CXX_STANDARD_REQUIRED=${CMAKE_CXX_STANDARD_REQUIRED})
list(APPEND COMMON_CMAKE_ARGS -DCMAKE_CXX_EXTENSIONS=${CMAKE_CXX_EXTENSIONS})

if (LINUX)
    # macro that create symbolic links
    macro(makeLink src dest target)
        if (NOT EXISTS ${dest})
            add_custom_command(TARGET ${target} PRE_BUILD
                    COMMAND ${CMAKE_COMMAND} -E create_symlink ${src} ${dest} DEPENDS ${dest} COMMENT "mklink ${src} -> ${dest}")
        endif ()
    endmacro()
    # macro that copy folder
    macro(makeCopy src dest target)
        if (NOT EXISTS ${dest})
            add_custom_command(TARGET ${target} PRE_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory ${src} ${dest} DEPENDS ${dest} COMMENT "copy ${src} -> ${dest}")
        endif ()
    endmacro()
    # macro that copy file
    macro(makeCopyFile src dest target)
        if (NOT EXISTS ${dest})
            add_custom_command(TARGET ${target} PRE_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${src} ${dest} DEPENDS ${dest} COMMENT "copy ${src} -> ${dest}")
        endif ()
    endmacro()
else ()
    # macro that create symbolic links
    macro(makeLink src dest target)
        if (NOT EXISTS ${dest})
            add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E create_symlink ${src} ${dest} DEPENDS ${dest} COMMENT "mklink ${src} -> ${dest}")
        endif ()
    endmacro()

    # macro that copy folder
    macro(makeCopyDir src dest target)
        if (NOT EXISTS ${dest})
            add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory ${src} ${dest} DEPENDS ${dest} COMMENT "copy ${src} -> ${dest}")
        endif ()
    endmacro()

    # macro that copy file
    macro(makeCopy src dest target)
        if (NOT EXISTS ${dest})
            add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${src} ${dest} DEPENDS ${dest} COMMENT "copy ${src} -> ${dest}")
        endif ()
    endmacro()
endif (LINUX)

if (WIN32)
    if (VS2015)
        if (CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(VCREDIST_FILE "vcredist_msvc2015_x64${CMAKE_EXECUTABLE_SUFFIX}" CACHE INTERNAL "")
            # maybe add arm architecture
        else ()
            set(VCREDIST_FILE "vcredist_msvc2015_x86${CMAKE_EXECUTABLE_SUFFIX}" CACHE INTERNAL "")
        endif ()
    elseif (VS2017)
        if (CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(VCREDIST_FILE "vcredist_msvc2017_x64${CMAKE_EXECUTABLE_SUFFIX}" CACHE INTERNAL "")
            # maybe add arm architecture
        else ()
            set(VCREDIST_FILE "vcredist_msvc2017_x86${CMAKE_EXECUTABLE_SUFFIX}" CACHE INTERNAL "")
        endif ()
    else ()
        if (CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(VCREDIST_FILE "vc_redist.x64${CMAKE_EXECUTABLE_SUFFIX}" CACHE INTERNAL "")
            # maybe add arm architecture
        else ()
            set(VCREDIST_FILE "vc_redist.x86${CMAKE_EXECUTABLE_SUFFIX}" CACHE INTERNAL "")
        endif ()
    endif ()
    install(FILES "${EXTERNAL_LIBRARY_FOLDER}/vcredist/${VCREDIST_FILE}" DESTINATION "${INSTALL_BIN_DIR}")
endif (WIN32)

set(MACOS_EXTERNAL_LIBS_FIXUP "${CMAKE_BINARY_DIR}/${INSTALL_LIB_DIR}")

# While some of the following projects are not installed (to save time & space), some have to
# be so that on MacOS, the paths embedded in the libraries to other libraries are correct (as they are set to
# install path, not build). This is required for macdeployqt to be able to find them

# Some of the following projects are currently built only for MacOS
# For WIN32, we use precompiled versions from sdrangel-windows-libraries
# For LINUX, we require system pacakges to be installed
if (APPLE)
    set(USE_PRECOMPILED_LIBS FALSE)
else()
    set(USE_PRECOMPILED_LIBS TRUE)
endif()

# We don't CACHE the _DEPENDS variables, as some external projects are only added when NOT _FOUND
# and not in subsequent configures when _FOUND (E.g. ffmpeg/opencv aren't added second time
# around as it slows down the build and causes some dependencies to always be rebuilt.)
# Use PARENT_SCOPE so they can be used in other CMakeFiles.
macro(set_global NAME VALUE)
    set(${NAME} ${VALUE})
    set(${NAME} ${VALUE} PARENT_SCOPE)
endmacro()

# Setting a cache variable doesn't update the variable in parent scope if already set,
# so we need to set the cache and parent_scope separately
macro(set_global_cache NAME VALUE)
    set(${NAME} ${VALUE} CACHE INTERNAL "")
    set(${NAME} ${VALUE} PARENT_SCOPE)
endmacro()

if (NOT LIBUSB_FOUND AND NOT USE_PRECOMPILED_LIBS)
    # Cloning git repo doesn't include configure, so we download the bz2 which does
    ExternalProject_Add(libusb
            URL https://github.com/libusb/libusb/releases/download/v1.0.26/libusb-1.0.26.tar.bz2
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/libusb"
            CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
            BUILD_COMMAND ${MAKE}
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(libusb install_dir)
    set_global(LIBUSB_DEPENDS libusb)
    set_global_cache(LIBUSB_FOUND ON)
    set(LIBUSB_EXTERNAL ON CACHE INTERNAL "")
    set(LIBUSB_INCLUDE_DIR "${install_dir}/include/libusb-1.0" CACHE INTERNAL "")
    if (WIN32)
        set(LIBUSB_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/libusb.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(LIBUSB_LIBRARIES "${install_dir}/lib${LIB_SUFFIX}/libusb-1.0${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    elseif (APPLE)
        set(LIBUSB_LIBRARIES "${install_dir}/lib/libusb-1.0${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${install_dir}/lib")
    endif ()
endif (NOT LIBUSB_FOUND AND NOT USE_PRECOMPILED_LIBS)

if (NOT HIDAPI_FOUND AND NOT USE_PRECOMPILED_LIBS AND ENABLE_FUNCUBE)
    ExternalProject_Add(hidapi
            GIT_REPOSITORY https://github.com/libusb/hidapi.git
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/hidapi"
            CMAKE_ARGS ${COMMON_CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(hidapi install_dir)
    set_global(HIDAPI_DEPENDS hidapi)
    set_global_cache(HIDAPI_FOUND ON)
    set(HIDAPI_EXTERNAL ON CACHE INTERNAL "")
    set(HIDAPI_ROOT_DIR "${install_dir}" CACHE INTERNAL "")
    set(HIDAPI_INCLUDE_DIRS "${install_dir}/include/hidapi" CACHE INTERNAL "")
    if (APPLE)
        set(HIDAPI_LIBRARIES "${install_dir}/lib/libhidapi${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${install_dir}/lib")
    endif ()
endif ()

if (NOT OPUS_FOUND AND NOT USE_PRECOMPILED_LIBS)
    ExternalProject_Add(opus
            GIT_REPOSITORY https://github.com/xiph/opus
            GIT_TAG v1.3.1
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/opus"
            CMAKE_ARGS ${COMMON_CMAKE_ARGS} -DOPUS_BUILD_SHARED_LIBRARY=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(opus install_dir)
    set_global(OPUS_DEPENDS opus)
    set_global_cache(OPUS_FOUND ON)
    set(OPUS_EXTERNAL ON CACHE INTERNAL "")
    set(OPUS_INCLUDE_DIRS "${install_dir}/include" CACHE INTERNAL "")
    set(OPUS_PKG_CONFIG_DIR "${install_dir}/lib/pkgconfig" CACHE INTERNAL "")
    if (WIN32)
        set(OPUS_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/libopus.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(OPUS_LIBRARIES "${install_dir}/lib${LIB_SUFFIX}/libopus${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    elseif (APPLE)
        set(OPUS_LIBRARIES "${install_dir}/lib/libopus${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${install_dir}/lib")
    endif ()
endif (NOT OPUS_FOUND AND NOT USE_PRECOMPILED_LIBS)

if (NOT ZLIB_FOUND AND NOT USE_PRECOMPILED_LIBS)
    ExternalProject_Add(zlib
            URL https://zlib.net/current/zlib.tar.gz
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/zlib"
            CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(zlib install_dir)
    set_global(ZLIB_DEPENDS zlib)
    set_global_cache(ZLIB_FOUND ON)
    set(ZLIB_EXTERNAL ON CACHE INTERNAL "")
    set(ZLIB_INCLUDE_DIRS "${install_dir}/include" CACHE INTERNAL "")
    if (WIN32)
        set(ZLIB_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/libz.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(ZLIB_LIBRARIES "${install_dir}/lib${LIB_SUFFIX}/libz${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    elseif (APPLE)
        set(ZLIB_LIBRARIES "${install_dir}/lib/libz${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${install_dir}/lib")
    endif ()
endif (NOT ZLIB_FOUND AND NOT USE_PRECOMPILED_LIBS)

if (NOT FAAD_FOUND AND NOT USE_PRECOMPILED_LIBS)
    ExternalProject_Add(faad
            URL https://downloads.sourceforge.net/project/faac/faad2-src/faad2-2.8.0/faad2-2.8.8.tar.gz
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/faad"
            CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(faad source_dir binary_dir install_dir)
    set_global(FAAD_DEPENDS faad)
    set_global_cache(FAAD_FOUND ON)
    set(FAAD_EXTERNAL ON CACHE INTERNAL "")
    set(FAAD_INCLUDE_DIR "${install_dir}/include" CACHE INTERNAL "")
    if (WIN32)
        set(FAAD_LIBRARY "${SDRANGEL_BINARY_LIB_DIR}/libfaad.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(FAAD_LIBRARY "${install_dir}/lib${LIB_SUFFIX}/libfaad${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    elseif (APPLE)
        set(FAAD_LIBRARY "${install_dir}/lib/libfaad${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${install_dir}/lib")
    endif ()
endif (NOT FAAD_FOUND AND NOT USE_PRECOMPILED_LIBS)

if (NOT FFTW3F_FOUND AND NOT USE_PRECOMPILED_LIBS)
    ExternalProject_Add(fftw3f
            URL http://fftw.org/fftw-3.3.10.tar.gz
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/fftw3f"
            CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-float
            BUILD_COMMAND ${MAKE}
            INSTALL_COMMAND ""
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(fftw3f source_dir binary_dir)
    set_global(FFTW3F_DEPENDS fftw3f)
    set_global_cache(FFTW3F_FOUND ON)
    set(FFTW3F_EXTERNAL ON CACHE INTERNAL "")
    set(FFTW3F_INCLUDE_DIRS "${source_dir}/api" "${binary_dir}" CACHE INTERNAL "")
    if (WIN32)
        set(FFTW3F_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/libfftw3f.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(FFTW3F_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libfftw3f${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    elseif (APPLE)
        set(FFTW3F_LIBRARIES "${binary_dir}/.libs/libfftw3f.a" CACHE INTERNAL "")
    endif ()
endif (NOT FFTW3F_FOUND AND NOT USE_PRECOMPILED_LIBS)

# Can't build Boost using ExternalProject_Add, as we want to use find_package(Boost) as it defines many variables,
# and using find_package requires Boost to have already been built
# So instead, we use FetchContent and build Boost at CMake configure time
include(FetchContent)
FetchContent_Declare(boost
    URL "https://sourceforge.net/projects/boost/files/boost/${BOOST_TAG}/boost_${BOOST_TAG2}.tar.gz/download"
)

find_package(Boost QUIET)
if (NOT Boost_FOUND AND NOT USE_PRECOMPILED_LIBS)

    message("Downloading Boost")
    FetchContent_MakeAvailable(boost)

    # Add path to where Boost is built
    set(CMAKE_PREFIX_PATH ${boost_BINARY_DIR})

    # Check to see if already built - Don't build again
    find_package(Boost QUIET)
    if(NOT Boost_FOUND)
        execute_process(
            COMMAND ./bootstrap.sh --prefix=${boost_BINARY_DIR}
             WORKING_DIRECTORY ${boost_SOURCE_DIR}
        )
        execute_process(
            COMMAND ./b2
            WORKING_DIRECTORY ${boost_SOURCE_DIR}
        )
        execute_process(
            COMMAND ./b2 install
            WORKING_DIRECTORY ${boost_SOURCE_DIR}
        )
        message("Boost built in ${boost_BINARY_DIR}")
    endif()

    # Cache binary dir so we know it was built in subsequent builds
    set(boost_BINARY_DIR ${boost_BINARY_DIR} CACHE INTERNAL "")

    # Check it was built and import the variables
    find_package(Boost REQUIRED)

    if (APPLE)
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${boost_BINARY_DIR}/lib")
    endif()
else()
    if (DEFINED boost_BINARY_DIR)
        if (APPLE)
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${boost_BINARY_DIR}/lib")
        endif()
    endif()
endif()

if (NOT FFMPEG_FOUND AND NOT USE_PRECOMPILED_LIBS)

    # Libs only used by ffmpeg

    # They are installed so ffmpeg can use pkg-config to find them

    if (NOT X264_FOUND OR X264_EXTERNAL)
        # x264's configure script forces a full rebuild each time, so set UPDATE_DISCONNECTED
        # so it's only downloaded
        ExternalProject_Add(x264
            GIT_REPOSITORY https://code.videolan.org/videolan/x264.git
            UPDATE_DISCONNECTED TRUE
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/x264"
            CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-shared
            BUILD_COMMAND ${MAKE}
            TEST_COMMAND ""
            )
        ExternalProject_Get_Property(x264 install_dir)
        set_global(X264_DEPENDS x264)
        set(X264_PKG_CONFIG_DIR "${install_dir}/lib/pkgconfig")
        set_global_cache(X264_FOUND ON)
        set(X264_EXTERNAL ON CACHE INTERNAL "")
        if (APPLE)
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${install_dir}/lib")
        endif ()
    endif (NOT X264_FOUND OR X264_EXTERNAL)

    if (NOT X265_FOUND OR X265_EXTERNAL)
        ExternalProject_Add(x265
            GIT_REPOSITORY https://bitbucket.org/multicoreware/x265_git.git
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/x265"
            SOURCE_SUBDIR "source"
            CMAKE_ARGS ${COMMON_CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            TEST_COMMAND ""
            )
        ExternalProject_Get_Property(x265 install_dir)
        set_global(X265_DEPENDS x265)
        set(X265_PKG_CONFIG_DIR "${install_dir}/lib/pkgconfig")
        set_global_cache(X265_FOUND ON)
        set(X265_EXTERNAL ON CACHE INTERNAL "")
        if (APPLE)
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${install_dir}/lib")
        endif ()
    endif (NOT X265_FOUND OR X265_EXTERNAL)

    if (NOT FDK_AAC_FOUND OR FDK_AAC_EXTERNAL)
        ExternalProject_Add(fdk_aac
            GIT_REPOSITORY https://github.com/mstorsjo/fdk-aac.git
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/fdk-aac"
            CMAKE_ARGS ${COMMON_CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            TEST_COMMAND ""
            )
        ExternalProject_Get_Property(fdk_aac install_dir)
        set_global(FDK_AAC_DEPENDS fdk_aac)
        set(FDK_AAC_PKG_CONFIG_DIR "${install_dir}/lib/pkgconfig")
        set_global_cache(FDK_AAC_FOUND ON)
        set(FDK_AAC_EXTERNAL ON CACHE INTERNAL "")
        if (APPLE)
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${install_dir}/lib")
        endif ()
    endif (NOT FDK_AAC_FOUND OR FDK_AAC_EXTERNAL)

    # https://www.mpg123.de/download.shtml
    if (NOT MPG123_FOUND OR MPG123_EXTERNAL)
        ExternalProject_Add(mpg123
            URL https://downloads.sourceforge.net/project/mpg123/mpg123/1.29.3/mpg123-1.29.3.tar.bz2
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/mpg123"
            CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-shared
            BUILD_COMMAND ${MAKE}
            TEST_COMMAND ""
            )
        ExternalProject_Get_Property(mpg123 install_dir)
        set_global(MPG123_DEPENDS mpg123)
        set(MPG123_PKG_CONFIG_DIR "${install_dir}/lib/pkgconfig")
        set_global_cache(MPG123_FOUND ON)
        set(MPG123_EXTERNAL ON CACHE INTERNAL "")
        if (APPLE)
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${install_dir}/lib")
        endif ()
    endif (NOT MPG123_FOUND OR MPG123_EXTERNAL)

    if (NOT LAME_FOUND OR LAME_EXTERNAL)
        ExternalProject_Add(lame
            SVN_REPOSITORY http://svn.code.sf.net/p/lame/svn/trunk/lame
            UPDATE_DISCONNECTED TRUE
            DEPENDS ${MPG123_DEPENDS}
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/lame"
            CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env PKG_CONFIG_PATH=${MPG123_PKG_CONFIG_DIR} <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-shared --disable-frontend
            BUILD_COMMAND ${MAKE}
            TEST_COMMAND ""
            )
        ExternalProject_Get_Property(lame install_dir)
        set_global(LAME_DEPENDS lame)
        # ffmpeg doesn't try to use pkg-config for lame, as it doesn't include a .pc file
        #set(LAME_PKG_CONFIG_DIR "${install_dir}/lib/pkgconfig")
        set(LAME_EXTRA_CFLAGS "-I${install_dir}/include")
        if (APPLE AND (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL arm64))
            set(LAME_EXTRA_LDFLAGS "-L${install_dir}/lib -Wl,-ld_classic")
        else()
            set(LAME_EXTRA_LDFLAGS "-L${install_dir}/lib")
        endif()
        set_global_cache(LAME_FOUND ON)
        set(LAME_EXTERNAL ON CACHE INTERNAL "")
        if (APPLE)
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${install_dir}/lib")
        endif ()
    endif (NOT LAME_FOUND OR LAME_EXTERNAL)

    # We have to install this so OpenCV can find it
    # OpenCV doesn't currently build with ffmpeg 5, so use 4
    set(FFMPEG_EXTRA_LDFLAGS ${LAME_EXTRA_LDFLAGS})
    set(FFMPEG_EXTRA_CFLAGS ${LAME_EXTRA_CFLAGS})
    if (APPLE AND (XCODE_VERSION VERSION_GREATER_EQUAL 15.0.0))
        set(FFMPEG_EXTRA_LDFLAGS "${FFMPEG_EXTRA_LDFLAGS} -Wl,-ld_classic")
    endif()
    ExternalProject_Add(ffmpeg
            GIT_REPOSITORY https://github.com/FFmpeg/FFmpeg.git
            GIT_TAG n4.4.2
            DEPENDS ${X264_DEPENDS} ${X265_DEPENDS} ${OPUS_DEPENDS} ${FDK_AAC_DEPENDS} ${LAME_DEPENDS}
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/ffmpeg"
            CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env PKG_CONFIG_PATH=${X264_PKG_CONFIG_DIR}:${X265_PKG_CONFIG_DIR}:${OPUS_PKG_CONFIG_DIR}:${FDK_AAC_PKG_CONFIG_DIR} <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-shared --enable-gpl --enable-nonfree --enable-libx264 --enable-libx265 --enable-libopus --enable-libfdk-aac --enable-libmp3lame --extra-ldflags=${FFMPEG_EXTRA_LDFLAGS} --extra-cflags=${FFMPEG_EXTRA_CFLAGS}
            BUILD_COMMAND ${MAKE}
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(ffmpeg install_dir)
    set_global(FFMPEG_DEPENDS ffmpeg)
    set(FFMPEG_INSTALL_DIR "${install_dir}")
    set_global_cache(FFMPEG_FOUND ON)
    set(FFMPEG_EXTERNAL ON CACHE INTERNAL "")
    set(FFMPEG_INCLUDE_DIRS "${install_dir}/include" CACHE INTERNAL "")
    set(AVCODEC_INCLUDE_DIRS "${install_dir}/include" CACHE INTERNAL "")
    set(AVFORMAT_INCLUDE_DIRS "${install_dir}/include" CACHE INTERNAL "")
    set(AVUTIL_INCLUDE_DIRS "${install_dir}/include" CACHE INTERNAL "")
    set(SWRESAMPLE_INCLUDE_DIRS "${install_dir}/include" CACHE INTERNAL "")
    set(SWSCALE_INCLUDE_DIRS "${install_dir}/include" CACHE INTERNAL "")
    set(AVCODEC_LIBRARIES "${install_dir}/lib/libavcodec${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    set(AVFORMAT_LIBRARIES "${install_dir}/lib/libavformat${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    set(AVUTIL_LIBRARIES "${install_dir}/lib/libavutil${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    set(SWRESAMPLE_LIBRARIES "${install_dir}/lib/libswresample${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    set(SWSCALE_LIBRARIES "${install_dir}/lib/libswscale${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    if (APPLE)
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${install_dir}/lib")
    endif ()

endif (NOT FFMPEG_FOUND AND NOT USE_PRECOMPILED_LIBS)

# OpenCV only used by Mod ATV curently
if (NOT OpenCV_FOUND AND NOT USE_PRECOMPILED_LIBS AND ENABLE_CHANNELTX_MODATV)
    ExternalProject_Add(opencv
            GIT_REPOSITORY https://github.com/opencv/opencv.git
            GIT_TAG 4.5.5
            DEPENDS "${FFMPEG_DEPENDS}"
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/opencv"
            CMAKE_ARGS ${COMMON_CMAKE_ARGS} -DWITH_FFMPEG=ON -DCMAKE_PREFIX_PATH=${FFMPEG_INSTALL_DIR}
            INSTALL_COMMAND ""
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(opencv source_dir binary_dir)
    set_global(OpenCV_DEPENDS opencv)
    set_global_cache(OpenCV_FOUND ON)
    set(OpenCV_EXTERNAL ON CACHE INTERNAL "")
    set(OpenCV_INCLUDE_DIRS
        "${source_dir}/modules/calib3d/include"
        "${source_dir}/modules/core/include"
        "${source_dir}/modules/dnn/include"
        "${source_dir}/modules/features2d/include"
        "${source_dir}/modules/flann/include"
        "${source_dir}/modules/gapi/include"
        "${source_dir}/modules/highgui/include"
        "${source_dir}/modules/imgcodecs/include"
        "${source_dir}/modules/imgproc/include"
        "${source_dir}/modules/java/include"
        "${source_dir}/modules/java/include"
        "${source_dir}/modules/js/include"
        "${source_dir}/modules/ml/include"
        "${source_dir}/modules/obj/include"
        "${source_dir}/modules/objdetect/include"
        "${source_dir}/modules/photo/include"
        "${source_dir}/modules/python/include"
        "${source_dir}/modules/stitching/include"
        "${source_dir}/modules/ts/include"
        "${source_dir}/modules/video/include"
        "${source_dir}/modules/videoio/include"
        "${source_dir}/modules/world/include"
        "${binary_dir}"
        CACHE INTERNAL "")
    if (WIN32)
        set(OpenCV_LIBS "${SDRANGEL_BINARY_LIB_DIR}/libopencv.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(OpenCV_LIBS "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libopencv${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    elseif (APPLE)
        set(OpenCV_LIBS "${binary_dir}/lib/libopencv_core${CMAKE_SHARED_LIBRARY_SUFFIX};${binary_dir}/lib/libopencv_core${CMAKE_SHARED_LIBRARY_SUFFIX};${binary_dir}/lib/libopencv_highgui${CMAKE_SHARED_LIBRARY_SUFFIX};${binary_dir}/lib/libopencv_video${CMAKE_SHARED_LIBRARY_SUFFIX};${binary_dir}/lib/libopencv_videoio${CMAKE_SHARED_LIBRARY_SUFFIX};${binary_dir}/lib/libopencv_imgproc${CMAKE_SHARED_LIBRARY_SUFFIX};${binary_dir}/lib/libopencv_imgcodecs${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/lib")
    endif ()
endif (NOT OpenCV_FOUND AND NOT USE_PRECOMPILED_LIBS AND ENABLE_CHANNELTX_MODATV)

if ((NOT CODEC2_FOUND OR CODEC2_EXTERNAL) AND NOT USE_PRECOMPILED_LIBS)
    # needs speexdsp
    if (WIN32)
        set(CODEC2_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/codec2.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(CODEC2_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libcodec2${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    endif ()
    # Repo temp set to codec2-dev to use v1.0.3 tag
    # Should revert to codec2 when we eventually update to latest (v1.2.0+) release
    ExternalProject_Add(codec2
            GIT_REPOSITORY https://github.com/drowe67/codec2-dev.git
            GIT_TAG ${CODEC2_TAG}
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/codec2"
            CMAKE_ARGS ${COMMON_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${CODEC2_LIBRARIES}"
            INSTALL_COMMAND ""
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(codec2 source_dir binary_dir)
    set_global(CODEC2_DEPENDS codec2)
    set_global_cache(CODEC2_FOUND ON)
    set(CODEC2_EXTERNAL ON CACHE INTERNAL "")
    set(CODEC2_INCLUDE_DIR "${source_dir}/src" "${binary_dir}" CACHE INTERNAL "")
    if (WIN32)
        set(CODEC2_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/codec2.lib" CACHE INTERNAL "")
        makeCopy("${source_dir}/src" "${source_dir}/src/codec2" codec2)
    elseif (LINUX)
        set(CODEC2_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libcodec2${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        makeLink("${source_dir}/src" "${source_dir}/src/codec2" codec2)
    elseif (APPLE)
        set(CODEC2_LIBRARIES "${binary_dir}/src/libcodec2${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        # some source include "codec2/comp.h" and some without codec2
        makeLink("${source_dir}/src" "${source_dir}/src/codec2" codec2)
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/src")
    endif ()
endif ((NOT CODEC2_FOUND OR CODEC2_EXTERNAL) AND NOT USE_PRECOMPILED_LIBS)

if (NOT CM256CC_FOUND OR CM256CC_EXTERNAL)
    # needs boost
    if (WIN32)
        set(CM256CC_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/cm256cc.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(CM256CC_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libcm256cc${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    endif ()
    string(REPLACE ";" "|" CMAKE_PREFIX_PATH_SEP "${CMAKE_PREFIX_PATH}")
    ExternalProject_Add(cm256cc
            GIT_REPOSITORY https://github.com/f4exb/cm256cc.git
            GIT_TAG ${CM256CC_TAG}
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/cm256cc"
            LIST_SEPARATOR |
            CMAKE_ARGS ${COMMON_CMAKE_ARGS}
            -DBUILD_TOOLS=OFF
            -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH_SEP}
            -DENABLE_DISTRIBUTION=ON
            -DBoost_INCLUDE_DIR=${Boost_INCLUDE_DIR}
            BUILD_BYPRODUCTS "${CM256CC_LIBRARIES}"
            INSTALL_COMMAND ""
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(cm256cc source_dir binary_dir)
    set_global(CM256CC_DEPENDS cm256cc)
    set_global_cache(CM256CC_FOUND ON)
    set(CM256CC_EXTERNAL ON CACHE INTERNAL "")
    # we need cm256cc/library.h
    set(CM256CC_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/cm256cc/src" CACHE INTERNAL "")
    if (WIN32)
        install(FILES "${SDRANGEL_BINARY_BIN_DIR}/cm256cc${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif (APPLE) # kept in case of relaxation of APPLE exclusion
        set(CM256CC_LIBRARIES "${binary_dir}/libcm256cc${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        install(DIRECTORY "${binary_dir}/" DESTINATION "${INSTALL_LIB_DIR}"
                FILES_MATCHING PATTERN "libcm256cc*${CMAKE_SHARED_LIBRARY_SUFFIX}")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}")
    endif ()
endif (NOT CM256CC_FOUND OR CM256CC_EXTERNAL)

if ((NOT LIBDSDCC_FOUND OR LIBDSDCC_EXTERNAL) AND (NOT LIBMBE_FOUND OR LIBMBE_EXTERNAL))
    set(USE_MBELIB OFF)
    if (WIN32)
        set(LIBMBE_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/libmbe.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(LIBMBE_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libmbe${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    endif ()
    ExternalProject_Add(mbelib
            GIT_REPOSITORY https://github.com/kasper93/mbelib.git
            GIT_TAG ${MBELIB_TAG}
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/mbelib"
            CMAKE_ARGS -DDISABLE_TEST=ON ${COMMON_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${LIBMBE_LIBRARIES}"
            INSTALL_COMMAND ""
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(mbelib source_dir binary_dir)
    set(USE_MBELIB ON)
    set_global(DSDCC_DEPENDS mbelib)
    set_global_cache(LIBMBE_FOUND ON)
    set(LIBMBE_EXTERNAL ON CACHE INTERNAL "")
    set(LIBMBE_INCLUDE_DIR "${source_dir}" CACHE INTERNAL "")
    if (WIN32)
        install(FILES "${SDRANGEL_BINARY_BIN_DIR}/libmbe${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif (APPLE)
        set(LIBMBE_LIBRARIES "${binary_dir}/libmbe${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        install(DIRECTORY "${binary_dir}/" DESTINATION "${INSTALL_LIB_DIR}"
                FILES_MATCHING PATTERN "libmbe*${CMAKE_SHARED_LIBRARY_SUFFIX}")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}")
    endif ()
endif ((NOT LIBDSDCC_FOUND OR LIBDSDCC_EXTERNAL) AND (NOT LIBMBE_FOUND OR LIBMBE_EXTERNAL))

if (NOT LIBSERIALDV_FOUND OR LIBSERIALDV_EXTERNAL)
    # Works on MacOS with a dummy serialDV
    if (WIN32)
        set(LIBSERIALDV_LIBRARY "${SDRANGEL_BINARY_LIB_DIR}/serialdv.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(LIBSERIALDV_LIBRARY "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libserialdv${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    endif ()
    ExternalProject_Add(serialdv
            GIT_REPOSITORY https://github.com/f4exb/serialDV.git
            GIT_TAG ${SERIALDV_TAG}
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/serialdv"
            CMAKE_ARGS -DBUILD_TOOL=OFF ${COMMON_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${LIBSERIALDV_LIBRARY}"
            INSTALL_COMMAND ""
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(serialdv source_dir binary_dir)
    set_global_cache(LIBSERIALDV_FOUND ON)
    set(LIBSERIALDV_EXTERNAL ON CACHE INTERNAL "")
    set(LIBSERIALDV_INCLUDE_DIR "${source_dir}" CACHE INTERNAL "")
    if (WIN32)
        makeCopyDir("${source_dir}" "${source_dir}/dsp" serialdv)
        install(FILES "${SDRANGEL_BINARY_BIN_DIR}/serialdv${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif (LINUX)
        # because sdrbase/dsp/dvserialworker.h use dsp/dvcontroller.h
        # so we need a link
        makeLink("${source_dir}" "${source_dir}/dsp" serialdv)
    elseif (APPLE)
        set(LIBSERIALDV_LIBRARY "${binary_dir}/libserialdv${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        # because sdrbase/dsp/dvserialworker.h use dsp/dvcontroller.h
        # so we need a link
        makeLink("${source_dir}" "${source_dir}/dsp" serialdv)
        install(DIRECTORY "${binary_dir}/" DESTINATION "${INSTALL_LIB_DIR}"
                FILES_MATCHING PATTERN "libserialdv*${CMAKE_SHARED_LIBRARY_SUFFIX}")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}")
    endif ()
endif (NOT LIBSERIALDV_FOUND OR LIBSERIALDV_EXTERNAL)

if ((NOT LIBDSDCC_FOUND OR LIBDSDCC_EXTERNAL) AND LIBMBE_FOUND)
    if (WIN32)
        set(LIBDSDCC_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/dsdcc.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(LIBDSDCC_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libdsdcc${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    endif ()
    ExternalProject_Add(dsdcc
            GIT_REPOSITORY https://github.com/f4exb/dsdcc.git
            GIT_TAG ${DSDCC_TAG}
            DEPENDS ${DSDCC_DEPENDS}
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/dsdcc"
            CMAKE_ARGS ${COMMON_CMAKE_ARGS}
            -DBUILD_TOOL=OFF -DUSE_MBELIB=${USE_MBELIB}
            -DLIBMBE_INCLUDE_DIR=${LIBMBE_INCLUDE_DIR}
            -DLIBMBE_LIBRARY=${LIBMBE_LIBRARIES}
            BUILD_BYPRODUCTS "${LIBDSDCC_LIBRARIES}"
            INSTALL_COMMAND ""
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(dsdcc source_dir binary_dir)
    set_global_cache(LIBDSDCC_FOUND ON)
    set(LIBDSDCC_EXTERNAL ON CACHE INTERNAL "")
    set(LIBDSDCC_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/dsdcc/src" CACHE INTERNAL "")
    if (WIN32)
        install(FILES "${SDRANGEL_BINARY_BIN_DIR}/dsdcc${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif (APPLE)
        set(LIBDSDCC_LIBRARIES "${binary_dir}/libdsdcc${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        install(DIRECTORY "${binary_dir}/" DESTINATION "${INSTALL_LIB_DIR}"
                FILES_MATCHING PATTERN "libdsdcc*${CMAKE_SHARED_LIBRARY_SUFFIX}")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/")
    endif ()
endif ((NOT LIBDSDCC_FOUND OR LIBDSDCC_EXTERNAL) AND LIBMBE_FOUND)

if (LINUX AND (NOT LIBSIGMF_FOUND OR LIBSIGMF_EXTERNAL))
    set(LIBSIGMF_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/liblibsigmf${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    ExternalProject_Add(libsigmf
            GIT_REPOSITORY https://github.com/f4exb/libsigmf.git
            GIT_TAG ${LIBSIGMF_TAG}
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/libsigmf"
            CMAKE_ARGS ${COMMON_CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            BUILD_BYPRODUCTS "${LIBSIGMF_LIBRARIES}"
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(libsigmf install_dir)
    set_global(LIBSIGMF_DEPENDS libsigmf)
    set_global_cache(LIBSIGMF_FOUND ON)
    set(LIBSIGMF_EXTERNAL ON CACHE INTERNAL "")
    set(LIBSIGMF_INCLUDE_DIR "${install_dir}/include" CACHE INTERNAL "")
endif (LINUX AND (NOT LIBSIGMF_FOUND OR LIBSIGMF_EXTERNAL))

# For APT demodulator
if(ENABLE_CHANNELRX_DEMODAPT)
    if (WIN32)
        set(APT_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/apt.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(APT_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libapt${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    endif()
    ExternalProject_Add(apt
            GIT_REPOSITORY https://github.com/srcejon/aptdec.git
            GIT_TAG libaptdec
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/apt"
            CMAKE_ARGS ${COMMON_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${APT_LIBRARIES}"
            INSTALL_COMMAND ""
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(apt source_dir binary_dir)
    set(APT_DEPENDS apt CACHE INTERNAL "")
    set_global_cache(APT_FOUND ON)
    set(APT_EXTERNAL ON CACHE INTERNAL "")
    set(APT_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/apt/src/apt/src" CACHE INTERNAL "")
    if (WIN32)
        install(FILES "${SDRANGEL_BINARY_BIN_DIR}/apt${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif (APPLE)
        set(APT_LIBRARIES "${binary_dir}/libapt${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        install(DIRECTORY "${binary_dir}/" DESTINATION "${INSTALL_LIB_DIR}"
                FILES_MATCHING PATTERN "libapt*${CMAKE_SHARED_LIBRARY_SUFFIX}")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/")
    endif ()
endif ()

# For Satellite Tracker feature and APT demod
if(ENABLE_FEATURE_SATELLITETRACKER OR ENABLE_CHANNELRX_DEMODAPT)
    # No tags for this in github - but doesn't change often
    # Fails to build with CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON on Windows
    if (WIN32)
        set(SGP4_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/sgp4s.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(SGP4_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libsgp4s${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    endif()
    ExternalProject_Add(sgp4
            GIT_REPOSITORY https://github.com/dnwrnr/sgp4.git
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/sgp4"
            CMAKE_ARGS ${COMMON_CMAKE_ARGS}
            -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF
            BUILD_BYPRODUCTS "${SGP4_LIBRARIES}"
            INSTALL_COMMAND ""
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(sgp4 source_dir binary_dir)
    set(SGP4_DEPENDS sgp4 CACHE INTERNAL "")
    set_global_cache(SGP4_FOUND ON)
    set(SGP4_EXTERNAL ON CACHE INTERNAL "")
    set(SGP4_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/sgp4/src/sgp4/libsgp4" CACHE INTERNAL "")
    if (WIN32)
        install(FILES "${SDRANGEL_BINARY_BIN_DIR}/sgp4s${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif (APPLE)
        set(SGP4_LIBRARIES "${binary_dir}/libsgp4/libsgp4s${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        install(DIRECTORY "${binary_dir}/libsgp4" DESTINATION "${INSTALL_LIB_DIR}"
                FILES_MATCHING PATTERN "libsgp4s*${CMAKE_SHARED_LIBRARY_SUFFIX}")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/libsgp4")
    endif ()
endif ()

if(ENABLE_CHANNELRX_REMOTETCPSINK)
    if (WIN32)
        set(FLAC_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/FLAC.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(FLAC_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/flac/src/flac-build/src/libFLAC/libFLAC${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    elseif (APPLE)
        set(FLAC_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/flac/src/flac-build/src/libFLAC/libFLAC${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    elseif (EMSCRIPTEN)
        set(FLAC_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/flac/src/flac-build/src/libFLAC/libFLAC.a" CACHE INTERNAL "")
    endif()
    ExternalProject_Add(flac
            GIT_REPOSITORY https://github.com/xiph/flac.git
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/flac"
            CMAKE_ARGS ${COMMON_CMAKE_ARGS} -DINSTALL_MANPAGES=OFF -D=BUILD_SHARED_LIBS=ON -DWITH_FORTIFY_SOURCE=OFF -DWITH_STACK_PROTECTOR=PFF -DBUILD_PROGRAMS=OFF -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF -DWITH_OGG=OFF -DBUILD_DOCS=OFF
            BUILD_BYPRODUCTS "${FLAC_LIBRARIES}"
            INSTALL_COMMAND ""
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(flac source_dir binary_dir)
    set_global(FLAC_DEPENDS flac)
    set_global_cache(FLAC_FOUND ON)
    set(FLAC_EXTERNAL ON CACHE INTERNAL "")
    set(FLAC_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/flac/src/flac/include" CACHE INTERNAL "")
    if (WIN32)
        install(FILES "${SDRANGEL_BINARY_BIN_DIR}/FLAC${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif (APPLE)
        install(DIRECTORY "${EXTERNAL_BUILD_LIBRARIES}/flac/src/flac-build/src/libFLAC" DESTINATION "${INSTALL_LIB_DIR}"
                FILES_MATCHING PATTERN "libFLAC*${CMAKE_SHARED_LIBRARY_SUFFIX}")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${EXTERNAL_BUILD_LIBRARIES}/flac/src/flac-build/src/libFLAC")
    endif ()
endif ()

# For Morse Decoder feature
if(ENABLE_FEATURE_MORSEDECODER)
    if (WIN32)
        set(GGMORSE_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/ggmorse.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(GGMORSE_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libggmorse${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    endif()
    ExternalProject_Add(ggmorse
        GIT_REPOSITORY https://github.com/ggerganov/ggmorse.git
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/ggmorse"
        CMAKE_ARGS ${COMMON_CMAKE_ARGS}
        -DGGMORSE_BUILD_TESTS=OFF
        -DGGMORSE_BUILD_EXAMPLES=OFF
        BUILD_BYPRODUCTS "${GGMORSE_LIBRARIES}"
        INSTALL_COMMAND ""
        TEST_COMMAND ""
        )
    ExternalProject_Get_Property(ggmorse source_dir binary_dir)
    set(GGMORSE_DEPENDS ggmorse CACHE INTERNAL "")
    set_global_cache(GGMORSE_FOUND ON)
    set(GGMORSE_EXTERNAL ON CACHE INTERNAL "")
    set(GGMORSE_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/ggmorse/src/ggmorse/include" CACHE INTERNAL "")
    if (WIN32)
        install(FILES "${SDRANGEL_BINARY_BIN_DIR}/ggmorse${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif (APPLE)
        set(GGMORSE_LIBRARIES "${binary_dir}/src/libggmorse${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        install(DIRECTORY "${binary_dir}/src" DESTINATION "${INSTALL_LIB_DIR}"
                FILES_MATCHING PATTERN "libggmorse*${CMAKE_SHARED_LIBRARY_SUFFIX}")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/src")
    endif ()
endif()

# VkFFT (header only library)
ExternalProject_Add(vkfft
        GIT_REPOSITORY https://github.com/DTolm/VkFFT.git
        GIT_TAG v1.3.1
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/vkfft"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        BUILD_BYPRODUCTS ""
        INSTALL_COMMAND ""
        TEST_COMMAND ""
        )
ExternalProject_Get_Property(vkfft source_dir)
set(VKFFT_INCLUDE_DIR "${source_dir}/vkFFT" CACHE INTERNAL "")

# requirements needed by many packages on windows
if (WIN32)
    ExternalProject_Add(pthreads4w
            GIT_REPOSITORY https://git.code.sf.net/p/pthreads4w/code
            GIT_TAG ${PTHREADS4W_TAG}
            PREFIX "${EXTERNAL_BUILD_LIBRARIES}/pthreads4w"
            PATCH_COMMAND ${CMAKE_COMMAND} -E touch "<SOURCE_DIR>/COPYING.LIB"
            BUILD_IN_SOURCE 1
            CONFIGURE_COMMAND ""
            BUILD_COMMAND nmake VC
            INSTALL_COMMAND ""
            TEST_COMMAND ""
            )
    ExternalProject_Get_Property(pthreads4w source_dir binary_dir)
    set_global(PTHREADS4W_DEPENDS pthreads4w)
    set_global_cache(PTHREADS4W_FOUND ON)
    set(PTHREADS4W_EXTERNAL ON CACHE INTERNAL "")
    set(PTHREADS4W_INCLUDE_DIR "${source_dir}" CACHE INTERNAL "")
    set(PTHREADS4W_LIBRARIES "${source_dir}/pthreadVC2.lib" CACHE INTERNAL "")
    makeCopy("${source_dir}/pthreadVC2${CMAKE_SHARED_LIBRARY_SUFFIX}" "${SDRANGEL_BINARY_BIN_DIR}/pthreadVC2${CMAKE_SHARED_LIBRARY_SUFFIX}" pthreads4w)
    # needed by bladerf
    makeCopy("${source_dir}/pthreadVC2${CMAKE_SHARED_LIBRARY_SUFFIX}" "${source_dir}/dll/x64/pthreadVC2${CMAKE_SHARED_LIBRARY_SUFFIX}" pthreads4w)
    install(FILES "${source_dir}/pthreadVC2${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
endif (WIN32)

# For DAB demodulator
if (ZLIB_FOUND AND FAAD_FOUND AND ENABLE_CHANNELRX_DEMODDAB)
    if (WIN32)
        set(DAB_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/dab_lib.lib" CACHE INTERNAL "")
    elseif (LINUX)
        set(DAB_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libdab_lib${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    endif()
    if (WIN32)
        ExternalProject_Add(dab
                GIT_REPOSITORY https://github.com/srcejon/dab-cmdline.git
                GIT_TAG msvc
                DEPENDS ${PTHREADS4W_DEPENDS}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/dab"
                SOURCE_SUBDIR "library"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                        -DFFTW3F_INCLUDE_DIRS=${FFTW3F_INCLUDE_DIRS}
                        -DFFTW3F_LIBRARIES=${FFTW3F_LIBRARIES}
                        -DFAAD_INCLUDE_DIR=${FAAD_INCLUDE_DIR}
                        -DFAAD_LIBRARY=${FAAD_LIBRARY}
                        -DZLIB_INCLUDE_DIR=${ZLIB_INCLUDE_DIRS}
                        -DZLIB_LIBRARY=${ZLIB_LIBRARIES}
                        -DPTHREADS=${PTHREADS4W_LIBRARIES}
                BUILD_BYPRODUCTS "${DAB_LIBRARIES}"
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
    else(WIN32)
        ExternalProject_Add(dab
                GIT_REPOSITORY https://github.com/srcejon/dab-cmdline.git
                GIT_TAG msvc
                DEPENDS ${ZLIB_DEPENDS} ${FAAD_DEPENDS}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/dab"
                SOURCE_SUBDIR "library"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                        -DFFTW3F_INCLUDE_DIRS=${FFTW3F_INCLUDE_DIRS}
                        -DFFTW3F_LIBRARIES=${FFTW3F_LIBRARIES}
                        -DFAAD_INCLUDE_DIR=${FAAD_INCLUDE_DIR}
                        -DFAAD_LIBRARY=${FAAD_LIBRARY}
                        -DZLIB_INCLUDE_DIR=${ZLIB_INCLUDE_DIRS}
                        -DZLIB_LIBRARY=${ZLIB_LIBRARIES}
                BUILD_BYPRODUCTS "${DAB_LIBRARIES}"
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
    endif(WIN32)
    ExternalProject_Get_Property(dab source_dir binary_dir)
    set_global(DAB_DEPENDS dab)
    set_global_cache(DAB_FOUND ON)
    set(DAB_EXTERNAL ON CACHE INTERNAL "")
    set(DAB_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/dab/src/dab" CACHE INTERNAL "")
    if (WIN32)
        install(FILES "${SDRANGEL_BINARY_BIN_DIR}/dab_lib${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif (APPLE)
        set(DAB_LIBRARIES "${binary_dir}/libdab_lib${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
        install(DIRECTORY "${binary_dir}/" DESTINATION "${INSTALL_LIB_DIR}"
                FILES_MATCHING PATTERN "libdab_lib*${CMAKE_SHARED_LIBRARY_SUFFIX}")
        set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/")
    endif ()
else()
    message(STATUS "Can't build DAB library as missing zlib or faad")
endif()

# Device interface libraries.
if (LINUX)
    # apt install liblimesuite-dev (only on ubuntu 18.04). Can be picky on version though so let's build it.
    # needs pkgconfig and libusb
    if (ENABLE_LIMESUITE AND (NOT LIMESUITE_FOUND OR LIMESUITE_EXTERNAL))
        set_global_cache(LIMESUITE_FOUND ON)
        set(LIMESUITE_EXTERNAL ON CACHE INTERNAL "")
        set(LIMESUITE_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/limesuite/src/limesuite/src" CACHE INTERNAL "")
        set(LIMESUITE_LIBRARY "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libLimeSuite.so" CACHE INTERNAL "")
        ExternalProject_Add(limesuite
                GIT_REPOSITORY https://github.com/myriadrf/LimeSuite.git
                GIT_TAG ${LIMESUITE_TAG}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/limesuite"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                  -DLIME_SUITE_EXTVER=release
                  -DENABLE_GUI=OFF
                  -DENABLE_NOVENARF7=OFF
                  -DENABLE_SOAPY_LMS7=OFF
                  -DENABLE_OCTAVE=OFF
                  -DENABLE_QUICKTEST=OFF
                  -DENABLE_EXAMPLES=OFF
                  -DENABLE_LIME_UTIL=OFF
                  -DENABLE_SIMD_FLAGS=SSE3
                BUILD_BYPRODUCTS "${LIMESUITE_LIBRARY}"
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
        ExternalProject_Get_Property(limesuite source_dir)
        makeCopyFile("${source_dir}/src/limeRFE/limeRFE.h" "${source_dir}/src/lime/limeRFE.h" limesuite)
    endif ()

    # apt install libbladerf-dev
    # needs pkgconfig and libusb
    if (ENABLE_BLADERF AND (NOT LIBBLADERF_FOUND OR LIBBLADERF_EXTERNAL))
        set_global_cache(LIBBLADERF_FOUND ON)
        set(LIBBLADERF_EXTERNAL ON CACHE INTERNAL "")
        set(LIBBLADERF_INCLUDE_DIRS "${EXTERNAL_BUILD_LIBRARIES}/bladerf/src/bladerf/host/libraries/libbladeRF/include" CACHE INTERNAL "")
        set(LIBBLADERF_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/bladerf/src/bladerf-build/output/libbladeRF.so.2" CACHE INTERNAL "")
        ExternalProject_Add(bladerf
                GIT_REPOSITORY https://github.com/Nuand/bladeRF.git
                GIT_TAG ${BLADERF_TAG}
                GIT_SUBMODULES
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/bladerf"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                SOURCE_SUBDIR host/
                BUILD_BYPRODUCTS "${LIBBLADERF_LIBRARIES}"
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
        install(FILES "${LIBBLADERF_LIBRARIES}" DESTINATION "${INSTALL_LIB_DIR}")
    endif ()

    # apt install libiio-dev
    # needs pkgconfig, libusb, libxml2, lzma, xz, libiconv
    if (ENABLE_IIO AND (NOT LIBIIO_FOUND OR LIBIIO_EXTERNAL))
        set_global_cache(LIBIIO_FOUND ON)
        set(LIBIIO_EXTERNAL ON CACHE INTERNAL "")
        set(LIBIIO_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/libiio/src/libiio" CACHE INTERNAL "")
        set(LIBIIO_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libiio.so" CACHE INTERNAL "")
        ExternalProject_Add(libiio
                GIT_REPOSITORY https://github.com/analogdevicesinc/libiio.git
                GIT_TAG ${LIBIIO_TAG}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/libiio"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                  -DOSX_PACKAGE=OFF
                  -DWITH_DOC=OFF
                  -DWITH_MATLAB_BINDINGS_API=OFF
                  -DWITH_TESTS=OFF
                BUILD_BYPRODUCTS "${LIBIIO_LIBRARIES}"
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
    endif ()

    # apt install libairspyhf-dev (only on ubuntu 18.04)
    # needs libusb and pkgconfig
    if (ENABLE_AIRSPYHF AND (NOT LIBAIRSPYHF_FOUND OR LIBAIRSPYHF_EXTERNAL))
        set_global_cache(LIBAIRSPYHF_FOUND ON)
        set(LIBAIRSPYHF_EXTERNAL ON CACHE INTERNAL "")
        set(LIBAIRSPYHF_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libairspyhf.so" CACHE INTERNAL "")
        ExternalProject_Add(airspyhf
                GIT_REPOSITORY https://github.com/airspy/airspyhf.git
                GIT_TAG ${AIRSPYHF_TAG}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/airspyhf"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                BUILD_BYPRODUCTS "${LIBAIRSPYHF_LIBRARIES}"
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
        ExternalProject_Get_Property(airspyhf source_dir)
        set(LIBAIRSPYHF_INCLUDE_DIR "${source_dir}/.." CACHE INTERNAL "")
        makeLink("${source_dir}/libairspyhf/src" "${source_dir}/../libairspyhf" airspyhf)
    endif ()

    # needs pkgconfig, libusb, autoconf, automake and libtool
    if (ENABLE_PERSEUS AND (NOT LIBPERSEUS_FOUND OR LIBPERSEUS_EXTERNAL))
        set_global_cache(LIBPERSEUS_FOUND ON)
        set(LIBPERSEUS_EXTERNAL ON CACHE INTERNAL "")
        set(LIBPERSEUS_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/perseus/src/perseus" CACHE INTERNAL "")
        set(LIBPERSEUS_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libperseus-sdr.so" CACHE INTERNAL "")
        ExternalProject_Add(perseus
                GIT_REPOSITORY https://github.com/f4exb/libperseus-sdr.git
                GIT_TAG ${PERSEUS_TAG}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/perseus"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                BUILD_BYPRODUCTS "${LIBPERSEUS_LIBRARIES}"
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
    endif ()

    # apt install librtlsdr-dev
    # needs pkgconfig and libusb
    if (ENABLE_RTLSDR AND (NOT LIBRTLSDR_FOUND OR LIBRTLSDR_EXTERNAL))
        set_global_cache(LIBRTLSDR_FOUND ON)
        set(LIBRTLSDR_EXTERNAL ON CACHE INTERNAL "")
        set(LIBRTLSDR_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/rtlsdr/src/rtlsdr/include" CACHE INTERNAL "")
        set(LIBRTLSDR_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/librtlsdr.so" CACHE INTERNAL "")
        ExternalProject_Add(rtlsdr
                GIT_REPOSITORY https://github.com/osmocom/rtl-sdr.git
                GIT_TAG ${RTLSDR_TAG}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/rtlsdr"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS} -DDETACH_KERNEL_DRIVER=ON -DINSTALL_UDEV_RULES=${RTLSDR_UDEV}
                BUILD_BYPRODUCTS "${LIBRTLSDR_LIBRARIES}"
                BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG> -- rtlsdr
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
    endif ()

    # needs pkgconfig and libusb
    if (ENABLE_MIRISDR AND (NOT LIBMIRISDR_FOUND OR LIBMIRISDR_EXTERNAL))
        set_global_cache(LIBMIRISDR_FOUND ON)
        set(LIBMIRISDR_EXTERNAL ON CACHE INTERNAL "")
        set(LIBMIRISDR_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/libmirisdr/src/libmirisdr/include" CACHE INTERNAL "")
        set(LIBMIRISDR_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/libmirisdr.so" CACHE INTERNAL "")
        ExternalProject_Add(libmirisdr
                GIT_REPOSITORY https://github.com/f4exb/libmirisdr-4.git
                GIT_TAG ${MIRISDR_TAG}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/libmirisdr"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                BUILD_BYPRODUCTS "${LIBMIRISDR_LIBRARIES}"
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
    endif ()

endif (LINUX)

if (WIN32 OR APPLE)
    if (ENABLE_RTLSDR)
        set(RTLSDR_LIBUSB_INCLUDE "${LIBUSB_INCLUDE_DIR}")
        if (WIN32)
            set(LIBRTLSDR_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/rtlsdr.lib" CACHE INTERNAL "")
            set(RTLSDR_LIBUSB_INCLUDE "${LIBUSB_INCLUDE_DIR}/libusb-1.0")
        endif ()
        # Disable pkg-config to allow LIBUSB_INCLUDE_DIRS to be used
        set(DISABLE_PKGCONFIG "-DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=ON")
        # If we want to use https://github.com/librtlsdr/librtlsdr instead of https://github.com/osmocom/rtl-sdr.git
        #  - Don't use DISABLE_PKGCONFIG
        #  - Use -DLIBUSB_INCLUDE_DIR instead of -DLIBUSB_INCLUDE_DIRS
        #  - Build target is rtl_sdr rather than rtlsdr
        #  - Use tag development otherwise it will not build with MSVC
        ExternalProject_Add(rtlsdr
                GIT_REPOSITORY https://github.com/osmocom/rtl-sdr.git
                GIT_TAG ${RTLSDR_TAG}
                DEPENDS ${PTHREADS4W_DEPENDS} ${LIBUSB_DEPENDS}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/rtlsdr"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                -DINSTALL_UDEV_RULES=${RTLSDR_UDEV}
                -DLIBUSB_LIBRARIES=${LIBUSB_LIBRARIES}
                -DLIBUSB_INCLUDE_DIRS=${RTLSDR_LIBUSB_INCLUDE}
                -DTHREADS_PTHREADS_INCLUDE_DIR=${PTHREADS4W_INCLUDE_DIR}
                -DTHREADS_PTHREADS_WIN32_LIBRARY=${PTHREADS4W_LIBRARIES}
                ${DISABLE_PKGCONFIG}
                BUILD_BYPRODUCTS "${LIBRTLSDR_LIBRARIES}"
                BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG> -- rtlsdr
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
        ExternalProject_Get_Property(rtlsdr source_dir binary_dir)
        set_global_cache(LIBRTLSDR_FOUND ON)
        set(LIBRTLSDR_EXTERNAL ON CACHE INTERNAL "")
        set(LIBRTLSDR_INCLUDE_DIR "${source_dir}/include" CACHE INTERNAL "")
        if (WIN32)
            install(FILES "${SDRANGEL_BINARY_BIN_DIR}/rtlsdr${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
        elseif (APPLE)
            set(LIBRTLSDR_LIBRARIES "${binary_dir}/src/librtlsdr${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
            install(DIRECTORY "${binary_dir}/src/" DESTINATION "${INSTALL_LIB_DIR}"
                    FILES_MATCHING PATTERN "librtlsdr*${CMAKE_SHARED_LIBRARY_SUFFIX}")
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/src/")
        endif ()
    endif (ENABLE_RTLSDR)

    if (ENABLE_LIMESUITE)
        # needs pkgconfig, libusb
        # We have to install this, as makeCopy("${source_dir}/src/limeRFE/limeRFE.h" "${source_dir}/src/lime/limeRFE.h" limesuite)
        # causes git stash to fail
        if (WIN32)
            set(LIMESUITE_LIBRARY "${SDRANGEL_BINARY_LIB_DIR}/LimeSuite.lib" CACHE INTERNAL "")
        endif ()
        ExternalProject_Add(limesuite
                GIT_REPOSITORY https://github.com/myriadrf/LimeSuite.git
                GIT_TAG ${LIMESUITE_TAG}
                DEPENDS ${LIBUSB_DEPENDS}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/limesuite"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                -DLIME_SUITE_EXTVER=release
                -DENABLE_GUI=OFF
                -DENABLE_NOVENARF7=OFF
                -DENABLE_SOAPY_LMS7=OFF
                -DENABLE_OCTAVE=OFF
                -DENABLE_QUICKTEST=OFF
                -DENABLE_EXAMPLES=OFF
                -DENABLE_LIME_UTIL=ON
                -DENABLE_SIMD_FLAGS=SSE3
                -DENABLE_FX3=ON
                -DFX3_SDK_PATH=${FX3SDK_DIR}
                -DLIBUSB_1_LIBRARY=${LIBUSB_LIBRARIES}
                -DLIBUSB_1_INCLUDE_DIR=${LIBUSB_INCLUDE_DIR}
                BUILD_BYPRODUCTS "${LIMESUITE_LIBRARY}"
                TEST_COMMAND ""
                )
        ExternalProject_Get_Property(limesuite source_dir binary_dir install_dir)
        set_global_cache(LIMESUITE_FOUND ON)
        set(LIMESUITE_EXTERNAL ON CACHE INTERNAL "")
        set(LIMESUITE_INCLUDE_DIR "${install_dir}/include" CACHE INTERNAL "")
        if (WIN32)
            install(FILES "${SDRANGEL_BINARY_BIN_DIR}/LimeSuite${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
            makeCopy("${source_dir}/src/limeRFE/limeRFE.h" "${source_dir}/src/lime/limeRFE.h" limesuite)
        elseif (APPLE)
            set(LIMESUITE_LIBRARY "${install_dir}/lib/libLimeSuite${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
            install(DIRECTORY "${intstall_dir}/lib/" DESTINATION "${INSTALL_LIB_DIR}"
                    FILES_MATCHING PATTERN "libLimeSuite*${CMAKE_SHARED_LIBRARY_SUFFIX}")
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${install_dir}/lib/")
        endif ()
    endif (ENABLE_LIMESUITE)

    if (ENABLE_SOAPYSDR)
        if (WIN32)
            set(SOAPYSDR_LINK_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/SoapySDR.lib" CACHE INTERNAL "")
        endif ()
        ExternalProject_Add(soapysdr
                GIT_REPOSITORY https://github.com/pothosware/SoapySDR.git
                GIT_TAG ${SOAPYSDR_TAG}
                DEPENDS ${LIBUSB_DEPENDS}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/soapysdr"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                -DENABLE_PYTHON=OFF
                -DENABLE_PYTHON3=OFF
                -DENABLE_TESTS=OFF
                BUILD_BYPRODUCTS "${SOAPYSDR_LINK_LIBRARIES}"
                BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG> -- SoapySDR
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
        ExternalProject_Get_Property(soapysdr source_dir binary_dir)
        set_global_cache(SOAPYSDR_FOUND ON)
        set(SOAPYSDR_EXTERNAL ON CACHE INTERNAL "")
        set(SOAPYSDR_INCLUDE_DIRS "${source_dir}/include" CACHE INTERNAL "")
        if (WIN32)
            install(FILES "${SDRANGEL_BINARY_BIN_DIR}/SoapySDR${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
            install(DIRECTORY "${SDRANGEL_BINARY_LIB_DIR}/SoapySDR" DESTINATION "${INSTALL_LIB_DIR}/lib")
        elseif (APPLE)
            set(SOAPYSDR_LINK_LIBRARIES "${binary_dir}/lib/libSoapySDR${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
            install(DIRECTORY "${binary_dir}/lib/" DESTINATION "${INSTALL_LIB_DIR}"
                    FILES_MATCHING PATTERN "libSoapySDR*${CMAKE_SHARED_LIBRARY_SUFFIX}")
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/lib/")
        endif ()
    endif (ENABLE_SOAPYSDR)

    if (ENABLE_AIRSPY)
        set(AIRSPY_LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR})
        if (WIN32)
            set(LIBAIRSPY_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/airspy.lib" CACHE INTERNAL "")
            set(AIRSPY_LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR}/libusb-1.0)
        endif ()
        # needs libusb, pthreads
        # We need to set CMAKE_INSTALL_NAME_DIR, otherwise path to libs is set to install path of /usr/local/
        # but as we don't install, macdeployqt will fail to find them
        ExternalProject_Add(airspy
                GIT_REPOSITORY https://github.com/airspy/airspyone_host
                GIT_TAG ${AIRSPY_TAG}
                DEPENDS ${PTHREADS4W_DEPENDS} ${LIBUSB_DEPENDS}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/airspy"
                SOURCE_SUBDIR "libairspy"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                -DLIBUSB_LIBRARIES=${LIBUSB_LIBRARIES}
                -DLIBUSB_INCLUDE_DIR=${AIRSPY_LIBUSB_INCLUDE_DIR}
                -DTHREADS_PTHREADS_INCLUDE_DIR=${PTHREADS4W_INCLUDE_DIR}
                -DTHREADS_PTHREADS_WIN32_LIBRARY=${PTHREADS4W_LIBRARIES}
                -DCMAKE_INSTALL_NAME_DIR=<BINARY_DIR>/src
                BUILD_BYPRODUCTS "${LIBAIRSPY_LIBRARIES}"
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
        if (WIN32)
            add_dependencies(airspy pthreads4w)
        endif ()
        ExternalProject_Get_Property(airspy source_dir binary_dir)
        set_global_cache(LIBAIRSPY_FOUND ON)
        set(LIBAIRSPY_EXTERNAL ON CACHE INTERNAL "")
        set(LIBAIRSPY_INCLUDE_DIR "${source_dir}/libairspy/src" CACHE INTERNAL "")
        if (WIN32)
            makeCopyDir("${source_dir}/libairspy/src" "${source_dir}/libairspy/src/libairspy" airspy)
            makeCopy("${binary_dir}/../airspy-tools/src/airspy${CMAKE_SHARED_LIBRARY_SUFFIX}" "${SDRANGEL_BINARY_BIN_DIR}/airspy${CMAKE_SHARED_LIBRARY_SUFFIX}" airspy)
            install(FILES "${SDRANGEL_BINARY_BIN_DIR}/airspy${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
        elseif (APPLE)
            set(LIBAIRSPY_LIBRARIES "${binary_dir}/src/libairspy${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
            makeLink("${source_dir}/libairspy/src" "${source_dir}/libairspy/src/libairspy" airspy)
            install(DIRECTORY "${binary_dir}/libairspy/src/" DESTINATION "${INSTALL_LIB_DIR}"
                    FILES_MATCHING PATTERN "libairspy*${CMAKE_SHARED_LIBRARY_SUFFIX}")
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/src")
        endif ()
    endif (ENABLE_AIRSPY)

    if (ENABLE_AIRSPYHF)
        set(AIRSPYHF_LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR})
        if (WIN32)
            set(LIBAIRSPYHF_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/airspyhf.lib" CACHE INTERNAL "")
            set(AIRSPYHF_LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR}/libusb-1.0)
        endif ()
        # needs pkgconfig, libusb, pthreads
        ExternalProject_Add(airspyhf
                GIT_REPOSITORY https://github.com/airspy/airspyhf.git
                GIT_TAG ${AIRSPYHF_TAG}
                DEPENDS ${PTHREADS4W_DEPENDS} ${LIBUSB_DEPENDS}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/airspyhf"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                -DLIBUSB_LIBRARIES=${LIBUSB_LIBRARIES}
                -DLIBUSB_INCLUDE_DIR=${AIRSPYHF_LIBUSB_INCLUDE_DIR}
                -DTHREADS_PTHREADS_INCLUDE_DIR=${PTHREADS4W_INCLUDE_DIR}
                -DTHREADS_PTHREADS_WIN32_LIBRARY=${PTHREADS4W_LIBRARIES}
                -DCMAKE_INSTALL_NAME_DIR=<BINARY_DIR>/libairspyhf/src
                BUILD_BYPRODUCTS "${LIBAIRSPYHF_LIBRARIES}"
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
        if (WIN32)
            add_dependencies(airspyhf pthreads4w)
        endif()
        ExternalProject_Get_Property(airspyhf source_dir binary_dir)
        set_global_cache(LIBAIRSPYHF_FOUND ON)
        set(LIBAIRSPYHF_EXTERNAL ON CACHE INTERNAL "")
        set(LIBAIRSPYHF_INCLUDE_DIR "${source_dir}/libairspyhf/src" CACHE INTERNAL "")
        if (WIN32)
            makeCopyDir("${source_dir}/libairspyhf/src" "${source_dir}/libairspyhf/src/libairspyhf" airspyhf)
            install(FILES "${SDRANGEL_BINARY_BIN_DIR}/airspyhf${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
        elseif (APPLE)
            set(LIBAIRSPYHF_LIBRARIES "${binary_dir}/libairspyhf/src/libairspyhf${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
            makeLink("${source_dir}/libairspyhf/src" "${source_dir}/libairspyhf/src/libairspyhf" airspyhf)
            install(DIRECTORY "${binary_dir}/libairspyhf/src/" DESTINATION "${INSTALL_LIB_DIR}"
                    FILES_MATCHING PATTERN "libairspyhf*${CMAKE_SHARED_LIBRARY_SUFFIX}")
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/libairspyhf/src")
        endif ()
    endif (ENABLE_AIRSPYHF)

    if (ENABLE_HACKRF)
        # needs pkgconfig, libusb, fftw, pthreads
        if (WIN32)
            set(HACKRF_LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR}/libusb-1.0)
            set(LIBHACKRF_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/hackrf.lib" CACHE INTERNAL "")
        else ()
            set(HACKRF_LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR})
        endif ()
        ExternalProject_Add(hackrf
                GIT_REPOSITORY https://github.com/greatscottgadgets/hackrf.git
                GIT_TAG ${HACKRF_TAG}
                GIT_SUBMODULES ""
                DEPENDS ${PTHREADS4W_DEPENDS} ${LIBUSB_DEPENDS}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/hackrf"
                SOURCE_SUBDIR "host/libhackrf"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                -DLIBUSB_LIBRARIES=${LIBUSB_LIBRARIES}
                -DLIBUSB_INCLUDE_DIR=${HACKRF_LIBUSB_INCLUDE_DIR}
                -DTHREADS_PTHREADS_INCLUDE_DIR=${PTHREADS4W_INCLUDE_DIR}
                -DTHREADS_PTHREADS_WIN32_LIBRARY=${PTHREADS4W_LIBRARIES}
                -DFFTW_INCLUDES=${FFTW3F_INCLUDE_DIRS}
                -DFFTW_LIBRARIES=${FFTW3F_LIBRARIES}
                BUILD_BYPRODUCTS "${LIBHACKRF_LIBRARIES}"
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
        if (WIN32)
            add_dependencies(hackrf pthreads4w)
        endif()
        ExternalProject_Get_Property(hackrf source_dir binary_dir)
        set_global_cache(LIBHACKRF_FOUND ON)
        set(LIBHACKRF_EXTERNAL ON CACHE INTERNAL "")
        set(LIBHACKRF_INCLUDE_DIR "${source_dir}/host/libhackrf" CACHE INTERNAL "")
        if (WIN32)
            # include "libhackrf/hackrf.h"
            makeCopyDir("${source_dir}/host/libhackrf/src" "${source_dir}/host/libhackrf/libhackrf" hackrf)
            install(FILES "${SDRANGEL_BINARY_BIN_DIR}/hackrf${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
        elseif (APPLE)
            set(LIBHACKRF_LIBRARIES "${binary_dir}/src/libhackrf${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
            # include "libhackrf/hackrf.h"
            makeLink("${source_dir}/host/libhackrf/src" "${source_dir}/host/libhackrf/libhackrf" hackrf)
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/")
            install(DIRECTORY "${binary_dir}/src/" DESTINATION "${INSTALL_LIB_DIR}"
                    FILES_MATCHING PATTERN "libhackrf*${CMAKE_SHARED_LIBRARY_SUFFIX}")
        endif ()
    endif (ENABLE_HACKRF)

    if (ENABLE_IIO)
        if (WIN32)
            ExternalProject_Add(libxml2
                    GIT_REPOSITORY https://github.com/GNOME/libxml2.git
                    GIT_TAG ${LIBXML2_TAG}
                    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/libxml2"
                    BUILD_IN_SOURCE 1
                    CONFIGURE_COMMAND cd <SOURCE_DIR>/win32 && cscript configure.js compiler=msvc iconv=no
                    BUILD_COMMAND cd <SOURCE_DIR>/win32 && nmake -f Makefile.msvc
                    INSTALL_COMMAND ""
                    TEST_COMMAND ""
                    )
        elseif (APPLE)
            ExternalProject_Add(libxml2
                    GIT_REPOSITORY https://github.com/GNOME/libxml2.git
                    GIT_TAG ${LIBXML2_TAG}
                    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/libxml2"
                    CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                    -DBUILD_SHARED_LIBS=OFF
                    -DLIBXML2_WITH_ICONV=OFF
                    -DLIBXML2_WITH_LZMA=OFF
                    -DLIBXML2_WITH_PYTHON=OFF
                    -DLIBXML2_WITH_ZLIB=OFF
                    TEST_COMMAND ""
                    )
        endif ()
        ExternalProject_Get_Property(libxml2 source_dir binary_dir)
        set_global(LIBXML2_DEPENDS libxml2)
        set_global_cache(LIBXML2_FOUND ON)
        set(LIBXML2_EXTERNAL ON CACHE INTERNAL "")
        if (WIN32)
            set(LIBXML2_INCLUDE_DIR "${source_dir}/include" CACHE INTERNAL "")
        else ()
            set(LIBXML2_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/libxml2/include/libxml2" CACHE INTERNAL "")
        endif ()
        if (WIN32)
            set(LIBXML2_LIBRARIES "${source_dir}/win32/bin.msvc/libxml2.lib" CACHE INTERNAL "")
            set(LIBIIO_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/libiio.lib" CACHE INTERNAL "")
            makeCopy("${source_dir}/win32/bin.msvc/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}" "${SDRANGEL_BINARY_BIN_DIR}/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}" libxml2)
            install(FILES "${source_dir}/win32/bin.msvc/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
        else ()
            set(LIBXML2_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/libxml2/lib/libxml2.a" CACHE INTERNAL "")
        endif ()
        if (WIN32)
            set(IIO_LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR})
        else ()
            set(IIO_LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR}/..)
        endif ()
        # needs pkgconfig, libusb, pthreads, libxml2
        ExternalProject_Add(libiio
                GIT_REPOSITORY https://github.com/analogdevicesinc/libiio.git
                GIT_TAG ${LIBIIO_TAG}
                DEPENDS ${PTHREADS4W_DEPENDS} ${LIBXML2_DEPENDS} ${LIBUSB_DEPENDS}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/libiio"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                -DLIBUSB_LIBRARIES=${LIBUSB_LIBRARIES}
                -DLIBUSB_INCLUDE_DIR=${IIO_LIBUSB_INCLUDE_DIR}
                -DTHREADS_PTHREADS_INCLUDE_DIR=${PTHREADS4W_INCLUDE_DIR}
                -DTHREADS_PTHREADS_WIN32_LIBRARY=${PTHREADS4W_LIBRARIES}
                -DLIBXML2_INCLUDE_DIR=${LIBXML2_INCLUDE_DIR}
                -DLIBXML2_LIBRARIES=${LIBXML2_LIBRARIES}
                -DCSHARP_BINDINGS=OFF
                -DPYTHON_BINDINGS=OFF
                -DOSX_PACKAGE=OFF
                -DWITH_DOC=OFF
                -DWITH_MATLAB_BINDINGS_API=OFF
                -DWITH_TESTS=OFF
                BUILD_BYPRODUCTS "${LIBIIO_LIBRARIES}"
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
        if (WIN32)
            add_dependencies(libiio pthreads4w)
        endif()
        ExternalProject_Get_Property(libiio source_dir binary_dir)
        set_global_cache(LIBIIO_FOUND ON)
        set(LIBIIO_EXTERNAL ON CACHE INTERNAL "")
        set(LIBIIO_INCLUDE_DIR "${source_dir}" CACHE INTERNAL "")
        if (WIN32)
            install(FILES "${SDRANGEL_BINARY_BIN_DIR}/libiio${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
        elseif (APPLE)
            set(LIBIIO_LIBRARIES "${binary_dir}/libiio${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
            install(DIRECTORY "${binary_dir}/" DESTINATION "${INSTALL_LIB_DIR}"
                    FILES_MATCHING PATTERN "libiio*${CMAKE_SHARED_LIBRARY_SUFFIX}")
            makeCopy("${binary_dir}/iio.framework/iio" "${binary_dir}/libiio${CMAKE_SHARED_LIBRARY_SUFFIX}" libiio)
            add_custom_command(TARGET libiio POST_BUILD COMMAND install_name_tool -id "${binary_dir}/libiio${CMAKE_SHARED_LIBRARY_SUFFIX}" "${binary_dir}/libiio${CMAKE_SHARED_LIBRARY_SUFFIX}")
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}")
        endif ()
    endif (ENABLE_IIO)

    if (ENABLE_BLADERF)
        # needs pkgconfig, libusb, pthreads
        if (WIN32)
            set(LIBBLADERF_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/bladeRF.lib" CACHE INTERNAL "")
            set(LIBBLADERF_LIBUSB_HEADER_FILE ${LIBUSB_INCLUDE_DIR}/libusb-1.0/libusb.h)
        else ()
            set(LIBBLADERF_LIBUSB_HEADER_FILE ${LIBUSB_INCLUDE_DIR}/libusb.h)
        endif ()
        ExternalProject_Add(bladerf
                GIT_REPOSITORY https://github.com/Nuand/bladeRF.git
                GIT_TAG ${BLADERF_TAG}
                DEPENDS ${PTHREADS4W_DEPENDS} ${LIBUSB_DEPENDS}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/bladerf"
                SOURCE_SUBDIR "host"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                -DPKG_CONFIG_EXECUTABLE=${PKG_CONFIG_EXECUTABLE}
                -DENABLE_BACKEND_USB=ON
                -DENABLE_BACKEND_LIBUSB=ON
                -DLIBUSB_HEADER_FILE=${LIBBLADERF_LIBUSB_HEADER_FILE}
                -Dusb_LIBRARY=${LIBUSB_LIBRARIES}
                -DLIBUSB_PATH=${EXTERNAL_LIBRARY_FOLDER}/libusb
                -DTHREADS_PTHREADS_INCLUDE_DIR=${PTHREADS4W_INCLUDE_DIR}
                -DTHREADS_PTHREADS_WIN32_LIBRARY=${PTHREADS4W_LIBRARIES}
                -DLIBPTHREADSWIN32_PATH=${PTHREADS4W_INCLUDE_DIR}
                -DENABLE_FX3_BUILD=OFF
                -DENABLE_HOST_BUILD=ON
                -DENABLE_BACKEND_CYAPI=OFF
                -DTREAT_WARNINGS_AS_ERRORS=OFF
                BUILD_BYPRODUCTS "${LIBBLADERF_LIBRARIES}"
                INSTALL_COMMAND ""
                TEST_COMMAND ""
                )
        if (WIN32)
            add_dependencies(bladerf pthreads4w)
        endif()
        ExternalProject_Get_Property(bladerf source_dir binary_dir)
        set_global_cache(LIBBLADERF_FOUND ON)
        set(LIBBLADERF_EXTERNAL ON CACHE INTERNAL "")
        set(LIBBLADERF_INCLUDE_DIRS "${source_dir}/host/libraries/libbladeRF/include" CACHE INTERNAL "")
        if (WIN32)
            install(FILES "${SDRANGEL_BINARY_BIN_DIR}/bladeRF${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
        elseif (APPLE)
            set(LIBBLADERF_LIBRARIES "${binary_dir}/output/libbladeRF${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
            install(DIRECTORY "${binary_dir}/output/" DESTINATION "${INSTALL_LIB_DIR}"
                    FILES_MATCHING PATTERN "libbladeRF*${CMAKE_SHARED_LIBRARY_SUFFIX}")
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/output")
        endif ()
    endif (ENABLE_BLADERF)

    if (ENABLE_USRP AND Boost_FOUND AND NOT WIN32 AND ((NOT UHD_FOUND) OR (ENABLE_EXTERNAL_LIBRARIES STREQUAL "ON")))
        if (DEFINED PYTHON_EXECUTABLE)
            set(UHD_PYTHON_ARGS "-DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}")
        else ()
            set(UHD_PYTHON_ARGS "")
        endif ()
        ExternalProject_Add(uhd
                GIT_REPOSITORY https://github.com/EttusResearch/uhd.git
                GIT_TAG ${UHD_TAG}
                DEPENDS ${LIBUSB_DEPENDS} ${Boost_DEPENDS}
                PREFIX "${EXTERNAL_BUILD_LIBRARIES}/uhd"
                SOURCE_SUBDIR "host"
                CMAKE_ARGS ${COMMON_CMAKE_ARGS}
                    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                    -DBoost_INCLUDE_DIR=${Boost_INCLUDE_DIR}
                    -DLIBUSB_LIBRARIES=${LIBUSB_LIBRARIES}
                    -DLIBUSB_INCLUDE_DIR=${LIBUSB_INCLUDE_DIR}
                    ${UHD_PYTHON_ARGS}
                    -DCMAKE_INSTALL_NAME_DIR=<INSTALL_DIR>/lib
                TEST_COMMAND ""
                )
        ExternalProject_Get_Property(uhd install_dir)
        set_global_cache(UHD_FOUND ON)
        set(UHD_EXTERNAL ON CACHE INTERNAL "")
        set(UHD_INCLUDE_DIR "${install_dir}/include" CACHE INTERNAL "")
        if (WIN32)
            install(FILES "${SDRANGEL_BINARY_BIN_DIR}/uhd${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
        elseif (APPLE)
            set(UHD_LIBRARIES "${install_dir}/lib/libuhd${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
            set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${install_dir}/lib")
        endif ()
    endif (ENABLE_USRP AND Boost_FOUND AND NOT WIN32 AND ((NOT UHD_FOUND) OR (ENABLE_EXTERNAL_LIBRARIES STREQUAL "ON")))

endif (WIN32 OR APPLE)

# Set CACHE so variable is visible in cpack CMakeFile
set(MACOS_EXTERNAL_LIBS_FIXUP ${MACOS_EXTERNAL_LIBS_FIXUP} CACHE INTERNAL "")
