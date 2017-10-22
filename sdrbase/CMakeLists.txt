project (sdrbase)

set(sdrbase_SOURCES
    mainwindow.cpp

    audio/audiodeviceinfo.cpp
    audio/audiofifo.cpp
    audio/audiooutput.cpp
    audio/audioinput.cpp

    dsp/afsquelch.cpp
    dsp/agc.cpp
    dsp/downchannelizer.cpp
    dsp/upchannelizer.cpp
    dsp/channelmarker.cpp
    dsp/ctcssdetector.cpp
    dsp/cwkeyer.cpp
    dsp/dspcommands.cpp
    dsp/dspengine.cpp
    dsp/dspdevicesourceengine.cpp
    dsp/dspdevicesinkengine.cpp
    dsp/fftengine.cpp
    dsp/fftfilt.cxx
    dsp/fftwindow.cpp
    dsp/filterrc.cpp
    dsp/filtermbe.cpp
    dsp/filerecord.cpp
    dsp/interpolator.cpp
    dsp/hbfiltertraits.cpp
    dsp/lowpass.cpp
    dsp/nco.cpp
    dsp/ncof.cpp
    dsp/pidcontroller.cpp
    dsp/phaselock.cpp
    dsp/samplesinkfifo.cpp
    dsp/samplesourcefifo.cpp
    dsp/samplesinkfifodoublebuffered.cpp
    dsp/basebandsamplesink.cpp
    dsp/basebandsamplesource.cpp
    dsp/nullsink.cpp
    dsp/recursivefilters.cpp
    dsp/spectrumscopecombovis.cpp
    dsp/spectrumscopengcombovis.cpp
    dsp/scopevis.cpp
    dsp/scopevisng.cpp
    dsp/scopevismulti.cpp
    dsp/spectrumvis.cpp
    dsp/threadedbasebandsamplesink.cpp
    dsp/threadedbasebandsamplesource.cpp
    dsp/wfir.cpp

    gui/aboutdialog.cpp
    gui/addpresetdialog.cpp
    gui/basicchannelsettingswidget.cpp
    gui/basicchannelsettingsdialog.cpp
    gui/buttonswitch.cpp
    gui/channelwindow.cpp
    gui/clickablelabel.cpp
    gui/colormapper.cpp
    gui/cwkeyergui.cpp
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
    gui/mypositiondialog.cpp
    gui/pluginsdialog.cpp
    gui/audiodialog.cpp
    gui/presetitem.cpp
    gui/rollupwidget.cpp
    gui/samplingdevicecontrol.cpp
    gui/scale.cpp
    gui/scaleengine.cpp
    gui/transverterbutton.cpp
    gui/transverterdialog.cpp
    gui/valuedial.cpp
    gui/valuedialz.cpp

    dsp/devicesamplesource.cpp
    dsp/devicesamplesink.cpp

    plugin/pluginapi.cpp
    plugin/plugininterface.cpp
    plugin/pluginmanager.cpp

    settings/preferences.cpp
    settings/preset.cpp
    settings/mainsettings.cpp

    util/CRC64.cpp
    util/db.cpp
    util/message.cpp
    util/messagequeue.cpp
    util/prettyprint.cpp
    util/syncmessenger.cpp
    util/samplesourceserializer.cpp
    util/simpleserializer.cpp
    #util/spinlock.cpp
    
    device/devicesourceapi.cpp
    device/devicesinkapi.cpp
)

set(sdrbase_HEADERS
    mainwindow.h

    audio/audiodeviceinfo.h
    audio/audiofifo.h
    audio/audiooutput.h
    audio/audioinput.h

    dsp/afsquelch.h
    dsp/downchannelizer.h
    dsp/upchannelizer.h
    dsp/channelmarker.h
    dsp/complex.h
    dsp/cwkeyer.h
    dsp/decimators.h
    dsp/interpolators.h
    dsp/dspcommands.h
    dsp/dspengine.h
    dsp/dspdevicesourceengine.h
    dsp/dspdevicesinkengine.h
    dsp/dsptypes.h
    dsp/fftengine.h
    dsp/fftfilt.h
    dsp/fftwengine.h
    dsp/fftwindow.h
    dsp/filterrc.h
    dsp/filtermbe.h
    dsp/filerecord.h
    dsp/gfft.h
    dsp/interpolator.h
    dsp/hbfiltertraits.h
    dsp/inthalfbandfilter.h
    dsp/inthalfbandfilterdb.h
    dsp/inthalfbandfiltereo1.h
    dsp/inthalfbandfiltereo1i.h
    dsp/inthalfbandfilterst.h
    dsp/inthalfbandfiltersti.h
    dsp/kissfft.h
    dsp/kissengine.h
    dsp/lowpass.h
    dsp/misc.h
    dsp/movingaverage.h
    dsp/nco.h
    dsp/ncof.h
    dsp/phasediscri.h
    dsp/phaselock.h
    dsp/pidcontroller.h
    dsp/recursivefilters.h
    dsp/samplesinkfifo.h
    dsp/samplesourcefifo.h
    dsp/samplesinkfifodoublebuffered.h
    dsp/samplesinkfifodecimator.h
    dsp/basebandsamplesink.h
    dsp/basebandsamplesource.h
    dsp/nullsink.h
    dsp/spectrumscopecombovis.h
    dsp/spectrumscopengcombovis.h
    dsp/scopevis.h
    dsp/scopevisng.h
    dsp/scopevismulti.h
    dsp/spectrumvis.h
    dsp/threadedbasebandsamplesink.h
    dsp/threadedbasebandsamplesource.h
    dsp/wfir.h

    gui/aboutdialog.h
    gui/addpresetdialog.h
    gui/basicchannelsettingswidget.h
    gui/basicchannelsettingsdialog.h
    gui/buttonswitch.h
    gui/channelwindow.h
    gui/colormapper.h
    gui/cwkeyergui.h
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
    gui/mypositiondialog.h
    gui/physicalunit.h
    gui/pluginsdialog.h
    gui/audiodialog.h
    gui/presetitem.h
    gui/rollupwidget.h
    gui/samplingdevicecontrol.h
    gui/scale.h
    gui/scaleengine.h
    gui/transverterbutton.h
    gui/transverterdialog.h    
    gui/valuedial.h
    gui/valuedialz.h

    dsp/devicesamplesource.h
    dsp/devicesamplesink.h

    plugin/pluginapi.h
    plugin/plugininstancegui.h
    plugin/plugininterface.h
    plugin/pluginmanager.h

    settings/preferences.h
    settings/preset.h
    settings/mainsettings.h

    util/CRC64.h
    util/db.h
    util/doublebuffer.h
    util/export.h
    util/message.h
    util/messagequeue.h
    util/movingaverage.h
    util/prettyprint.h
    util/syncmessenger.h
    util/samplesourceserializer.h
    util/simpleserializer.h
    #util/spinlock.h
    
    device/devicesourceapi.h
    device/devicesinkapi.h
)

