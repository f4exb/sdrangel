project (sdrbase)

if(WIN32)
    set(OPUS_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/external/windows/libopus/include")
    set(OPUS_LIBRARIES "${CMAKE_SOURCE_DIR}/external/windows/libopus/lib/x64/libopus.lib")
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

if (LIMESUITE_FOUND)
    set(sdrbase_SOURCES
        ${sdrbase_SOURCES}
        limerfe/limerfecontroller.cpp
    )
    set(sdrbase_HEADERS
        ${sdrbase_HEADERS}
        limerfe/limerfecontroller.h
    )
    include_directories(${LIMESUITE_INCLUDE_DIR})
    set(sdrbase_LIMERFE_LIB ${LIMESUITE_LIBRARY})
endif (LIMESUITE_FOUND)

# serialdv now required
add_definitions(-DDSD_USE_SERIALDV)
include_directories(${LIBSERIALDV_INCLUDE_DIR})
set(sdrbase_SERIALDV_LIB ${LIBSERIALDV_LIBRARY})

set(sdrbase_SOURCES
    ${sdrbase_SOURCES}
    ambe/ambeengine.cpp
    ambe/ambeworker.cpp

    audio/audiocompressor.cpp
    audio/audiocompressorsnd.cpp
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
    channel/channelutils.cpp
    channel/remotedataqueue.cpp
    channel/remotedatareadqueue.cpp

    commands/command.cpp

    dsp/afsquelch.cpp
    dsp/agc.cpp
    dsp/downchannelizer.cpp
    dsp/upchannelizer.cpp
    dsp/channelmarker.cpp
    dsp/ctcssdetector.cpp
    dsp/channelsamplesink.cpp
    dsp/channelsamplesource.cpp
    dsp/cwkeyer.cpp
    dsp/cwkeyersettings.cpp
    dsp/decimatorsif.cpp
    dsp/decimatorsff.cpp
    dsp/decimatorsfi.cpp
    dsp/decimatorc.cpp
    dsp/dspcommands.cpp
    dsp/dspengine.cpp
    dsp/dspdevicesourceengine.cpp
    dsp/dspdevicesinkengine.cpp
    dsp/dspdevicemimoengine.cpp
    dsp/fftcorr.cpp
    dsp/fftengine.cpp
    dsp/fftfactory.cpp
    dsp/fftfilt.cpp
    dsp/fftwindow.cpp
    dsp/filterrc.cpp
    dsp/filtermbe.cpp
    dsp/filerecord.cpp
    dsp/filerecordinterface.cpp
    dsp/fmpreemphasis.cpp
    dsp/freqlockcomplex.cpp
    dsp/interpolator.cpp
    dsp/glscopesettings.cpp
    dsp/glspectrumsettings.cpp
    dsp/hbfilterchainconverter.cpp
    dsp/hbfiltertraits.cpp
    dsp/lowpass.cpp
    dsp/mimochannel.cpp
    dsp/nco.cpp
    dsp/ncof.cpp
    dsp/phaselock.cpp
    dsp/phaselockcomplex.cpp
    dsp/projector.cpp
    dsp/samplemififo.cpp
    dsp/samplemofifo.cpp
    dsp/samplesinkfifo.cpp
    dsp/samplesimplefifo.cpp
    dsp/samplesourcefifo.cpp
    dsp/samplesourcefifodb.cpp
    dsp/basebandsamplesink.cpp
    dsp/basebandsamplesource.cpp
    dsp/nullsink.cpp
    dsp/recursivefilters.cpp
    dsp/wfir.cpp
    dsp/devicesamplesource.cpp
    dsp/devicesamplesink.cpp
    dsp/devicesamplemimo.cpp
    dsp/devicesamplestatic.cpp
    dsp/spectrumvis.cpp

    device/deviceapi.cpp
    device/deviceenumerator.cpp
    device/deviceuserargs.cpp
    device/deviceutils.cpp

    limerfe/limerfeusbcalib.cpp

    settings/preferences.cpp
    settings/preset.cpp
    settings/mainsettings.cpp

    util/crc.cpp
    util/CRC64.cpp
    util/db.cpp
    util/fixedtraits.cpp
    util/lfsr.cpp
    util/message.cpp
    util/messagequeue.cpp
    util/prettyprint.cpp
    util/rtpsink.cpp
    util/syncmessenger.cpp
    util/samplesourceserializer.cpp
    util/simpleserializer.cpp
    util/serialutil.cpp
    #util/spinlock.cpp
    util/uid.cpp
    util/timeutil.cpp

    plugin/plugininterface.cpp
    plugin/pluginapi.cpp
    plugin/pluginmanager.cpp

    webapi/webapiadapterbase.cpp
    webapi/webapiadapterinterface.cpp
    webapi/webapirequestmapper.cpp
    webapi/webapiserver.cpp

    websockets/wsspectrum.cpp

    mainparser.cpp

    resources/webapi.qrc
)

set(sdrbase_HEADERS
    ${sdrbase_HEADERS}
    ambe/ambeengine.h
    ambe/ambeworker.h

    audio/audiocompressor.h
    audio/audiocompressorsnd.h
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
    channel/channelutils.h
    channel/remotedataqueue.h
    channel/remotedatareadqueue.h
    channel/remotedatablock.h

    commands/command.h

    dsp/afsquelch.h
    dsp/autocorrector.h
    dsp/downchannelizer.h
    dsp/upchannelizer.h
    dsp/channelmarker.h
    dsp/channelsamplesink.h
    dsp/channelsamplesource.h
    dsp/complex.h
    dsp/cwkeyer.h
    dsp/cwkeyersettings.h
    dsp/decimators.h
    dsp/decimatorsif.h
    dsp/decimatorsff.h
    dsp/decimatorsfi.h
    dsp/decimatorsu.h
    dsp/decimatorc.h
    dsp/interpolators.h
    dsp/interpolatorsif.h
    dsp/dspcommands.h
    dsp/dspengine.h
    dsp/dspdevicesourceengine.h
    dsp/dspdevicesinkengine.h
    dsp/dspdevicemimoengine.h
    dsp/dsptypes.h
    dsp/fftcorr.h
    dsp/fftengine.h
    dsp/fftfactory.h
    dsp/fftfilt.h
    dsp/fftwengine.h
    dsp/fftwindow.h
    dsp/filterrc.h
    dsp/filtermbe.h
    dsp/filerecord.h
    dsp/filerecordinterface.h
    dsp/fmpreemphasis.h
    dsp/freqlockcomplex.h
    dsp/gfft.h
    dsp/glscopesettings.h
    dsp/glspectrumsettings.h
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
    dsp/mimochannel.h
    dsp/misc.h
    dsp/movingaverage.h
    dsp/nco.h
    dsp/ncof.h
    dsp/phasediscri.h
    dsp/phaselock.h
    dsp/phaselockcomplex.h
    dsp/projector.h
    dsp/raisedcosine.h
    dsp/recursivefilters.h
    dsp/samplemififo.h
    dsp/samplemofifo.h
    dsp/samplesinkfifo.h
    dsp/samplesimplefifo.h
    dsp/samplesourcefifo.h
    dsp/samplesourcefifodb.h
    dsp/basebandsamplesink.h
    dsp/basebandsamplesource.h
    dsp/nullsink.h
    dsp/wfir.h
    dsp/devicesamplesource.h
    dsp/devicesamplesink.h
    dsp/devicesamplemimo.h
    dsp/devicesamplestatic.h
    dsp/spectrumvis.h

    device/deviceapi.h
    device/deviceenumerator.h
    device/deviceuserargs.h
    device/deviceutils.h

    limerfe/limerfeusbcalib.h

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
    util/incrementalarray.h
    util/incrementalvector.h
    util/lfsr.h
    util/message.h
    util/messagequeue.h
    util/movingaverage.h
    util/prettyprint.h
    util/rtpsink.h
    util/syncmessenger.h
    util/samplesourceserializer.h
    util/simpleserializer.h
    util/serialutil.h
    #util/spinlock.h
    util/uid.h
    util/timeutil.h

    webapi/webapiadapterbase.h
    webapi/webapiadapterinterface.h
    webapi/webapirequestmapper.h
    webapi/webapiserver.h

    websockets/wsspectrum.h

    mainparser.h
)

include_directories(
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

if(LIMESUITE_EXTERNAL)
    add_dependencies(sdrbase limesuite)
endif()

if(LIBSERIALDV_EXTERNAL)
    add_dependencies(sdrbase serialdv)
endif()

target_link_libraries(sdrbase
    ${OPUS_LIBRARIES}
    ${sdrbase_FFTW3F_LIB}
    ${sdrbase_SERIALDV_LIB}
    ${sdrbase_LIMERFE_LIB}
    Qt5::Core
    Qt5::Multimedia
    Qt5::WebSockets
    httpserver
    qrtplib
    swagger
)

install(TARGETS sdrbase DESTINATION ${INSTALL_LIB_DIR})
