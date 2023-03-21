cmake_minimum_required(VERSION 3.14)

project(convert_transmitters LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

#include_directories(../../../../../exports)
include_directories(.)

add_executable(convert_transmitters
  main.cpp
  ../../../../../sdrbase/util/csv.cpp
  ../../../../../sdrbase/util/csv.h
  ../../../../../sdrbase/util/units.cpp
  ../../../../../sdrbase/util/units.h
  export.h
)
target_link_libraries(convert_transmitters Qt${QT_VERSION_MAJOR}::Core)

install(TARGETS convert_transmitters
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
