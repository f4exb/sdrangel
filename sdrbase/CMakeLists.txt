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

if (LIBSIGMF_FOUND AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(sdrbase_SOURCES
        ${sdrbase_SOURCES}
        dsp/sigmffilerecord.cpp
    )
    set(sdrbase_HEADERS
        ${sdrbase_HEADERS}
        dsp/sigmf_forward.h
        dsp/sigmffilerecord.h
    )
    include_directories(${LIBSIGMF_INCLUDE_DIR})
    set(sdrbase_LIBSIGMF_LIB ${LIBSIGMF_LIBRARIES})
endif()

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
    audio/audiooutputdevice.cpp
    audio/audioinputdevice.cpp
    audio/audionetsink.cpp
    audio/audioresampler.cpp

    channel/channelapi.cpp
    channel/channelutils.cpp
    channel/channelwebapiutils.cpp
    channel/remotedataqueue.cpp
    channel/remotedatareadqueue.cpp

    commands/command.cpp

    dsp/afsquelch.cpp
    dsp/agc.cpp
    dsp/downchannelizer.cpp
    dsp/upchannelizer.cpp
    dsp/channelmarker.cpp
    dsp/ctcssdetector.cpp
    dsp/ctcssfrequencies.cpp
    dsp/channelsamplesink.cpp
    dsp/channelsamplesource.cpp
    dsp/costasloop.cpp
    dsp/cwkeyer.cpp
    dsp/cwkeyersettings.cpp
    dsp/datafifo.cpp
    dsp/dcscodes.cpp
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
    dsp/firfilter.cpp
    dsp/fmpreemphasis.cpp
    dsp/freqlockcomplex.cpp
    dsp/interpolator.cpp
    dsp/glscopesettings.cpp
    dsp/spectrumsettings.cpp
    dsp/goertzel.cpp
    dsp/hbfilterchainconverter.cpp
    dsp/hbfiltertraits.cpp
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
    dsp/scopevis.cpp
    dsp/basebandsamplesink.cpp
    dsp/basebandsamplesource.cpp
    dsp/nullsink.cpp
    dsp/recursivefilters.cpp
    dsp/wfir.cpp
    dsp/devicesamplesource.cpp
    dsp/devicesamplesink.cpp
    dsp/devicesamplemimo.cpp
    dsp/devicesamplestatic.cpp
    dsp/spectrummarkers.cpp
    dsp/spectrumvis.cpp
    dsp/wavfilerecord.cpp

    device/deviceapi.cpp
    device/deviceenumerator.cpp
    device/deviceset.cpp
    device/deviceuserargs.cpp
    device/deviceutils.cpp

    feature/feature.cpp
    feature/featureset.cpp
    feature/featureutils.cpp
    feature/featurewebapiutils.cpp

    limerfe/limerfeusbcalib.cpp

    pipes/datapipes.cpp
    pipes/datapipescommon.cpp
    pipes/datapipesgcworker.cpp
    pipes/messagepipes.cpp
    pipes/messagepipescommon.cpp
    pipes/messagepipesgcworker.cpp
    pipes/pipeendpoint.cpp

    settings/featuresetpreset.cpp
    settings/preferences.cpp
    settings/preset.cpp
    settings/mainsettings.cpp

    util/ais.cpp
    util/ax25.cpp
    util/aprs.cpp
    util/astronomy.cpp
    util/azel.cpp
    util/crc.cpp
    util/CRC64.cpp
    util/csv.cpp
    util/db.cpp
    util/fixedtraits.cpp
    util/fits.cpp
    util/golay2312.cpp
    util/httpdownloadmanager.cpp
    util/interpolation.cpp
    util/lfsr.cpp
    util/maidenhead.cpp
    util/message.cpp
    util/messagequeue.cpp
    util/morse.cpp
    util/prettyprint.cpp
    util/rtpsink.cpp
    util/syncmessenger.cpp
    util/samplesourceserializer.cpp
    util/simpleserializer.cpp
    util/serialutil.cpp
    #util/spinlock.cpp
    util/uid.cpp
    util/units.cpp
    util/timeutil.cpp
    util/visa.cpp

    plugin/plugininterface.cpp
    plugin/pluginapi.cpp
    plugin/pluginmanager.cpp

    webapi/webapiadapter.cpp
    webapi/webapiadapterbase.cpp
    webapi/webapiadapterinterface.cpp
    webapi/webapirequestmapper.cpp
    webapi/webapiserver.cpp
    webapi/webapiutils.cpp

    websockets/wsspectrum.cpp

    mainparser.cpp
    maincore.cpp

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
    audio/audiooutputdevice.h
    audio/audioopus.h
    audio/audioinputdevice.h
    audio/audionetsink.h
    audio/audioresampler.h

    channel/channelapi.h
    channel/channelutils.h
    channel/channelwebapiutils.h
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
    dsp/costasloop.h
    dsp/ctcssdetector.h
    dsp/ctcssfrequencies.h
    dsp/cwkeyer.h
    dsp/cwkeyersettings.h
    dsp/datafifo.h
    dsp/dcscodes.h
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
    dsp/firfilter.h
    dsp/fmpreemphasis.h
    dsp/freqlockcomplex.h
    dsp/gfft.h
    dsp/glscopesettings.h
    dsp/glspectruminterface.h
    dsp/spectrummarkers.h
    dsp/spectrumsettings.h
    dsp/goertzel.h
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
    dsp/firfilter.h
    dsp/gaussian.h
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
    dsp/rootraisedcosine.h
    dsp/samplemififo.h
    dsp/samplemofifo.h
    dsp/samplesinkfifo.h
    dsp/samplesimplefifo.h
    dsp/samplesourcefifo.h
    dsp/scopevis.h
    dsp/basebandsamplesink.h
    dsp/basebandsamplesource.h
    dsp/nullsink.h
    dsp/wfir.h
    dsp/devicesamplesource.h
    dsp/devicesamplesink.h
    dsp/devicesamplemimo.h
    dsp/devicesamplestatic.h
    dsp/spectrumvis.h
    dsp/wavfilerecord.h

    device/deviceapi.h
    device/deviceenumerator.h
    device/deviceset.h
    device/deviceuserargs.h
    device/deviceutils.h

    feature/feature.h
    feature/featureset.h
    feature/featureutils.h
    feature/featurewebapiutils.h

    limerfe/limerfeusbcalib.h

    pipes/datapipes.h
    pipes/datapipescommon.h
    pipes/datapipesgcworker.h
    pipes/elementpipescommon.h
    pipes/elementpipesgc.h
    pipes/messagepipes.h
    pipes/messagepipescommon.h
    pipes/messagepipesgcworker.h
    pipes/pipeendpoint.h

    plugin/plugininterface.h
    plugin/pluginapi.h
    plugin/pluginmanager.h

    settings/featuresetpreset.h
    settings/preferences.h
    settings/preset.h
    settings/mainsettings.h

    util/ais.h
    util/ax25.h
    util/aprs.h
    util/astronomy.h
    util/azel.h
    util/CRC64.h
    util/csv.h
    util/db.h
    util/doublebuffer.h
    util/doublebufferfifo.h
    util/doublebuffermultiple.h
    util/fixedtraits.h
    util/fits.h
    util/golay2312.h
    util/httpdownloadmanager.h
    util/incrementalarray.h
    util/incrementalvector.h
    util/interpolation.h
    util/lfsr.h
    util/maidenhead.h
    util/message.h
    util/messagequeue.h
    util/morse.h
    util/movingaverage.h
    util/prettyprint.h
    util/rtpsink.h
    util/syncmessenger.h
    util/samplesourceserializer.h
    util/simpleserializer.h
    util/serialutil.h
    #util/spinlock.h
    util/uid.h
    util/units.h
    util/timeutil.h
    util/visa.h

    webapi/webapiadapter.h
    webapi/webapiadapterbase.h
    webapi/webapiadapterinterface.h
    webapi/webapirequestmapper.h
    webapi/webapiserver.h
    webapi/webapiutils.h

    websockets/wsspectrum.h

    mainparser.h
    maincore.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/httpserver
    ${CMAKE_SOURCE_DIR}/logging
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
    ${sdrbase_LIBSIGMF_LIB}
    Qt5::Core
    Qt5::Multimedia
    Qt5::WebSockets
    httpserver
    logging
    qrtplib
    swagger
)

install(TARGETS sdrbase DESTINATION ${INSTALL_LIB_DIR})
