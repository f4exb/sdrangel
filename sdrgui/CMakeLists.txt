project (sdrgui)

# with cmake <3.9 doesn't work with ../ used on webapi
# see https://cmake.org/cmake/help/v3.9/prop_tgt/AUTOUIC_SEARCH_PATHS.html
set(CMAKE_AUTOUIC OFF)

set(sdrgui_SOURCES
    mainwindow.cpp
    gui/aboutdialog.cpp
    gui/addpresetdialog.cpp
    gui/ambedevicesdialog.cpp
    gui/audiodialog.cpp
    gui/audioselectdialog.cpp
    gui/basicchannelsettingsdialog.cpp
    gui/basicdevicesettingsdialog.cpp
    gui/basicfeaturesettingsdialog.cpp
    gui/buttonswitch.cpp
    gui/channeladddialog.cpp
    gui/channelsdock.cpp
    gui/channelwindow.cpp
    gui/clickablelabel.cpp
    gui/colormapper.cpp
    gui/commanditem.cpp
    gui/commandkeyreceiver.cpp
    gui/commandoutputdialog.cpp
    gui/crightclickenabler.cpp
    gui/customtextedit.cpp
    gui/cwkeyergui.cpp
    gui/devicestreamselectiondialog.cpp
    gui/deviceuserargsdialog.cpp
    gui/dmsspinbox.cpp
    gui/editcommanddialog.cpp
    gui/externalclockbutton.cpp
    gui/externalclockdialog.cpp
    gui/fmpreemphasisdialog.cpp
    gui/featureadddialog.cpp
    gui/featuresdock.cpp
    gui/featurepresetsdialog.cpp
    gui/featurewindow.cpp
    gui/glscope.cpp
    gui/glscopegui.cpp
    gui/glshadercolors.cpp
    gui/glshadersimple.cpp
    gui/glshadertextured.cpp
    gui/glshadertvarray.cpp
    gui/glspectrum.cpp
    gui/glspectrumgui.cpp
    gui/graphicsviewzoom.cpp
    gui/httpdownloadmanagergui.cpp
    gui/indicator.cpp
    gui/levelmeter.cpp
    gui/loggingdialog.cpp
    gui/mypositiondialog.cpp
    gui/pluginsdialog.cpp
    gui/presetitem.cpp
    gui/rollupwidget.cpp
    gui/samplingdevicedialog.cpp
    gui/samplingdevicesdock.cpp
    gui/scaleengine.cpp
    gui/scaledimage.cpp
    gui/sdrangelsplash.cpp
    gui/spectrummarkersdialog.cpp
    gui/tickedslider.cpp
    gui/transverterbutton.cpp
    gui/transverterdialog.cpp
    gui/tvscreen.cpp
    gui/tvscreenanalog.cpp
    gui/valuedial.cpp
    gui/valuedialz.cpp
    gui/wsspectrumsettingsdialog.cpp
    gui/wrappingdatetimeedit.cpp

    dsp/scopevisxy.cpp

    device/deviceuiset.cpp

    channel/channelgui.cpp

    feature/featuregui.cpp
    feature/featureuiset.cpp

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
    gui/addpresetdialog.h
    gui/ambedevicesdialog.h
    gui/audiodialog.h
    gui/audioselectdialog.h
    gui/basicchannelsettingsdialog.h
    gui/basicdevicesettingsdialog.h
    gui/basicfeaturesettingsdialog.h
    gui/buttonswitch.h
    gui/channeladddialog.h
    gui/channelsdock.h
    gui/channelwindow.h
    gui/colormapper.h
    gui/commanditem.h
    gui/commandkeyreceiver.h
    gui/commandoutputdialog.h
    gui/crightclickenabler.h
    gui/customtextedit.h
    gui/cwkeyergui.h
    gui/devicestreamselectiondialog.h
    gui/deviceuserargsdialog.h
    gui/dmsspinbox.h
    gui/doublevalidator.h
    gui/editcommanddialog.h
    gui/externalclockbutton.h
    gui/externalclockdialog.h
    gui/fmpreemphasisdialog.h
    gui/featureadddialog.h
    gui/featuresdock.h
    gui/featurepresetsdialog.h
    gui/featurewindow.h
    gui/glscope.h
    gui/glscopegui.h
    gui/glshadercolors.h
    gui/glshadersimple.h
    gui/glshadertvarray.h
    gui/glshadertextured.h
    gui/glspectrum.h
    gui/glspectrumgui.h
    gui/graphicsviewzoom.h
    gui/httpdownloadmanagergui.h
    gui/indicator.h
    gui/levelmeter.h
    gui/loggingdialog.h
    gui/mypositiondialog.h
    gui/physicalunit.h
    gui/pluginsdialog.h
    gui/presetitem.h
    gui/rollupwidget.h
    gui/samplingdevicedialog.h
    gui/samplingdevicesdock.h
    gui/scaleengine.h
    gui/scaledimage.h
    gui/sdrangelsplash.h
    gui/spectrummarkersdialog.h
    gui/tickedslider.h
    gui/transverterbutton.h
    gui/transverterdialog.h
    gui/tvscreen.h
    gui/tvscreenanalog.h
    gui/valuedial.h
    gui/valuedialz.h
    gui/wsspectrumsettingsdialog.h
    gui/wrappingdatetimeedit.h

    dsp/scopevisxy.h

    device/deviceuiset.h

    channel/channelgui.h

    feature/featuregui.h
    feature/featureuiset.h

    device/devicegui.h

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
    mainwindow.ui
    gui/aboutdialog.ui
    gui/addpresetdialog.ui
    gui/ambedevicesdialog.ui
    gui/basicchannelsettingsdialog.ui
    gui/basicdevicesettingsdialog.ui
    gui/basicfeaturesettingsdialog.ui
    gui/channeladddialog.ui
    gui/commandoutputdialog.ui
    gui/cwkeyergui.ui
    gui/devicestreamselectiondialog.ui
    gui/deviceuserargsdialog.ui
    gui/editcommanddialog.ui
    gui/externalclockdialog.ui
    gui/fmpreemphasisdialog.ui
    gui/featureadddialog.ui
    gui/featurepresetsdialog.ui
    gui/glscopegui.ui
    gui/glspectrumgui.ui
    gui/pluginsdialog.ui
    gui/audiodialog.ui
    gui/audioselectdialog.ui
    gui/samplingdevicecontrol.ui
    gui/samplingdevicedialog.ui
    gui/spectrummarkersdialog.ui
    gui/myposdialog.ui
    gui/transverterdialog.ui
    gui/loggingdialog.ui
    gui/wsspectrumsettingsdialog.ui
    soapygui/discreterangegui.ui
    soapygui/intervalrangegui.ui
    soapygui/intervalslidergui.ui
    soapygui/complexfactorgui.ui
    soapygui/arginfogui.ui
)

