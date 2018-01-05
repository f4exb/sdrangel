project (sdrgui)

set(sdrgui_SOURCES
    mainwindow.cpp
    gui/aboutdialog.cpp
    gui/addpresetdialog.cpp
    gui/basicchannelsettingsdialog.cpp
    gui/buttonswitch.cpp
    gui/channelwindow.cpp
    gui/clickablelabel.cpp
    gui/colormapper.cpp
    gui/commanditem.cpp
    gui/commandkeyreceiver.cpp
    gui/commandoutputdialog.cpp
    gui/cwkeyergui.cpp
    gui/editcommanddialog.cpp
    gui/externalclockbutton.cpp
    gui/externalclockdialog.cpp
    gui/glscope.cpp
    gui/glscopegui.cpp
    gui/glscopeng.cpp
    gui/glscopemulti.cpp
    gui/glscopenggui.cpp
    gui/glscopemultigui.cpp
    gui/glshadersimple.cpp
    gui/glshadertextured.cpp
    gui/glspectrum.cpp
    gui/glspectrumgui.cpp
    gui/indicator.cpp
    gui/levelmeter.cpp
    gui/loggingdialog.cpp
    gui/mypositiondialog.cpp
    gui/pluginsdialog.cpp
    gui/audiodialog.cpp
    gui/presetitem.cpp
    gui/rollupwidget.cpp
    gui/samplingdevicecontrol.cpp
    gui/samplingdevicedialog.cpp
    gui/scale.cpp
    gui/scaleengine.cpp
    gui/tickedslider.cpp
    gui/transverterbutton.cpp
    gui/transverterdialog.cpp
    gui/valuedial.cpp
    gui/valuedialz.cpp
    
    dsp/scopevis.cpp
    dsp/scopevisng.cpp
    dsp/scopevismulti.cpp
    dsp/spectrumvis.cpp
    dsp/spectrumscopecombovis.cpp
    dsp/spectrumscopengcombovis.cpp
    
    device/deviceuiset.cpp
    
    webapi/webapiadaptergui.cpp
)

set(sdrgui_HEADERS
    mainwindow.h
    gui/aboutdialog.h
    gui/addpresetdialog.h
    gui/basicchannelsettingsdialog.h
    gui/buttonswitch.h
    gui/channelwindow.h
    gui/colormapper.h
    gui/commanditem.h
    gui/commandkeyreceiver.h
    gui/commandoutputdialog.h
    gui/cwkeyergui.h
    gui/editcommanddialog.h
    gui/externalclockbutton.h
    gui/externalclockdialog.h
    gui/glscope.h
    gui/glscopegui.h
    gui/glscopeng.h
    gui/glscopemulti.h
    gui/glscopenggui.h
    gui/glscopemultigui.h
    gui/glshadersimple.h
    gui/glshadertextured.h
    gui/glspectrum.h
    gui/glspectrumgui.h
    gui/indicator.h
    gui/levelmeter.h
    gui/loggingdialog.h    
    gui/mypositiondialog.h
    gui/physicalunit.h
    gui/pluginsdialog.h
    gui/audiodialog.h
    gui/presetitem.h
    gui/rollupwidget.h
    gui/samplingdevicecontrol.h
    gui/samplingdevicedialog.h
    gui/scale.h
    gui/scaleengine.h
    gui/tickedslider.h
    gui/transverterbutton.h
    gui/transverterdialog.h    
    gui/valuedial.h
    gui/valuedialz.h
    
    dsp/scopevis.h
    dsp/scopevisng.h
    dsp/scopevismulti.h
    dsp/spectrumvis.h
    dsp/spectrumscopecombovis.h
    dsp/spectrumscopengcombovis.h
    
    device/deviceuiset.h
    
    webapi/webapiadaptergui.h
)

set(sdrgui_SOURCES
    ${sdrgui_SOURCES}
    ${sdrgui_HEADERS}
)

set(sdrgui_FORMS
    mainwindow.ui
    gui/aboutdialog.ui
    gui/addpresetdialog.ui
    gui/basicchannelsettingsdialog.ui
    gui/commandoutputdialog.ui
    gui/cwkeyergui.ui
    gui/editcommanddialog.ui
    gui/externalclockdialog.ui
    gui/glscopegui.ui
    gui/glscopenggui.ui
    gui/glscopemultigui.ui
    gui/glspectrumgui.ui
    gui/pluginsdialog.ui
    gui/audiodialog.ui
    gui/samplingdevicecontrol.ui
    gui/samplingdevicedialog.ui
    gui/myposdialog.ui
    gui/transverterdialog.ui
    gui/loggingdialog.ui
)

set(sdrgui_RESOURCES
    resources/res.qrc
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)

qt5_wrap_ui(sdrgui_FORMS_HEADERS ${sdrgui_FORMS})
qt5_add_resources(sdrgui_RESOURCES_RCC ${sdrgui_RESOURCES})

if(WIN32)
    SET(sdrgui_SOURCES ${sdrgui_SOURCES} resources/sdrangel.rc)
endif(WIN32)

add_library(sdrgui SHARED
    ${sdrgui_SOURCES}
    ${sdrgui_HEADERS_MOC}
    ${sdrgui_FORMS_HEADERS}
    ${sdrgui_RESOURCES_RCC}
)

include_directories(
    .
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${CMAKE_SOURCE_DIR}/logging
    ${CMAKE_SOURCE_DIR}/httpserver
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
    ${CMAKE_CURRENT_BINARY_DIR}
    ${OPENGL_INCLUDE_DIR}
)

target_link_libraries(sdrgui
    ${QT_LIBRARIES}
    ${OPENGL_LIBRARIES}
    sdrbase
    logging
)

set_target_properties(sdrgui PROPERTIES DEFINE_SYMBOL "sdrangel_EXPORTS")
target_compile_features(sdrgui PRIVATE cxx_generalized_initializers) # cmake >= 3.1.0

qt5_use_modules(sdrgui Core Widgets OpenGL Multimedia)

install(TARGETS sdrgui DESTINATION lib)

