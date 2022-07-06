project(modems)

set(modems_SOURCES
    m17/Correlator.cpp
    m17/FreqDevEstimator.cpp
    m17/Golay24.cpp
    m17/M17Demodulator.cpp
    m17/M17Modulator.cpp
)

set(modems_HEADERS
    m17/ax25_frame.h
    m17/CarrierDetect.h
    m17/ClockRecovery.h
    m17/Convolution.h
    m17/Correlator.h
    m17/CRC16.h
    m17/DataCarrierDetect.h
    m17/DeviationError.h
    m17/Filter.h
    m17/FirFilter.h
    m17/FreqDevEstimator.h
    m17/FrequencyError.h
    m17/Fsk4Demod.h
    m17/Golay24.h
    m17/IirFilter.h
    m17/LinkSetupFrame.h
    m17/M17Demodulator.h
    m17/M17FrameDecoder.h
    m17/M17Framer.h
    m17/M17Modulator.h
    m17/M17Randomizer.h
    m17/M17Synchronizer.h
    m17/PhaseEstimator.h
    m17/PolynomialInterleaver.h
    m17/queue.h
    m17/SlidingDFT.h
    m17/SymbolEvm.h
    m17/Trellis.h
    m17/Util.h
    m17/Viterbi.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/exports
)

add_library(modems SHARED
	${modems_SOURCES}
)

target_link_libraries(modems
    Qt5::Core
)

install(TARGETS modems DESTINATION ${INSTALL_LIB_DIR})
