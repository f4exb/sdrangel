project (sdrbase)

if(WIN32)
    set(OPUS_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/external/windows/opus/include")
    set(OPUS_LIBRARIES "${CMAKE_SOURCE_DIR}/external/windows/libopus-0.x64.dll")
endif()
find_package(Opus REQUIRED)

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
    set(sdrbase_FFTW3F_LIB ${FFTW3F_LIBRARIES})
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
    message(STATUS "Will have SerialDV support")
    add_definitions(-DDSD_USE_SERIALDV)
    include_directories(${LIBSERIALDV_INCLUDE_DIR})
    set(sdrbase_SERIALDV_LIB ${LIBSERIALDV_LIBRARY})
else(LIBSERIALDV_FOUND)
    message(STATUS "No SerialDV support")
endif(LIBSERIALDV_FOUND)

set(sdrbase_SOURCES
    ${sdrbase_SOURCES}
    audio/audiocompressor.cpp
    audio/audiodevicemanager.cpp
    audio/audiofifo.cpp
    audio/audiofilter.cpp
    audio/audiog722.cpp
    audio/audioopus.cpp
    audio/audiooutput.cpp
    audio/audioinput.cpp
    audio/audionetsink.cpp
    audio/audioresampler.cpp

    channel/channelapi.cpp
    channel/remotedataqueue.cpp
    channel/remotedatareadqueue.cpp

    commands/command.cpp

    dsp/afsquelch.cpp
    dsp/agc.cpp
    dsp/downchannelizer.cpp
    dsp/upchannelizer.cpp
    dsp/channelmarker.cpp
    dsp/ctcssdetector.cpp
    dsp/cwkeyer.cpp
    dsp/cwkeyersettings.cpp
    dsp/decimatorsif.cpp
    dsp/decimatorsff.cpp
    dsp/decimatorsfi.cpp
    dsp/dspcommands.cpp
    dsp/dspengine.cpp
    dsp/dspdevicesourceengine.cpp
    dsp/dspdevicesinkengine.cpp
    dsp/fftcorr.cpp
    dsp/fftengine.cpp
    dsp/fftfilt.cpp
    dsp/fftwindow.cpp
    dsp/filterrc.cpp
    dsp/filtermbe.cpp
    dsp/filerecord.cpp
    dsp/freqlockcomplex.cpp
    dsp/interpolator.cpp
    dsp/hbfilterchainconverter.cpp
    dsp/hbfiltertraits.cpp
    dsp/lowpass.cpp
    dsp/nco.cpp
    dsp/ncof.cpp
    dsp/phaselock.cpp
    dsp/phaselockcomplex.cpp
    dsp/projector.cpp
    dsp/samplesinkfifo.cpp
    dsp/samplesourcefifo.cpp
    dsp/samplesinkfifodoublebuffered.cpp
    dsp/basebandsamplesink.cpp
    dsp/basebandsamplesource.cpp
    dsp/nullsink.cpp
    dsp/recursivefilters.cpp
    dsp/threadedbasebandsamplesink.cpp
    dsp/threadedbasebandsamplesource.cpp
    dsp/wfir.cpp
    dsp/devicesamplesource.cpp
    dsp/devicesamplesink.cpp

    device/deviceapi.cpp
    device/deviceenumerator.cpp

    settings/preferences.cpp
    settings/preset.cpp
    settings/mainsettings.cpp

    util/CRC64.cpp
    util/db.cpp
    util/fixedtraits.cpp
    util/message.cpp
    util/messagequeue.cpp
    util/prettyprint.cpp
    util/rtpsink.cpp
    util/syncmessenger.cpp
    util/samplesourceserializer.cpp
    util/simpleserializer.cpp
    #util/spinlock.cpp
    util/uid.cpp
    util/timeutil.cpp

    plugin/plugininterface.cpp
    plugin/pluginapi.cpp
    plugin/pluginmanager.cpp

    webapi/webapiadapterinterface.cpp
    webapi/webapirequestmapper.cpp
    webapi/webapiserver.cpp

    mainparser.cpp

    resources/res.qrc
)

set(sdrbase_HEADERS
    ${sdrbase_HEADERS}
    audio/audiocompressor.h
    audio/audiodevicemanager.h
    audio/audiofifo.h
    audio/audiofilter.h
    audio/audiog722.h
    audio/audiooutput.h
    audio/audioopus.h
    audio/audioinput.h
    audio/audionetsink.h
    audio/audioresampler.h

    channel/channelapi.h
    channel/remotedataqueue.h
    channel/remotedatareadqueue.h
    channel/remotedatablock.h

    commands/command.h

    dsp/afsquelch.h
    dsp/autocorrector.h
    dsp/downchannelizer.h
    dsp/upchannelizer.h
    dsp/channelmarker.h
    dsp/complex.h
    dsp/cwkeyer.h
    dsp/cwkeyersettings.h
    dsp/decimators.h
    dsp/decimatorsif.h
    dsp/decimatorsff.h
    dsp/decimatorsfi.h
    dsp/decimatorsu.h
    dsp/interpolators.h
    dsp/dspcommands.h
    dsp/dspengine.h
    dsp/dspdevicesourceengine.h
    dsp/dspdevicesinkengine.h
    dsp/dsptypes.h
    dsp/fftcorr.h
    dsp/fftengine.h
    dsp/fftfilt.h
    dsp/fftwengine.h
    dsp/fftwindow.h
    dsp/filterrc.h
    dsp/filtermbe.h
    dsp/filerecord.h
    dsp/freqlockcomplex.h
    dsp/gfft.h
    dsp/hbfilterchainconverter.h
    dsp/iirfilter.h
    dsp/interpolator.h
    dsp/hbfiltertraits.h
    dsp/inthalfbandfilter.h
    dsp/inthalfbandfilterdb.h
    dsp/inthalfbandfilterdbf.h
    dsp/inthalfbandfiltereo.h
    # dsp/inthalfbandfiltereo1.h
    # dsp/inthalfbandfiltereo1i.h
    # dsp/inthalfbandfiltereo2.h
    dsp/inthalfbandfiltereof.h
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
    dsp/phaselockcomplex.h
    dsp/projector.h
    dsp/recursivefilters.h
    dsp/samplesinkfifo.h
    dsp/samplesourcefifo.h
    dsp/samplesinkfifodoublebuffered.h
    dsp/samplesinkfifodecimator.h
    dsp/basebandsamplesink.h
    dsp/basebandsamplesource.h
    dsp/nullsink.h
    dsp/threadedbasebandsamplesink.h
    dsp/threadedbasebandsamplesource.h
    dsp/wfir.h
    dsp/devicesamplesource.h
    dsp/devicesamplesink.h

    device/deviceapi.h
    device/deviceenumerator.h

    plugin/plugininstancegui.h
    plugin/plugininterface.h
    plugin/pluginapi.h
    plugin/pluginmanager.h

    settings/preferences.h
    settings/preset.h
    settings/mainsettings.h

    util/CRC64.h
    util/db.h
    util/doublebuffer.h
    util/doublebufferfifo.h
    util/fixedtraits.h
    util/message.h
    util/messagequeue.h
    util/movingaverage.h
    util/prettyprint.h
    util/rtpsink.h
    util/syncmessenger.h
    util/samplesourceserializer.h
    util/simpleserializer.h
    #util/spinlock.h
    util/uid.h
    util/timeutil.h

    webapi/webapiadapterinterface.h
    webapi/webapirequestmapper.h
    webapi/webapiserver

    mainparser.h
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/httpserver
    ${CMAKE_SOURCE_DIR}/qrtplib
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
    ${OPUS_INCLUDE_DIRS}
)

add_library(sdrbase SHARED
    ${sdrbase_SOURCES}
)

target_link_libraries(sdrbase
    ${OPUS_LIBRARIES}
    ${sdrbase_FFTW3F_LIB}
    ${sdrbase_SERIALDV_LIB}
    Qt5::Core
    Qt5::Multimedia
    httpserver
    qrtplib
    swagger
)

install(TARGETS sdrbase DESTINATION ${INSTALL_LIB_DIR})
