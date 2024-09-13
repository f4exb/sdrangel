project(atv)

set(atv_SOURCES
    atvdemod.cpp
    atvdemodbaseband.cpp
    atvdemodsink.cpp
    atvdemodsettings.cpp
    atvdemodwebapiadapter.cpp
	atvdemodgui.cpp
	atvdemodplugin.cpp
	atvdemodgui.ui
)

set(atv_HEADERS
    atvdemod.h
    atvdemodbaseband.h
    atvdemodsink.h
    atvdemodsettings.h
    atvdemodwebapiadapter.h
	atvdemodgui.h
	atvdemodplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
)

set(TARGET_NAME ${PLUGINS_PREFIX}demodatv)
set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${atv_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME ATVDemodPlugin ${atv_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    Qt::Widgets
	sdrbase
	sdrgui
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