set(sdrbase_SOURCES
    ${sdrbase_SOURCES}
    ${sdrbase_HEADERS}
)

set(sdrbase_FORMS
    mainwindow.ui

    gui/aboutdialog.ui
    gui/addpresetdialog.ui
    gui/basicchannelsettingswidget.ui
    gui/basicchannelsettingsdialog.ui
    gui/cwkeyergui.ui
    gui/glscopegui.ui
    gui/glscopenggui.ui
    gui/glscopemultigui.ui
    gui/glspectrumgui.ui
    gui/pluginsdialog.ui
    gui/audiodialog.ui
    gui/samplingdevicecontrol.ui
    gui/myposdialog.ui
    gui/transverterdialog.ui
)

set(sdrbase_RESOURCES
    resources/res.qrc
)

if(FFTW3F_FOUND)
    set(sdrbase_SOURCES
        ${sdrbase_SOURCES}
        dsp/fftwengine.cpp
    )
    set(sdrbase_HEADERS
        ${sdrbase_HEADERS}
        dsp/fftwengine.h
    )
    add_definitions(-DUSE_FFTW)
    include_directories(${FFTW3F_INCLUDE_DIRS})
else(FFTW3F_FOUND)
    set(sdrbase_SOURCES
        ${sdrbase_SOURCES}
        dsp/kissengine.cpp
        dsp/kissfft.h
    )
    set(sdrbase_HEADERS
        ${sdrbase_HEADERS}
        dsp/kissengine.h
    )
    add_definitions(-DUSE_KISSFFT)
endif(FFTW3F_FOUND)

if (LIBSERIALDV_FOUND)
    set(sdrbase_SOURCES
        ${sdrbase_SOURCES}
        dsp/dvserialworker.cpp
        dsp/dvserialengine.cpp
    )
    set(sdrbase_HEADERS
        ${sdrbase_HEADERS}
        dsp/dvserialworker.h
        dsp/dvserialengine.h
    )
    add_definitions(-DDSD_USE_SERIALDV)
    include_directories(${LIBSERIALDV_INCLUDE_DIR})
endif(LIBSERIALDV_FOUND)

if (BUILD_DEBIAN)
    set(sdrbase_SOURCES
        ${sdrbase_SOURCES}
        dsp/dvserialworker.cpp
        dsp/dvserialengine.cpp
    )
    set(sdrbase_HEADERS
        ${sdrbase_HEADERS}
        dsp/dvserialworker.h
        dsp/dvserialengine.h
    )
    add_definitions(-DDSD_USE_SERIALDV)
    include_directories(${LIBSERIALDVSRC})
endif (BUILD_DEBIAN)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)

qt5_wrap_ui(sdrbase_FORMS_HEADERS ${sdrbase_FORMS})
qt5_add_resources(sdrbase_RESOURCES_RCC ${sdrbase_RESOURCES})

if(WIN32)
    SET(sdrbase_SOURCES ${sdrbase_SOURCES} resources/sdrangel.rc)
endif(WIN32)

add_library(sdrbase SHARED
    ${sdrbase_SOURCES}
    ${sdrbase_HEADERS_MOC}
    ${sdrbase_FORMS_HEADERS}
    ${sdrbase_RESOURCES_RCC}
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${OPENGL_INCLUDE_DIR}
    .
)

target_link_libraries(sdrbase
    ${QT_LIBRARIES}
    ${OPENGL_LIBRARIES}
)

if(FFTW3F_FOUND)
    target_link_libraries(sdrbase ${FFTW3F_LIBRARIES})
endif(FFTW3F_FOUND)

if(LIBSERIALDV_FOUND)
    target_link_libraries(sdrbase ${LIBSERIALDV_LIBRARY})
endif(LIBSERIALDV_FOUND)

if (BUILD_DEBIAN)
    target_link_libraries(sdrbase serialdv)
endif (BUILD_DEBIAN)

set_target_properties(sdrbase PROPERTIES DEFINE_SYMBOL "sdrangel_EXPORTS")
target_compile_features(sdrbase PRIVATE cxx_generalized_initializers) # cmake >= 3.1.0

qt5_use_modules(sdrbase Core Widgets OpenGL Multimedia)

install(TARGETS sdrbase DESTINATION lib)

