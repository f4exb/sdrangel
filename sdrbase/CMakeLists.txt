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
endif(FFTW3F_FOUND)

# Kiss FFT is always available
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

# Vulkan SDK: https://vulkan.lunarg.com/
# Windows Vulkan SDK is missing glslang_c_interface.h
# See bug: https://vulkan.lunarg.com/issue/view/63d158a85df11200d569b2ab
# Copy it from Linux SDK
if (${VKFFT_BACKEND} EQUAL 0)
    find_package(Vulkan)
    if(Vulkan_FOUND)
        set(sdrbase_SOURCES
            ${sdrbase_SOURCES}
            dsp/vulkanvkfftengine.cpp
            dsp/vulkanvkfftengine.h
        )
    endif()
endif()

# CUDA Toolkit: https://developer.nvidia.com/cuda-downloads
if (${VKFFT_BACKEND} EQUAL 1)
    find_package(CUDAToolkit)    
    if(CUDA_FOUND)
        enable_language(CUDA)
        set(sdrbase_SOURCES
            ${sdrbase_SOURCES}
            dsp/cudavkfftengine.cpp
            dsp/cudavkfftengine.h
        )
    endif()
    if(WIN32)
        set(WINDOWS_FIXUP_BUNDLE_LIB_DIRS ${WINDOWS_FIXUP_BUNDLE_LIB_DIRS} ${CUDA_TOOLKIT_ROOT_DIR}/bin PARENT_SCOPE)
    endif()
endif()

if((Vulkan_FOUND AND (${VKFFT_BACKEND} EQUAL 0)) OR (CUDA_FOUND AND (${VKFFT_BACKEND} EQUAL 1)))
    set(sdrbase_SOURCES
        ${sdrbase_SOURCES}
        dsp/vkfftengine.cpp
        dsp/vkfftengine.h
        dsp/vkfftutils.cpp
        dsp/vkfftutils.h
    )
    include_directories(${VKFFT_INCLUDE_DIR})
endif()

if (LIBSIGMF_FOUND)
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
    dsp/fftnr.cpp
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
    dsp/morsedemod.cpp
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
    settings/pluginpreset.cpp
    settings/preferences.cpp
    settings/preset.cpp
    settings/mainsettings.cpp
    settings/rollupstate.cpp

    util/airlines.cpp
    util/ais.cpp
    util/android.cpp
    util/aprsfi.cpp
    util/aviationweather.cpp
    util/ax25.cpp
    util/aprs.cpp
    util/astronomy.cpp
    util/azel.cpp
    util/baudot.cpp
    util/callsign.cpp
    util/colormap.cpp
    util/coordinates.cpp
    util/countrydat.cpp
    util/crc.cpp
    util/CRC64.cpp
    util/csv.cpp
    util/db.cpp
    util/dsc.cpp
    util/fixedtraits.cpp
    util/fits.cpp
    util/flightinformation.cpp
    util/ft8message.cpp
    util/giro.cpp
    util/goesxray.cpp
    util/golay2312.cpp
    util/grb.cpp
    util/httpdownloadmanager.cpp
    util/interpolation.cpp
    util/kiwisdrlist.cpp
    util/lfsr.cpp
    util/maidenhead.cpp
    util/message.cpp
    util/messagequeue.cpp
    util/mmsi.cpp
    util/morse.cpp
    util/nasaglobalimagery.cpp
    util/navtex.cpp
    util/openaip.cpp
    util/osndb.cpp
    util/ourairportsdb.cpp
    util/peakfinder.cpp
    util/planespotters.cpp
    util/png.cpp
    util/prettyprint.cpp
    util/profiler.cpp
    util/psk31.cpp
    util/radiosonde.cpp
    util/rainviewer.cpp
    util/rtpsink.cpp
    util/syncmessenger.cpp
    util/samplesourceserializer.cpp
    util/sdrangelserverlist.cpp
    util/simpleserializer.cpp
    util/serialutil.cpp
    util/socket.cpp
    util/solardynamicsobservatory.cpp
    util/sondehub.cpp
    #util/spinlock.cpp
    util/spyserverlist.cpp
    util/stix.cpp
    util/rtty.cpp
    util/uid.cpp
    util/units.cpp
    util/timeutil.cpp
    util/visa.cpp
    util/vlftransmitters.cpp
    util/waypoints.cpp
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

    availablechannelorfeature.cpp
    availablechannelorfeaturehandler.cpp
    mainparser.cpp
    maincore.cpp
    remotetcpsinkstarter.cpp

    resources/data.qrc
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
    dsp/fftnr.h
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
    dsp/morsedemod.h
    dsp/nco.h
    dsp/ncof.h
    dsp/phasediscri.h
    dsp/phaselock.h
    dsp/phaselockcomplex.h
    dsp/projector.h
    dsp/raisedcosine.h
    dsp/recursivefilters.h
    dsp/replaybuffer.h
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
    settings/pluginpreset.h
    settings/preferences.h
    settings/preset.h
    settings/mainsettings.h
    settings/rollupstate.h

    util/airlines.h
    util/ais.h
    util/android.h
    util/aprsfi.h
    util/aviationweather.h
    util/ax25.h
    util/aprs.h
    util/astronomy.h
    util/azel.h
    util/baudot.h
    util/callsign.h
    util/colormap.h
    util/coordinates.h
    util/countrydat.h
    util/CRC64.h
    util/csv.h
    util/db.h
    util/dsc.h
    util/doublebuffer.h
    util/doublebufferfifo.h
    util/doublebuffermultiple.h
    util/fixedtraits.h
    util/fits.h
    util/flightinformation.h
    util/ft8message.h
    util/giro.h
    util/goesxray.h
    util/golay2312.h
    util/grb.h
    util/httpdownloadmanager.h
    util/incrementalarray.h
    util/incrementalvector.h
    util/interpolation.h
    util/kiwisdrlist.h
    util/lfsr.h
    util/maidenhead.h
    util/message.h
    util/messagequeue.h
    util/mmsi.h
    util/morse.h
    util/movingaverage.h
    util/movingmaximum.h
    util/nasaglobalimagery.h
    util/navtex.h
    util/openaip.h
    util/osndb.h
    util/outairportsdb.h
    util/peakfinder.h
    util/planespotters.h
    util/png.h
    util/prettyprint.h
    util/profiler.h
    util/psk31.h
    util/radiosonde.h
    util/rainviewer.h
    util/rtpsink.h
    util/rtty.h
    util/syncmessenger.h
    util/samplesourceserializer.h
    util/sdrangelserverlist.h
    util/simpleserializer.h
    util/serialutil.h
    util/socket.h
    util/solardynamicsobservatory.h
    util/sondehub.h
    #util/spinlock.h
    util/spyserverlist.h
    util/stix.h
    util/uid.h
    util/units.h
    util/timeutil.h
    util/visa.h
    util/vlftransmitters.h
    util/waypoints.h
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

    availablechannelorfeature.h
    availablechannelorfeaturehandler.h
    mainparser.h
    maincore.h
    remotetcpsinkstarter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/httpserver
    ${CMAKE_SOURCE_DIR}/logging
    ${CMAKE_SOURCE_DIR}/qrtplib
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
    ${OPUS_INCLUDE_DIRS}
    ${Qt${QT_DEFAULT_MAJOR_VERSION}Gui_PRIVATE_INCLUDE_DIRS}
)

