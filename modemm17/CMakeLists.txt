project(modemm17)

set(modemm17_SOURCES
    ClockRecovery.cpp
    Correlator.cpp
    FreqDevEstimator.cpp
    Golay24.cpp
    LinkSetupFrame.cpp
    M17Demodulator.cpp
    M17Modulator.cpp
    M17Randomizer.cpp
)

set(modemm17_HEADERS
    ax25_frame.h
    CarrierDetect.h
    ClockRecovery.h
    Convolution.h
    Correlator.h
    CRC16.h
    DataCarrierDetect.h
    DeviationError.h
    Filter.h
    FirFilter.h
    FreqDevEstimator.h
    FrequencyError.h
    Fsk4Demod.h
    Golay24.h
    IirFilter.h
    LinkSetupFrame.h
    M17Demodulator.h
    M17FrameDecoder.h
    M17Framer.h
    M17Modulator.h
    M17Randomizer.h
    M17Synchronizer.h
    PhaseEstimator.h
    PolynomialInterleaver.h
    SlidingDFT.h
    SymbolEvm.h
    Trellis.h
    Util.h
    Viterbi.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/exports
)

add_library(modemm17 SHARED
	${modemm17_SOURCES}
)

target_link_libraries(modemm17
    Qt::Core
)

install(TARGETS modemm17 DESTINATION ${INSTALL_LIB_DIR})
