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
    gui/buttonswitch.cpp
    gui/channelwindow.cpp
    gui/clickablelabel.cpp
    gui/colormapper.cpp
    gui/commanditem.cpp
    gui/commandkeyreceiver.cpp
    gui/commandoutputdialog.cpp
    gui/comboboxnoarrow.cpp
    gui/crightclickenabler.cpp
    gui/cwkeyergui.cpp
    gui/devicestreamselectiondialog.cpp
    gui/deviceuserargsdialog.cpp
    gui/editcommanddialog.cpp
    gui/externalclockbutton.cpp
    gui/externalclockdialog.cpp
    gui/glscope.cpp
    gui/glscopegui.cpp
    gui/glshadercolors.cpp
    gui/glshadersimple.cpp
    gui/glshadertextured.cpp
    gui/glshadertvarray.cpp
    gui/glspectrum.cpp
    gui/glspectrumgui.cpp
    gui/indicator.cpp
    gui/levelmeter.cpp
    gui/loggingdialog.cpp
    gui/mypositiondialog.cpp
    gui/pluginsdialog.cpp
    gui/presetitem.cpp
    gui/rollupwidget.cpp
    gui/samplingdevicecontrol.cpp
    gui/samplingdevicedialog.cpp
    gui/scaleengine.cpp
    gui/sdrangelsplash.cpp
    gui/tickedslider.cpp
    gui/transverterbutton.cpp
    gui/transverterdialog.cpp
    gui/tvscreen.cpp
    gui/valuedial.cpp
    gui/valuedialz.cpp

    dsp/scopevis.cpp
    dsp/scopevisxy.cpp
    dsp/spectrumvis.cpp
    dsp/spectrumscopecombovis.cpp

    device/deviceuiset.cpp

    soapygui/discreterangegui.cpp
    soapygui/intervalrangegui.cpp
    soapygui/itemsettinggui.cpp
    soapygui/stringrangegui.cpp
    soapygui/dynamicitemsettinggui.cpp
    soapygui/dynamicargsettinggui.cpp
    soapygui/intervalslidergui.cpp
    soapygui/complexfactorgui.cpp
    soapygui/arginfogui.cpp

    webapi/webapiadaptergui.cpp

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
    gui/buttonswitch.h
    gui/channelwindow.h
    gui/colormapper.h
    gui/commanditem.h
    gui/commandkeyreceiver.h
    gui/commandoutputdialog.h
    gui/comboboxnoarrow.h
    gui/crightclickenabler.h
    gui/cwkeyergui.h
    gui/devicestreamselectiondialog.h
    gui/deviceuserargsdialog.h
    gui/doublevalidator.h
    gui/editcommanddialog.h
    gui/externalclockbutton.h
    gui/externalclockdialog.h
    gui/glscope.h
    gui/glscopegui.h
    gui/glshadercolors.h
    gui/glshadersimple.h
    gui/glshadertvarray.h
    gui/glshadertextured.h
    gui/glspectrum.h
    gui/glspectrumgui.h
    gui/indicator.h
    gui/levelmeter.h
    gui/loggingdialog.h
    gui/mypositiondialog.h
    gui/physicalunit.h
    gui/pluginsdialog.h
    gui/presetitem.h
    gui/rollupwidget.h
    gui/samplingdevicecontrol.h
    gui/samplingdevicedialog.h
    gui/scaleengine.h
    gui/sdrangelsplash.h
    gui/tickedslider.h
    gui/transverterbutton.h
    gui/transverterdialog.h
    gui/tvscreen.h
    gui/valuedial.h
    gui/valuedialz.h

    dsp/scopevis.h
    dsp/scopevisxy.h
    dsp/spectrumvis.h
    dsp/spectrumscopecombovis.h

    device/deviceuiset.h

    soapygui/discreterangegui.h
    soapygui/intervalrangegui.h
    soapygui/itemsettinggui.h
    soapygui/stringrangegui.h
    soapygui/dynamicitemsettinggui.h
    soapygui/dynamicargsettinggui.h
    soapygui/intervalslidergui.h
    soapygui/complexfactorgui.h
    soapygui/arginfogui.h

    webapi/webapiadaptergui.h
)

set(sdrgui_FORMS
  mainwindow.ui
  gui/aboutdialog.ui
  gui/addpresetdialog.ui
  gui/ambedevicesdialog.ui
  gui/basicchannelsettingsdialog.ui
  gui/basicdevicesettingsdialog.ui
  gui/commandoutputdialog.ui
  gui/cwkeyergui.ui
  gui/devicestreamselectiondialog.ui
  gui/deviceuserargsdialog.ui
  gui/editcommanddialog.ui
  gui/externalclockdialog.ui
  gui/glscopegui.ui
  gui/glspectrumgui.ui
  gui/pluginsdialog.ui
  gui/audiodialog.ui
  gui/audioselectdialog.ui
  gui/samplingdevicecontrol.ui
  gui/samplingdevicedialog.ui
  gui/myposdialog.ui
  gui/transverterdialog.ui
  gui/loggingdialog.ui
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
