cmake_minimum_required(VERSION 3.16)
project(packet_g3ruh_tests LANGUAGES CXX)
enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)
get_filename_component(SDRANGEL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
add_executable(packet_g3ruh_test
    test_g3ruh.cpp
    ../packetdemodsettings.cpp
    ${SDRANGEL_ROOT}/sdrbase/dsp/firfilter.cpp
    ${SDRANGEL_ROOT}/sdrbase/util/ax25.cpp
    ${SDRANGEL_ROOT}/sdrbase/util/crc.cpp
    ${SDRANGEL_ROOT}/sdrbase/util/lfsr.cpp
    ${SDRANGEL_ROOT}/sdrbase/util/simpleserializer.cpp)
target_include_directories(packet_g3ruh_test PRIVATE .. ${SDRANGEL_ROOT}/exports ${SDRANGEL_ROOT}/sdrbase)
target_link_libraries(packet_g3ruh_test PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)
option(PACKET_TEST_SANITIZERS "Enable address and undefined behaviour sanitizers" OFF)
if(PACKET_TEST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(packet_g3ruh_test PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(packet_g3ruh_test PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME packet_g3ruh COMMAND packet_g3ruh_test)
