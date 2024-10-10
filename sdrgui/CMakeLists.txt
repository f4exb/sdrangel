project (sdrgui)

# with cmake <3.9 doesn't work with ../ used on webapi
# see https://cmake.org/cmake/help/v3.9/prop_tgt/AUTOUIC_SEARCH_PATHS.html
set(CMAKE_AUTOUIC OFF)

set(sdrgui_SOURCES
    mainwindow.cpp
    gui/aboutdialog.cpp
    gui/acronymview.cpp
    gui/addpresetdialog.cpp
    gui/audiodialog.cpp
    gui/audioselectdialog.cpp
    gui/basicchannelsettingsdialog.cpp
    gui/basicdevicesettingsdialog.cpp
    gui/basicfeaturesettingsdialog.cpp
    gui/buttonswitch.cpp
    gui/channeladddialog.cpp
    gui/clickablelabel.cpp
    gui/colordialog.cpp
    gui/colormapper.cpp
    gui/commanditem.cpp
    gui/commandsdialog.cpp
    gui/commandoutputdialog.cpp
    gui/configurationsdialog.cpp
    gui/coursedeviationindicator.cpp
    gui/crightclickenabler.cpp
    gui/customtextedit.cpp
    gui/cwkeyergui.cpp
    gui/cwmousekeyerenabler.cpp
    gui/datetimedelegate.cpp
    gui/decimaldelegate.cpp
    gui/devicesetselectiondialog.cpp
    gui/devicesetpresetsdialog.cpp
    gui/devicestreamselectiondialog.cpp
    gui/deviceuserargsdialog.cpp
    gui/dialogpositioner.cpp
    gui/dialpopup.cpp
    gui/dmsspinbox.cpp
    gui/editcommanddialog.cpp
    gui/externalclockbutton.cpp
    gui/externalclockdialog.cpp
    gui/fmpreemphasisdialog.cpp
    gui/featureadddialog.cpp
    gui/featurelayout.cpp
    gui/featurepresetsdialog.cpp
    gui/fftdialog.cpp
    gui/fftnrdialog.cpp
    gui/fftwisdomdialog.cpp
    gui/flowlayout.cpp
    gui/framelesswindowresizer.cpp
    gui/frequencydelegate.cpp
    gui/glscope.cpp
    gui/glscopegui.cpp
    gui/glshadercolors.cpp
    gui/glshadercolormap.cpp
    gui/glshadersimple.cpp
    gui/glshaderspectrogram.cpp
    gui/glshadertextured.cpp
    gui/glshadertvarray.cpp
    gui/glspectrum.cpp
    gui/glspectrumgui.cpp
    gui/glspectrumview.cpp
    gui/graphicsdialog.cpp
    gui/graphicsviewzoom.cpp
    gui/httpdownloadmanagergui.cpp
    gui/indicator.cpp
    gui/int64delegate.cpp
    gui/int64validator.cpp
    gui/levelmeter.cpp
    gui/loggingdialog.cpp
    gui/logslider.cpp
    gui/loglabelslider.cpp
    gui/mdiutils.cpp
    gui/mypositiondialog.cpp
    gui/nanosecondsdelegate.cpp
    gui/perioddial.cpp
    gui/pluginsdialog.cpp
    gui/pluginpresetsdialog.cpp
    gui/presetitem.cpp
    gui/profiledialog.cpp
    gui/rollupcontents.cpp
    gui/samplingdevicedialog.cpp
    gui/scaleengine.cpp
    gui/scaledimage.cpp
    gui/scidoublespinbox.cpp
    gui/sdrangelsplash.cpp
    gui/spectrumcalibrationpointsdialog.cpp
    gui/spectrummarkersdialog.cpp
    gui/spectrummeasurementsdialog.cpp
    gui/spectrummeasurements.cpp
    gui/tablecolorchooser.cpp
    gui/tabletapandhold.cpp
    gui/tickedslider.cpp
    gui/timedelegate.cpp
    gui/transverterbutton.cpp
    gui/transverterdialog.cpp
    gui/tvscreen.cpp
    gui/tvscreenanalog.cpp
    gui/valuedial.cpp
    gui/valuedialz.cpp
    gui/welcomedialog.cpp
    gui/workspace.cpp
    gui/workspaceselectiondialog.cpp
    gui/wsspectrumsettingsdialog.cpp
    gui/wrappingdatetimeedit.cpp
    gui/wrappingdial.cpp
    gui/wrappingspinbox.cpp

    dsp/scopevisxy.cpp

    device/devicegui.cpp
    device/deviceuiset.cpp

    channel/channelgui.cpp

    feature/featuregui.cpp
    feature/featureuiset.cpp

    mainspectrum/mainspectrumgui.cpp

    soapygui/discreterangegui.cpp
    soapygui/intervalrangegui.cpp
    soapygui/itemsettinggui.cpp
    soapygui/stringrangegui.cpp
    soapygui/dynamicitemsettinggui.cpp
    soapygui/dynamicargsettinggui.cpp
    soapygui/intervalslidergui.cpp
    soapygui/complexfactorgui.cpp
    soapygui/arginfogui.cpp

    resources/res.qrc
)

