project(gnuradio)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)

find_package(GnuradioRuntime)
find_package(GnuradioOsmosdr)

if(UNIX AND NOT BOOST_ROOT AND EXISTS "/usr/lib64")
    list(APPEND BOOST_LIBRARYDIR "/usr/lib64") #fedora 64-bit fix
endif(UNIX AND NOT BOOST_ROOT AND EXISTS "/usr/lib64")

find_package(Boost COMPONENTS system)

if(NOT GNURADIO_RUNTIME_FOUND AND GNURADIO_OSMOSDR_FOUND)

    message(STATUS "GNU Radio not found, assuming built-in gr-osmosdr runtime.")

    set(GNURADIO_RUNTIME_FOUND TRUE)
    set(GNURADIO_RUNTIME_INCLUDE_DIRS "")
    set(GNURADIO_RUNTIME_LIBRARIES "")

    FOREACH(inc ${GNURADIO_OSMOSDR_INCLUDE_DIRS})
        LIST(APPEND GNURADIO_RUNTIME_INCLUDE_DIRS "${inc}/osmosdr/runtime")
    ENDFOREACH(inc)

    LIST(APPEND GNURADIO_RUNTIME_LIBRARIES ${GNURADIO_OSMOSDR_LIBRARIES})

endif(NOT GNURADIO_RUNTIME_FOUND AND GNURADIO_OSMOSDR_FOUND)

if(Boost_FOUND AND GNURADIO_RUNTIME_FOUND AND GNURADIO_OSMOSDR_FOUND)

set(gnuradio_SOURCES
	gnuradiogui.cpp
	gnuradioinput.cpp
	gnuradioplugin.cpp
	gnuradiothread.cpp
)

set(gnuradio_HEADERS
	gnuradiogui.h
	gnuradioinput.h
	gnuradioplugin.h
	gnuradiothread.h
)

set(gnuradio_FORMS
	gnuradiogui.ui
)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include-gpl
	${Boost_INCLUDE_DIRS}
	${GNURADIO_RUNTIME_INCLUDE_DIRS}
	${GNURADIO_OSMOSDR_INCLUDE_DIRS}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)
add_definitions(-DBOOST_ALL_NO_LIB)

if(MSVC)
add_definitions(-DNOMINMAX)
endif()

qt5_wrap_ui(gnuradio_FORMS_HEADERS ${gnuradio_FORMS})

add_library(inputgnuradio SHARED
	${gnuradio_SOURCES}
	${gnuradio_HEADERS_MOC}
	${gnuradio_FORMS_HEADERS}
)

target_link_libraries(inputgnuradio
	${QT_LIBRARIES}
	${Boost_LIBRARIES}
	${GNURADIO_RUNTIME_LIBRARIES}
	${GNURADIO_OSMOSDR_LIBRARIES}
	sdrbase
)

qt5_use_modules(inputgnuradio Core Widgets OpenGL Multimedia)

endif(Boost_FOUND AND GNURADIO_RUNTIME_FOUND AND GNURADIO_OSMOSDR_FOUND)