add_library(sdrbase
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

if(DEFINED LIBSIGMF_DEPENDS)
    add_dependencies(sdrbase "${LIBSIGMF_DEPENDS}")
endif()

if(Vulkan_FOUND AND (${VKFFT_BACKEND} EQUAL 0))
    target_compile_definitions(sdrbase PUBLIC -DVK_API_VERSION=11)
    target_include_directories(sdrbase PUBLIC ${Vulkan_INCLUDE_DIR} ${Vulkan_INCLUDE_DIR}/glslang/Include)
    add_compile_definitions(sdrbase VKFFT_BACKEND=0)

    find_library(VULKAN_SPIRV_LIB SPIRV HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_SPVREMAPPER_LIB SPVRemapper HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_SPIRV_TOOLS_LIB SPIRV-Tools HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_SPIRV_TOOLS_OPT_LIB SPIRV-Tools-opt HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_SPIRV_TOOLS_SHARED_LIB SPIRV-Tools-shared HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_OSDEPENDENT_LIB OSDependent HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_GLSLANG_LIB glslang HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_GLSLANG_RES_LIB glslang-default-resource-limits HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_MACHINEINDEPENDENT_LIB MachineIndependent HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_GENERICCODEGEN_LIB GenericCodeGen HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)

    if(MSVC)
        set(VULKAN_DEBUG_POSTFIX "d")
    else()
        set(VULKAN_DEBUG_POSTFIX "")
    endif()
    find_library(VULKAN_SPIRVD_LIB SPIRV${VULKAN_DEBUG_POSTFIX} HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_SPVREMAPPERD_LIB SPVRemapper${VULKAN_DEBUG_POSTFIX} HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_SPIRV_TOOLSD_LIB SPIRV-Tools${VULKAN_DEBUG_POSTFIX} HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_SPIRV_TOOLS_OPTD_LIB SPIRV-Tools-opt${VULKAN_DEBUG_POSTFIX} HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_SPIRV_TOOLS_SHAREDD_LIB SPIRV-Tools-shared${VULKAN_DEBUG_POSTFIX} HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_OSDEPENDENTD_LIB OSDependent${VULKAN_DEBUG_POSTFIX} HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_GLSLANGD_LIB glslang${VULKAN_DEBUG_POSTFIX} HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_GLSLANG_RESD_LIB glslang-default-resource-limits${VULKAN_DEBUG_POSTFIX} HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_MACHINEINDEPENDENTD_LIB MachineIndependent${VULKAN_DEBUG_POSTFIX} HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
    find_library(VULKAN_GENERICCODEGEND_LIB GenericCodeGen${VULKAN_DEBUG_POSTFIX} HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)

    target_link_libraries(sdrbase
        optimized ${VULKAN_SPIRV_LIB}
        optimized ${VULKAN_SPVREMAPPER_LIB}
        optimized ${VULKAN_SPIRV_TOOLS_LIB}
        optimized ${VULKAN_SPIRV_TOOLS_OPT_LIB}
        optimized ${VULKAN_SPIRV_TOOLS_SHARED_LIB}
        optimized ${VULKAN_OSDEPENDENT_LIB}
        optimized ${VULKAN_GLSLANG_LIB}
        optimized ${VULKAN_GLSLANG_RES_LIB}
        optimized ${VULKAN_MACHINEINDEPENDENT_LIB}
        optimized ${VULKAN_GENERICCODEGEN_LIB}
        optimized Vulkan::Vulkan
        debug ${VULKAN_SPIRVD_LIB}
        debug ${VULKAN_SPVREMAPPERD_LIB}
        debug ${VULKAN_SPIRV_TOOLSD_LIB}
        debug ${VULKAN_SPIRV_TOOLS_OPTD_LIB}
        debug ${VULKAN_SPIRV_TOOLS_SHAREDD_LIB}
        debug ${VULKAN_OSDEPENDENTD_LIB}
        debug ${VULKAN_GLSLANGD_LIB}
        debug ${VULKAN_GLSLANG_RESD_LIB}
        debug ${VULKAN_MACHINEINDEPENDENTD_LIB}
        debug ${VULKAN_GENERICCODEGEND_LIB}
        debug Vulkan::Vulkan
    )
endif()

if(CUDA_FOUND AND (${VKFFT_BACKEND} EQUAL 1))
    set_property(TARGET sdrbase PROPERTY CUDA_ARCHITECTURES 60 70 75 80 86)
    add_compile_definitions(sdrbase VKFFT_BACKEND=1)
    target_compile_options(sdrbase PUBLIC
        "$<$<COMPILE_LANGUAGE:CUDA>:SHELL:
        -DVKFFT_BACKEND=1
        -gencode arch=compute_60,code=compute_60
        -gencode arch=compute_70,code=compute_70
        -gencode arch=compute_75,code=compute_75
        -gencode arch=compute_80,code=compute_80
        -gencode arch=compute_86,code=compute_86>")
    set_target_properties(sdrbase PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
    set_target_properties(sdrbase PROPERTIES CUDA_RESOLVE_DEVICE_SYMBOLS ON)
    find_library(CUDA_NVRTC_LIB libnvrtc nvrtc HINTS "${CUDA_TOOLKIT_ROOT_DIR}/lib64" "${LIBNVRTC_LIBRARY_DIR}" "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64" /usr/lib64 /usr/local/cuda/lib64 REQUIRED)
    find_library(CUDA_LIB cuda HINTS "${CUDA_TOOLKIT_ROOT_DIR}/lib64" "${LIBNVRTC_LIBRARY_DIR}" "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64" /usr/lib64 /usr/local/cuda/lib64 REQUIRED)
    target_link_libraries(sdrbase ${CUDA_LIBRARIES} ${CUDA_LIB} ${CUDA_NVRTC_LIB})
    target_include_directories(sdrbase PUBLIC ${CUDA_INCLUDE_DIRS})
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
if (LIBSIGMF_FOUND)
    if(WIN32)
        target_link_libraries(sdrbase optimized ${LIBSIGMF_LIBRARIES})
        target_link_libraries(sdrbase debug ${LIBSIGMF_LIBRARIES_DEBUG})
    else()
        target_link_libraries(sdrbase ${LIBSIGMF_LIBRARIES})
    endif()
endif()
if (Qt6_FOUND)
    target_link_libraries(sdrbase
        Qt::CorePrivate
    )
endif()
if(ANDROID AND NOT ENABLE_QT6)
    target_link_libraries(sdrbase Qt::AndroidExtras)
endif()
if(ANDROID)
    target_link_libraries(sdrbase log)
endif()

target_compile_definitions(sdrbase PRIVATE
	-DPLUGIN_PREFIX=\"${PLUGIN_PREFIX}\"
)

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
