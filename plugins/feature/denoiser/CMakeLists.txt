project(denoiser)

set(denoiser_SOURCES
	denoiser.cpp
    denoisersettings.cpp
    denoiserplugin.cpp
    denoiserworker.cpp
    denoiserwebapiadapter.cpp
)

set(denoiser_HEADERS
	denoiser.h
    denoisersettings.h
    denoiserplugin.h
    denoiserworker.h
    denoiserwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${RNNOISE_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(denoiser_SOURCES
        ${denoiser_SOURCES}
        denoisergui.cpp
        denoisergui.ui
    )
    set(denoiser_HEADERS
        ${denoiser_HEADERS}
	    denoisergui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}featuredenoiser)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}featuredenoisersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${denoiser_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME DenoiserPlugin ${denoiser_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    ${RNNOISE_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
