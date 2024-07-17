project(wdsp)

set(wdsp_SOURCES
    ../custom/apple/apple_compat.c
    amd.cpp
    ammod.cpp
    amsq.cpp
    anb.cpp
    anf.cpp
    anr.cpp
    bandpass.cpp
    bldr.cpp
    bps.cpp
    bpsnba.cpp
    calculus.cpp
    cblock.cpp
    cfcomp.cpp
    cfir.cpp
    compress.cpp
    delay.cpp
    emnr.cpp
    emph.cpp
    eq.cpp
    fcurve.cpp
    fir.cpp
    fircore.cpp
    firmin.cpp
    firopt.cpp
    fmd.cpp
    fmmod.cpp
    fmsq.cpp
    gain.cpp
    gen.cpp
    icfir.cpp
    iir.cpp
    iqc.cpp
    lmath.cpp
    meter.cpp
    meterlog10.cpp
    nbp.cpp
    nob.cpp
    osctrl.cpp
    patchpanel.cpp
    resample.cpp
    resamplef.cpp
    rmatch.cpp
    RXA.cpp
    sender.cpp
    shift.cpp
    siphon.cpp
    slew.cpp
    snba.cpp
    ssql.cpp
    TXA.cpp
    varsamp.cpp
    wcpAGC.cpp
)

set(wdsp_HEADERS
    amd.hpp
    ammod.hpp
    amsq.hpp
    anb.hpp
    anf.hpp
    anr.hpp
    bandpass.hpp
    bldr.hpp
    bps.hpp
    bpsnba.hpp
    bufferprobe.hpp
    calculus.hpp
    cblock.hpp
    cfcomp.hpp
    cfir.hpp
    comm.hpp
    compress.hpp
    delay.hpp
    emnr.hpp
    emph.hpp
    eq.hpp
    fcurve.hpp
    fir.hpp
    fircore.hpp
    firmin.hpp
    firopt.hpp
    fmd.hpp
    fmmod.hpp
    fmsq.hpp
    gain.hpp
    gen.hpp
    icfir.hpp
    iir.hpp
    iqc.hpp
    lmath.hpp
    meter.hpp
    meterlog10.hpp
    nbp.hpp
    nob.hpp
    osctrl.hpp
    patchpanel.hpp
    resample.hpp
    resamplef.hpp
    rmatch.hpp
    RXA.hpp
    sender.hpp
    shift.hpp
    siphon.hpp
    slew.hpp
    snba.hpp
    ssql.hpp
    TXA.hpp
    varsamp.hpp
    wcpAGC.hpp
)

include_directories(
    ${CMAKE_SOURCE_DIR}/exports
    ${CUSTOM_APPLE_INCLUDE}
    ${FFTW3F_INCLUDE_DIRS}
)

add_library(wdsp SHARED
    ${wdsp_SOURCES}
)

target_link_libraries(wdsp
    ${FFTW3F_LIBRARIES}
    Qt::Core
)

set_target_properties(wdsp PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS true)
if (MSVC)
    set_target_properties(wdsp PROPERTIES INTERPROCEDURAL_OPTIMIZATION false)
endif()

install(TARGETS wdsp DESTINATION ${INSTALL_LIB_DIR})

if (LINUX)
    add_executable(wdsp_make_interface
        make_interface.cpp
    )

    add_executable(wdsp_make_calculus
        make_calculus.cpp
    )
    install(TARGETS wdsp_make_interface wdsp_make_calculus DESTINATION ${INSTALL_BIN_DIR})
endif()
