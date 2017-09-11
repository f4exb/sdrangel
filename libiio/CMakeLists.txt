cmake_minimum_required(VERSION 2.8.7)
project(iio)

set(LIBIIO_VERSION_MAJOR 0)
set(LIBIIO_VERSION_MINOR 10)
set(VERSION "${LIBIIO_VERSION_MAJOR}.${LIBIIO_VERSION_MINOR}")

# Set the default install path to /usr
if (NOT WIN32 AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "default install path" FORCE)
endif()

set(CMAKE_INSTALL_DOCDIR "" CACHE PATH "documentation root (DATAROOTDIR/doc/${PROJECT_NAME}${LIBIIO_VERSION_MAJOR}-doc)")
include(GNUInstallDirs)
set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_NAME}${LIBIIO_VERSION_MAJOR}-doc")

set(INSTALL_PKGCONFIG_DIR "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    CACHE PATH "Installation directory for pkgconfig (.pc) files")
mark_as_advanced(INSTALL_PKGCONFIG_DIR)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
        "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS None Debug Release RelWithDebInfo MinSizeRel)
endif()

set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries")

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    option(OSX_PACKAGE "Create a OSX package" ON)
    set(CMAKE_MACOSX_RPATH ON)
    set(SKIP_INSTALL_ALL ${OSX_PACKAGE})
endif()

option(WITH_NETWORK_BACKEND "Enable the network backend" OFF)
option(WITH_TESTS "Build the test programs" OFF)

if (WITH_TESTS)
    set(NEED_THREADS 1)
endif()

if (MSVC)
    # Avoid annoying warnings from Visual Studio
    add_definitions(-D_CRT_SECURE_NO_WARNINGS=1)

    set(CMAKE_FIND_LIBRARY_PREFIXES "lib" "")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll.a" ".a" ".lib")
endif()

if (NOT LOG_LEVEL)
    set(LOG_LEVEL Info CACHE STRING "Log level" FORCE)
    set_property(CACHE LOG_LEVEL PROPERTY STRINGS NoLog Error Warning Info Debug)
endif()

if (CMAKE_COMPILER_IS_GNUCC)
    if (NOT WIN32)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
    endif()

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare")

    include(CheckCCompilerFlag)
    check_c_compiler_flag(-Wpedantic HAS_WPEDANTIC)
    if (HAS_WPEDANTIC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpedantic")
    endif()
endif()

include(CheckSymbolExists)
check_symbol_exists(strdup "string.h" HAS_STRDUP)
check_symbol_exists(strerror_r "string.h" HAS_STRERROR_R)
check_symbol_exists(newlocale "locale.h" HAS_NEWLOCALE)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    option(WITH_IIOD "Build the IIO Daemon" OFF)
    option(WITH_LOCAL_BACKEND "Enable the local backend" OFF)

    if (WITH_IIOD AND NOT WITH_LOCAL_BACKEND)
        message(SEND_ERROR "IIOD can only be enabled if the local backend is enabled")
    endif()
    if (WITH_IIOD)
        set(NEED_THREADS 1)
    endif()

    set(CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE=1")
    add_definitions(-D_GNU_SOURCE=1)
endif()

option(ENABLE_IPV6 "Define if you want to enable IPv6 support" OFF)
if (ENABLE_IPV6)
    check_symbol_exists(in6addr_any "netinet/in.h" HAVE_IPV6)

    if (NOT HAVE_IPV6)
        message(WARNING "IPv6 is not available in your system.")
    endif()
endif()

set(LIBIIO_CFILES ${LIBIIOSRC}/backend.c 
    ${LIBIIOSRC}/channel.c 
    ${LIBIIOSRC}/device.c 
    ${LIBIIOSRC}/context.c 
    ${LIBIIOSRC}/buffer.c 
    ${LIBIIOSRC}/utilities.c 
    ${LIBIIOSRC}/scan.c)
set(LIBIIO_HEADERS ${LIBIIOSRC}/iio.h)

add_definitions(-D_POSIX_C_SOURCE=200809L -D__XSI_VISIBLE=500 -DLIBIIO_EXPORTS=1)

# Get the GIT hash of the latest commit
include(FindGit OPTIONAL)
if (GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --show-toplevel
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE LIBIIO_GIT_REPO
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (${LIBIIO_GIT_REPO} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE LIBIIO_VERSION_GIT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()
endif()

if (NOT LIBIIO_VERSION_GIT)
    set(LIBIIO_VERSION_GIT v${VERSION})
endif()

if(WITH_LOCAL_BACKEND)
    add_definitions(-DLOCAL_BACKEND=1)
    set(LIBIIO_CFILES ${LIBIIO_CFILES} ${LIBIIOSRC}/local.c)

    # Link with librt if present
    find_library(LIBRT_LIBRARIES rt)
    if (LIBRT_LIBRARIES)
        set(LIBS_TO_LINK ${LIBS_TO_LINK} ${LIBRT_LIBRARIES})
    endif()

    option(WITH_LOCAL_CONFIG "Read local context attributes from /etc/libiio.ini" OFF)
    if (WITH_LOCAL_CONFIG)
        find_library(LIBINI_LIBRARIES ini)
        find_path(LIBINI_INCLUDE_DIR ini.h)
        if (NOT LIBINI_LIBRARIES OR NOT LIBINI_INCLUDE_DIR)
            message(SEND_ERROR "WITH_LOCAL_CONFIG option requires libini to be installed")
        else()
            include_directories(${LIBINI_INCLUDE_DIR})
            set(LIBS_TO_LINK ${LIBS_TO_LINK} ${LIBINI_LIBRARIES})
        endif()
    endif()
endif()

find_library(LIBUSB_LIBRARIES usb-1.0)
find_path(LIBUSB_INCLUDE_DIR libusb-1.0/libusb.h)
if (LIBUSB_LIBRARIES AND LIBUSB_INCLUDE_DIR)
    option(WITH_USB_BACKEND "Enable the libusb backend" ON)

    if(WITH_USB_BACKEND)
        add_definitions(-DUSB_BACKEND=1)
        set(LIBIIO_CFILES ${LIBIIO_CFILES} ${LIBIIOSRC}/usb.c)
        set(LIBS_TO_LINK ${LIBS_TO_LINK} ${LIBUSB_LIBRARIES})
        set(IIOD_CLIENT 1)
        set(NEED_LIBXML2 1)
        set(NEED_THREADS 1)

        include_directories(${LIBUSB_INCLUDE_DIR})
    endif()
endif()

find_library(LIBSERIALPORT_LIBRARIES serialport)
find_path(LIBSERIALPORT_INCLUDE_DIR libserialport.h)
if (LIBSERIALPORT_LIBRARIES AND LIBSERIALPORT_INCLUDE_DIR)
    option(WITH_SERIAL_BACKEND "Enable the serial backend" OFF)

    if (WITH_SERIAL_BACKEND)
        file(STRINGS ${LIBSERIALPORT_INCLUDE_DIR}/libserialport.h LIBSERIALPORT_VERSION_STR REGEX "SP_PACKAGE_VERSION_STRING")
        string(REGEX REPLACE "#define SP_PACKAGE_VERSION_STRING \"(.*)\"" "\\1" LIBSERIALPORT_VERSION ${LIBSERIALPORT_VERSION_STR})
        if ("${LIBSERIALPORT_VERSION}" VERSION_LESS 0.1.1)
            message(SEND_ERROR "The installed version of libserialport is too old. The minimum version supported is 0.1.1.")
        endif()

        add_definitions(-DSERIAL_BACKEND=1)
        set(LIBIIO_CFILES ${LIBIIO_CFILES} ${LIBIIOSRC}/serial.c)
        set(LIBS_TO_LINK ${LIBS_TO_LINK} ${LIBSERIALPORT_LIBRARIES})

        set(NEED_THREADS 1)
        set(IIOD_CLIENT 1)
        set(NEED_LIBXML2 1)

        include_directories(${LIBSERIALPORT_INCLUDE_DIR})
    endif()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

if(WITH_NETWORK_BACKEND)
    if (WIN32)
        set(LIBS_TO_LINK ${LIBS_TO_LINK} wsock32 ws2_32)
    endif()

    if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        include(CheckCSourceCompiles)
        check_c_source_compiles("#include <fcntl.h>\nint main(void) { return O_TMPFILE; }" HAS_O_TMPFILE)

        if (HAS_O_TMPFILE)
            option(WITH_NETWORK_GET_BUFFER "Enable experimental zero-copy transfers" OFF)
        endif(HAS_O_TMPFILE)

        check_c_source_compiles("#include <sys/eventfd.h>\nint main(void) { return eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK); }" WITH_NETWORK_EVENTFD)
    endif()

    if(NOT WIN32)
        include(CheckCSourceCompiles)
        check_c_source_compiles("#include <unistd.h>\n#include <fcntl.h>\nint main(void) { int fd[2]; return pipe2(fd, O_CLOEXEC | O_NONBLOCK); }" HAS_PIPE2)
    endif()

    add_definitions(-DNETWORK_BACKEND=1)
    set(LIBIIO_CFILES ${LIBIIO_CFILES} ${LIBIIOSRC}/network.c)

    find_library(AVAHI_CLIENT_LIBRARIES avahi-client)
    find_library(AVAHI_COMMON_LIBRARIES avahi-common)
    if(AVAHI_CLIENT_LIBRARIES AND AVAHI_COMMON_LIBRARIES)
        set(HAVE_AVAHI ON)
        set(AVAHI_LIBRARIES ${AVAHI_CLIENT_LIBRARIES} ${AVAHI_COMMON_LIBRARIES})
        set(LIBS_TO_LINK ${LIBS_TO_LINK} ${AVAHI_LIBRARIES})
    endif()

    set(NEED_THREADS 1)
    set(IIOD_CLIENT 1)
    set(NEED_LIBXML2 1)
endif()

# Since libxml2-2.9.2, libxml2 provides its own LibXml2-config.cmake, with all
# variables correctly set.
# So, try first to find the CMake module provided by libxml2 package, then fallback
# on the CMake's FindLibXml2.cmake module (which can lack some definition, especially
# in static build case).
find_package(LibXml2 QUIET NO_MODULE)
if(DEFINED LIBXML2_VERSION_STRING)
    set(LIBXML2_FOUND ON)
    set(LIBXML2_INCLUDE_DIR ${LIBXML2_INCLUDE_DIRS})
else()
    include(FindLibXml2)
endif()

if (LIBXML2_FOUND)
    option(WITH_XML_BACKEND "Enable the serial backend" ON)

    if (WITH_XML_BACKEND)
        set(LIBIIO_CFILES ${LIBIIO_CFILES} ${LIBIIOSRC}/xml.c)
        add_definitions(${LIBXML2_DEFINITIONS} -DXML_BACKEND=1)
        include_directories(${LIBXML2_INCLUDE_DIR})
        set(LIBS_TO_LINK ${LIBS_TO_LINK} ${LIBXML2_LIBRARIES})
    endif()
endif()

if (NEED_LIBXML2 AND NOT (LIBXML2_FOUND AND WITH_XML_BACKEND))
    message(SEND_ERROR "The selected backends require libxml2 and the XML backend to be enabled")
endif()

if (NEED_THREADS)
    if (NOT WIN32)
        find_library(PTHREAD_LIBRARIES pthread)

        if (PTHREAD_LIBRARIES)
            set(LIBS_TO_LINK ${LIBS_TO_LINK} ${PTHREAD_LIBRARIES})
        else()
            message(WARNING "pthread library not found; support for threads will be disabled")
            set(NO_THREADS ON)
        endif()
    else()
    endif()

    set(LIBIIO_CFILES ${LIBIIO_CFILES} ${LIBIIOSRC}/lock.c)
endif()

if (IIOD_CLIENT)
    set(LIBIIO_CFILES ${LIBIIO_CFILES} ${LIBIIOSRC}/iiod-client.c)
endif()

configure_file(${LIBIIOSRC}/libiio.iss.cmakein ${CMAKE_CURRENT_BINARY_DIR}/libiio.iss @ONLY)

set(LIBIIO_PC ${CMAKE_CURRENT_BINARY_DIR}/libiio.pc)
configure_file(${LIBIIOSRC}/libiio.pc.cmakein ${LIBIIO_PC} @ONLY)

if(NOT SKIP_INSTALL_ALL)
    install(FILES ${LIBIIO_PC} DESTINATION "${INSTALL_PKGCONFIG_DIR}")
endif()

#set(SETUP_PY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/setup.py)
#configure_file(python/setup.py.in ${SETUP_PY} @ONLY)

#add_subdirectory(bindings)

if (WITH_MATLAB_BINDINGS_API)
    set(LIBIIO_EXTRA_HEADERS ${LIBIIO_EXTRA_HEADERS} bindings/matlab/iio-wrapper.h)
    add_definitions(-DMATLAB_BINDINGS_API=1)
endif()

if(WITH_TESTS)
    add_subdirectory(tests)
endif()

add_definitions(-DQT_SHARED)

add_library(iio SHARED ${LIBIIO_CFILES} ${LIBIIO_HEADERS} ${LIBIIO_EXTRA_HEADERS})
set_target_properties(iio PROPERTIES
    VERSION ${VERSION}
    SOVERSION ${LIBIIO_VERSION_MAJOR}
    FRAMEWORK TRUE
    PUBLIC_HEADER ${LIBIIO_HEADERS}
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
target_link_libraries(iio LINK_PRIVATE ${LIBS_TO_LINK})

if (MSVC)
    set_target_properties(iio PROPERTIES OUTPUT_NAME libiio)
endif()

if(NOT SKIP_INSTALL_ALL)
    install(TARGETS iio
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        FRAMEWORK DESTINATION /Library/Frameworks
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

find_package(Doxygen)
if(DOXYGEN_FOUND)
    option(WITH_DOC "Generate documentation with Doxygen" OFF)

    if (WITH_DOC)
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
        set(HTML_DEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/html)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/doc DESTINATION ${HTML_DEST_DIR})

        add_custom_command(TARGET iio POST_BUILD
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen" VERBATIM
            )

        if(NOT SKIP_INSTALL_ALL)
            install(DIRECTORY ${HTML_DEST_DIR} DESTINATION ${CMAKE_INSTALL_DOCDIR})
        endif()
    endif()
else()
    message(STATUS "Doxygen not found, API documentation won't be generated")
endif()

# Create an installer if compiling for OSX
if(OSX_PACKAGE)
    set(LIBIIO_PKG ${CMAKE_CURRENT_BINARY_DIR}/libiio-${VERSION}.g${LIBIIO_VERSION_GIT}.pkg)
    set(LIBIIO_TEMP_PKG ${CMAKE_CURRENT_BINARY_DIR}/libiio-${VERSION}-temp.pkg)
    set(LIBIIO_DISTRIBUTION_XML ${CMAKE_CURRENT_BINARY_DIR}/Distribution.xml)
    set(LIBIIO_FRAMEWORK_DIR ${CMAKE_CURRENT_BINARY_DIR}/iio.framework)
    configure_file(Distribution.xml.cmakein ${LIBIIO_DISTRIBUTION_XML} @ONLY)

    find_program(PKGBUILD_EXECUTABLE
        NAMES pkgbuild
        DOC "OSX Package builder (pkgbuild)")
    mark_as_advanced(PKGBUILD_EXECUTABLE)

    find_program(PRODUCTBUILD_EXECUTABLE
        NAMES productbuild
        DOC "OSX Package builder (productbuild)")
    mark_as_advanced(PRODUCTBUILD_EXECUTABLE)

    foreach(_tool ${IIO_TESTS_TARGETS})
        list(APPEND IIO_TESTS $<TARGET_FILE:${_tool}>)
    endforeach()

    add_custom_command(OUTPUT ${LIBIIO_PKG}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${LIBIIO_FRAMEWORK_DIR}/Tools
        COMMAND ${CMAKE_COMMAND} -E copy ${IIO_TESTS} ${LIBIIO_FRAMEWORK_DIR}/Tools
        COMMAND ${PKGBUILD_EXECUTABLE}
            --component ${LIBIIO_FRAMEWORK_DIR}
            --identifier com.adi.iio --version ${VERSION}
            --install-location /Library/Frameworks ${LIBIIO_TEMP_PKG}
        COMMAND ${PRODUCTBUILD_EXECUTABLE}
            --distribution ${LIBIIO_DISTRIBUTION_XML} ${LIBIIO_PKG}
        COMMAND ${CMAKE_COMMAND} -E remove ${LIBIIO_TEMP_PKG}
        DEPENDS iio ${IIO_TESTS_TARGETS} ${LIBIIO_DISTRIBUTION_XML}
    )

    if (PKGBUILD_EXECUTABLE AND PRODUCTBUILD_EXECUTABLE)
        add_custom_target(libiio-pkg ALL DEPENDS ${LIBIIO_PKG})

        install(CODE "execute_process(COMMAND /usr/sbin/installer -pkg ${LIBIIO_PKG} -target /)")
    else()
        message(WARNING "Missing pkgbuild or productbuild: OSX installer won't be created.")
    endif()
endif()

if(WITH_IIOD)
    if (NOT PTHREAD_LIBRARIES)
        message(WARNING "IIOD requires threads support; disabling")
        set(WITH_IIOD OFF CACHE BOOL "" FORCE)
    else()
        add_subdirectory(iiod)
    endif()
endif()

if (NOT OSX_PACKAGE)
    # Support creating some basic binpkgs via `make package`.
    # Disabled if OSX_PACKAGE is enabled, as tarballs would end up empty otherwise.
    option(ENABLE_PACKAGING "Create .deb/.rpm or .tar.gz packages via 'make package'" OFF)

    if(ENABLE_PACKAGING)
        if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
            include(cmake/DarwinPackaging.cmake)
        endif()
        if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
            include(cmake/LinuxPackaging.cmake)
        endif()
    endif()
endif()

if (WITH_USB_BACKEND AND CMAKE_SYSTEM_NAME MATCHES "^Linux")
    option(INSTALL_UDEV_RULE "Install a udev rule for detection of USB devices" OFF)

    if (INSTALL_UDEV_RULE)
        set(UDEV_RULES_INSTALL_DIR /lib/udev/rules.d CACHE PATH "default install path for udev rules")

        configure_file(libiio.rules.cmakein ${CMAKE_CURRENT_BINARY_DIR}/90-libiio.rules @ONLY)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/90-libiio.rules DESTINATION ${UDEV_RULES_INSTALL_DIR})
    endif()
endif()

configure_file(${LIBIIOSRC}//iio-config.h.cmakein ${CMAKE_CURRENT_BINARY_DIR}/iio-config.h @ONLY)

if (WITH_EXAMPLES)
    add_subdirectory(examples)
endif()

if (WITH_PLUTOSDR)
    add_subdirectory(plutosdr)
endif()