set(sdrgui_HEADERS
    mainwindow.h
    gui/aboutdialog.h
    gui/accessiblevaluedial.h
    gui/accessiblevaluedialz.h
    gui/acronymview.h
    gui/addpresetdialog.h
    gui/audiodialog.h
    gui/audioselectdialog.h
    gui/basicchannelsettingsdialog.h
    gui/basicdevicesettingsdialog.h
    gui/basicfeaturesettingsdialog.h
    gui/buttonswitch.h
    gui/channeladddialog.h
    gui/colordialog.h
    gui/colormapper.h
    gui/commanditem.h
    gui/commandsdialog.h
    gui/commandoutputdialog.h
    gui/configurationsdialog.h
    gui/coursedeviationindicator.h
    gui/crightclickenabler.h
    gui/customtextedit.h
    gui/cwkeyergui.h
    gui/cwmousekeyerenabler.h
    gui/datetimedelegate.h
    gui/decimaldelegate.h
    gui/devicesetselectiondialog.h
    gui/devicesetpresetsdialog.h
    gui/devicestreamselectiondialog.h
    gui/deviceuserargsdialog.h
    gui/dialogpositioner.h
    gui/dialpopup.h
    gui/dmsspinbox.h
    gui/doublevalidator.h
    gui/editcommanddialog.h
    gui/externalclockbutton.h
    gui/externalclockdialog.h
    gui/fmpreemphasisdialog.h
    gui/featureadddialog.h
    gui/featurelayout.h
    gui/featurepresetsdialog.h
    gui/fftdialog.h
    gui/fftnrdialog.h
    gui/fftwisdomdialog.h
    gui/flowlayout.h
    gui/framelesswindowresizer.h
    gui/frequencydelegate.h
    gui/glscope.h
    gui/glscopegui.h
    gui/glshadercolors.h
    gui/glshadercolormap.h
    gui/glshadersimple.h
    gui/glshaderspectrogram.h
    gui/glshadertvarray.h
    gui/glshadertextured.h
    gui/glspectrum.h
    gui/glspectrumgui.h
    gui/glspectrumview.h
    gui/graphicsdialog.h
    gui/graphicsviewzoom.h
    gui/httpdownloadmanagergui.h
    gui/indicator.h
    gui/int64delegate.h
    gui/int64validator.h
    gui/levelmeter.h
    gui/loggingdialog.h
    gui/logslider.h
    gui/loglabelslider.h
    gui/mdiutils.h
    gui/mypositiondialog.h
    gui/nanosecondsdelegate.h
    gui/perioddial.h
    gui/physicalunit.h
    gui/pluginsdialog.h
    gui/pluginpresetsdialog.h
    gui/presetitem.h
    gui/presets/dialog.h
    gui/profiledialog.h
    gui/qtcompatibility.h
    gui/rollupcontents.h
    gui/samplingdevicedialog.h
    gui/scaleengine.h
    gui/scaledimage.h
    gui/scidoublespinbox.h
    gui/sdrangelsplash.h
    gui/spectrumcalibrationpointsdialog.h
    gui/spectrummarkersdialog.h
    gui/spectrummeasurementsdialog.h
    gui/spectrummeasurements.h
    gui/tablecolorchooser.h
    gui/tabletapandhold.h
    gui/tickedslider.h
    gui/timedelegate.h
    gui/transverterbutton.h
    gui/transverterdialog.h
    gui/tvscreen.h
    gui/tvscreenanalog.h
    gui/valuedial.h
    gui/valuedialz.h
    gui/welcomedialog.h
    gui/workspace.h
    gui/workspaceselectiondialog.h
    gui/wsspectrumsettingsdialog.h
    gui/wrappingdatetimeedit.h
    gui/wrappingdial.h
    gui/wrappingspinbox.h

    dsp/scopevisxy.h

    device/devicegui.h
    device/deviceuiset.h

    channel/channelgui.h

    feature/featuregui.h
    feature/featureuiset.h

    mainspectrum/mainspectrumgui.h

    soapygui/discreterangegui.h
    soapygui/intervalrangegui.h
    soapygui/itemsettinggui.h
    soapygui/stringrangegui.h
    soapygui/dynamicitemsettinggui.h
    soapygui/dynamicargsettinggui.h
    soapygui/intervalslidergui.h
    soapygui/complexfactorgui.h
    soapygui/arginfogui.h
)

