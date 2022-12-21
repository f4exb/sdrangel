project (sdrbase)

if(WIN32)
    # This should probably be in ../CMakeLists.txt with the other variables like it
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

if (LIBSERIALDV_FOUND)
    add_definitions(-DDSD_USE_SERIALDV)
    include_directories(${LIBSERIALDV_INCLUDE_DIR})
    set(sdrbase_SERIALDV_LIB ${LIBSERIALDV_LIBRARY})
endif()

set(sdrbase_SOURCES
    ${sdrbase_SOURCES}

    audio/audiocompressor.cpp
    audio/audiocompressorsnd.cpp
    audio/audiodeviceinfo.cpp
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
    commands/commandkeyreceiver.cpp

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
    dsp/recursivefilters.cpp
    dsp/wfir.cpp
    dsp/devicesamplesource.cpp
    dsp/devicesamplesink.cpp
    dsp/devicesamplemimo.cpp
    dsp/devicesamplestatic.cpp
    dsp/spectrummarkers.cpp
    dsp/spectrumcalibrationpoint.cpp
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

    pipes/datafifostore.cpp
    pipes/datapipes.cpp
    pipes/datapipesgcworker.cpp
    pipes/messagepipes.cpp
    pipes/messagepipesgcworker.cpp
    pipes/messagequeuestore.cpp
    pipes/objectpipe.cpp
    pipes/objectpipesregistrations.cpp

    settings/configuration.cpp
    settings/featuresetpreset.cpp
    settings/preferences.cpp
    settings/preset.cpp
    settings/mainsettings.cpp
    settings/rollupstate.cpp

    util/ais.cpp
    util/android.cpp
    util/aviationweather.cpp
    util/ax25.cpp
    util/aprs.cpp
    util/astronomy.cpp
    util/azel.cpp
    util/colormap.cpp
    util/coordinates.cpp
    util/crc.cpp
    util/CRC64.cpp
    util/csv.cpp
    util/db.cpp
    util/fixedtraits.cpp
    util/fits.cpp
    util/flightinformation.cpp
    util/giro.cpp
    util/golay2312.cpp
    util/httpdownloadmanager.cpp
    util/interpolation.cpp
    util/lfsr.cpp
    util/maidenhead.cpp
    util/message.cpp
    util/messagequeue.cpp
    util/morse.cpp
    util/openaip.cpp
    util/osndb.cpp
    util/peakfinder.cpp
    util/planespotters.cpp
    util/png.cpp
    util/prettyprint.cpp
    util/radiosonde.cpp
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
    util/weather.cpp
    util/iot/device.cpp
    util/iot/homeassistant.cpp
    util/iot/tplink.cpp
    util/iot/visa.cpp

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

    audio/audiocompressor.h
    audio/audiocompressorsnd.h
    audio/audiodeviceinfo.h
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
    commands/commandkeyreceiver.h

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
    dsp/spectrumcalibrationpoint.h
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

    pipes/datafifostore.h
    pipes/datapipes.h
    pipes/datapipesgcworker.h
    pipes/elementpipescommon.h
    pipes/elementpipesgc.h
    pipes/messagepipes.h
    pipes/messagepipesgcworker.h
    pipes/messagequeuestore.h
    pipes/objectpipe.h
    pipes/objectpipesregistrations.h

    plugin/plugininterface.h
    plugin/pluginapi.h
    plugin/pluginmanager.h

    settings/configuration.h
    settings/featuresetpreset.h
    settings/preferences.h
    settings/preset.h
    settings/mainsettings.h
    settings/rollupstate.h

    util/ais.h
    util/android.h
    util/aviationweather.h
    util/ax25.h
    util/aprs.h
    util/astronomy.h
    util/azel.h
    util/colormap.h
    util/coordinates.h
    util/CRC64.h
    util/csv.h
    util/db.h
    util/doublebuffer.h
    util/doublebufferfifo.h
    util/doublebuffermultiple.h
    util/fixedtraits.h
    util/fits.h
    util/flightinformation.h
    util/giro.h
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
    util/openaip.h
    util/osndb.h
    util/peakfinder.h
    util/planespotters.h
    util/png.h
    util/prettyprint.h
    util/radiosonde.h
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
    util/weather.h
    util/iot/device.h
    util/iot/homeassistant.h
    util/iot/tplink.h
    util/iot/visa.h

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

if(DEFINED FFTW3F_DEPENDS)
    add_dependencies(sdrbase "${FFTW3F_DEPENDS}")
endif()

if(DEFINED OPUS_DEPENDS)
    add_dependencies(sdrbase "${OPUS_DEPENDS}")
endif()

target_link_libraries(sdrbase
    ${OPUS_LIBRARIES}
    ${sdrbase_FFTW3F_LIB}
    ${sdrbase_SERIALDV_LIB}
    ${sdrbase_LIMERFE_LIB}
    ${sdrbase_LIBSIGMF_LIB}
    Qt::Core
    Qt::Multimedia
    Qt::WebSockets
    Qt::Positioning
    httpserver
    logging
    qrtplib
    swagger
)
if (Qt6_FOUND)
    target_link_libraries(sdrbase
        Qt::Core5Compat
        Qt::CorePrivate
    )
endif()
if(ANDROID AND NOT ENABLE_QT6)
    target_link_libraries(sdrbase Qt::AndroidExtras)
endif()
if(ANDROID)
    target_link_libraries(sdrbase log)
endif()

install(TARGETS sdrbase DESTINATION ${INSTALL_LIB_DIR})

if(WIN32 AND Qt6_FOUND)
    # Run deployqt for Core5Compat
    include(DeployQt)
    windeployqt(sdrbase ${SDRANGEL_BINARY_BIN_DIR} "")
endif()

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:sdrbase> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_LIB_DIR} )
endif()
