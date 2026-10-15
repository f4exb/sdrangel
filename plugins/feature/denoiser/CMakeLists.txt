project(denoiser)

if(APPLE)
    find_library(VST3_COREFOUNDATION_FRAMEWORK CoreFoundation)
    if(NOT VST3_COREFOUNDATION_FRAMEWORK)
        message(FATAL_ERROR "CoreFoundation is required for VST3 support on macOS")
    endif()
endif()

set(denoiser_SOURCES
	denoiser.cpp
    denoisersettings.cpp
    denoiserplugin.cpp
    denoiserworker.cpp
    denoiserwebapiadapter.cpp
    nvidiaaudioeffects.cpp
    vst3effect.cpp
)

set(denoiser_HEADERS
	denoiser.h
    denoisersettings.h
    denoiserplugin.h
    denoiserworker.h
    denoiserwebapiadapter.h
    nvidiaaudioeffects.h
    vst3effect.h
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
    qt_add_plugin(${TARGET_NAME} CLASS_NAME DenoiserPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${denoiser_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${CMAKE_DL_LIBS}
    ${VST3_COREFOUNDATION_FRAMEWORK}
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    ${RNNOISE_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

if(NOT SERVER_MODE)
    add_executable(sdrangel-vst3-scan vst3scan.cpp vst3effect.cpp)
    target_link_libraries(sdrangel-vst3-scan PRIVATE Qt::Core ${CMAKE_DL_LIBS} ${VST3_COREFOUNDATION_FRAMEWORK})
    if(WIN32)
        set(VST3_SCAN_RUNTIME_DIR "${SDRANGEL_BINARY_BIN_DIR}")
    else()
        set(VST3_SCAN_RUNTIME_DIR "${CMAKE_BINARY_DIR}")
    endif()
    set_target_properties(sdrangel-vst3-scan PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${VST3_SCAN_RUNTIME_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${VST3_SCAN_RUNTIME_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${VST3_SCAN_RUNTIME_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${VST3_SCAN_RUNTIME_DIR}")
    install(TARGETS sdrangel-vst3-scan DESTINATION ${INSTALL_BIN_DIR})
endif()

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