if (LIMESUITE_FOUND)
  set(sdrgui_SOURCES
    ${sdrgui_SOURCES}
    limerfegui/limerfeusbdialog.cpp
  )
  set(sdrgui_HEADERS
    ${sdrgui_HEADERS}
    limerfegui/limerfeusbdialog.h
  )
  set(sdrgui_FORMS
    ${sdrgui_FORMS}
    limerfegui/limerfeusbdialog.ui
  )
  include_directories(${LIMESUITE_INCLUDE_DIR})
  set(sdrgui_LIMERFE_LIB ${LIMESUITE_LIBRARY})
endif (LIMESUITE_FOUND)

qt5_wrap_ui(sdrgui_FORMS_HEADERS ${sdrgui_FORMS})

include_directories(
  ${CMAKE_SOURCE_DIR}/exports
  ${CMAKE_SOURCE_DIR}/sdrbase
  ${CMAKE_SOURCE_DIR}/logging
  ${CMAKE_SOURCE_DIR}/httpserver
  ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
  ${OPENGL_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
)

add_library(sdrgui SHARED
  ${sdrgui_SOURCES}
  ${sdrgui_FORMS_HEADERS}
  )

target_link_libraries(sdrgui
  Qt5::Core
  Qt5::Widgets
  Qt5::OpenGL
  Qt5::Multimedia
  ${OPENGL_LIBRARIES}
  ${sdrgui_LIMERFE_LIB}
  sdrbase
  logging
  )

install(TARGETS sdrgui DESTINATION ${INSTALL_LIB_DIR})