set(sdrgui_FORMS
    # mainwindow.ui
    gui/aboutdialog.ui
    gui/addpresetdialog.ui
    gui/basicchannelsettingsdialog.ui
    gui/basicdevicesettingsdialog.ui
    gui/basicfeaturesettingsdialog.ui
    gui/channeladddialog.ui
    gui/commandsdialog.ui
    gui/commandoutputdialog.ui
    gui/configurationsdialog.ui
    gui/cwkeyergui.ui
    gui/devicesetpresetsdialog.ui
    gui/devicestreamselectiondialog.ui
    gui/deviceuserargsdialog.ui
    gui/editcommanddialog.ui
    gui/externalclockdialog.ui
    gui/fmpreemphasisdialog.ui
    gui/featureadddialog.ui
    gui/featurepresetsdialog.ui
    gui/fftdialog.ui
    gui/fftnrdialog.ui
    gui/fftwisdomdialog.ui
    gui/glscopegui.ui
    gui/glspectrumgui.ui
    gui/graphicsdialog.ui
    gui/pluginsdialog.ui
    gui/pluginpresetsdialog.ui
    gui/profiledialog.ui
    gui/audiodialog.ui
    gui/audioselectdialog.ui
    gui/samplingdevicecontrol.ui
    gui/samplingdevicedialog.ui
    gui/spectrummarkersdialog.ui
    gui/spectrummeasurementsdialog.ui
    gui/spectrumcalibrationpointsdialog.ui
    gui/myposdialog.ui
    gui/transverterdialog.ui
    gui/loggingdialog.ui
    gui/welcomedialog.ui
    gui/workspaceselectiondialog.ui
    gui/wsspectrumsettingsdialog.ui
    soapygui/discreterangegui.ui
    soapygui/intervalrangegui.ui
    soapygui/intervalslidergui.ui
    soapygui/complexfactorgui.ui
    soapygui/arginfogui.ui
)

qt_wrap_ui(sdrgui_FORMS_HEADERS ${sdrgui_FORMS})

include_directories(
  ${CMAKE_SOURCE_DIR}/exports
  ${CMAKE_SOURCE_DIR}/sdrbase
  ${CMAKE_SOURCE_DIR}/logging
  ${CMAKE_SOURCE_DIR}/httpserver
  ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
  ${Boost_INCLUDE_DIRS}
)
if(NOT ANDROID)
  include_directories(${OPENGL_INCLUDE_DIR})
endif()

add_library(sdrgui
  ${sdrgui_SOURCES}
  ${sdrgui_FORMS_HEADERS}
  )

target_link_libraries(sdrgui
  Qt::Core
  Qt::Widgets
  Qt::OpenGL
  Qt::Multimedia
  Qt::Positioning
  ${OPENGL_LIBRARIES}
  ${sdrgui_LIMERFE_LIB}
  sdrbase
  logging
  )
if (Qt6_FOUND)
  target_link_libraries(sdrgui
    Qt6::StateMachine
  )
  target_link_libraries(sdrbase
    Qt::OpenGLWidgets
  )
endif()

if(ANDROID)
  target_link_libraries(sdrgui GLESv3)
endif()

install(TARGETS sdrgui DESTINATION ${INSTALL_LIB_DIR})

if(WIN32 AND Qt6_FOUND)
    # Run deployqt for OpenGLWidgets
    include(DeployQt)
    windeployqt(sdrgui ${SDRANGEL_BINARY_BIN_DIR} "")
endif()

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:sdrgui> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_LIB_DIR} )
endif()
