project(liquiddsp)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

file(READ "${LIQUIDDSPSRC}/include/liquid.h" liquid_h)
string(REGEX MATCH "\\#define LIQUID_VERSION[ ]+\"([0-9]+\\.[0-9]+\\.[0-9]+)\"" LIQUID_VERSION_MATCHES "${liquid_h}")
if(NOT LIQUID_VERSION_MATCHES)
    message(FATAL_ERROR "Failed to extract version number from liquid.h")
endif(NOT LIQUID_VERSION_MATCHES)
set(LIQUID_VERSION ${CMAKE_MATCH_1})

message("-- Configuring for liquid-dsp ${LIQUID_VERSION}")

include(CheckCPUID)
include(CheckRequiredFunction)
include(CheckTypeSize)

include_directories(${LIQUIDDSPSRC})
include_directories(${LIQUIDDSPSRC}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

CHECK_INCLUDE_FILE(sys/resource.h HAVE_SYS_RESOURCE_H)

CHECK_INCLUDE_FILE(complex.h HAVE_COMPLEX_H)
CHECK_INCLUDE_FILE(float.h HAVE_FLOAT_H)
CHECK_INCLUDE_FILE(getopt.h HAVE_GETOPT_H)
CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)

CHECK_INCLUDE_FILE(mmintrin.h HAVE_MMINTRIN_H)
CHECK_INCLUDE_FILE(emmintrin.h HAVE_EMMINTRIN_H)
CHECK_INCLUDE_FILE(immintrin.h HAVE_IMMINTRIN_H)
CHECK_INCLUDE_FILE(pmmintrin.h HAVE_PMMINTRIN_H)
CHECK_INCLUDE_FILE(smmintrin.h HAVE_SMMINTRIN_H)
CHECK_INCLUDE_FILE(tmmintrin.h HAVE_TMMINTRIN_H)
CHECK_INCLUDE_FILE(xmmintrin.h HAVE_XMMINTRIN_H)

CHECK_INCLUDE_FILE(fec.h HAVE_FEC_H)
CHECK_INCLUDE_FILE(fftw3.h HAVE_FFTW3_H)

CHECK_TYPE_SIZE(int SIZEOF_INT)
CHECK_TYPE_SIZE("unsigned int" SIZEOF_UNSIGNED_INT)

CHECK_REQUIRED_FUNCTION(sinf m HAVE_SINF)
CHECK_REQUIRED_FUNCTION(cosf m HAVE_COSF)
CHECK_REQUIRED_FUNCTION(expf m HAVE_EXPF)
CHECK_REQUIRED_FUNCTION(cargf m HAVE_CARGF)
CHECK_REQUIRED_FUNCTION(cexpf m HAVE_CEXPF)
CHECK_REQUIRED_FUNCTION(crealf m HAVE_CREALF)
CHECK_REQUIRED_FUNCTION(cimagf m HAVE_CIMAGF)
CHECK_REQUIRED_FUNCTION(sqrtf m HAVE_SQRTF)

CHECK_LIBRARY_EXISTS(m sqrtf "" HAVE_LIBM)
if (HAVE_FEC_H)
    CHECK_LIBRARY_EXISTS(fec create_viterbi27 "" HAVE_LIBFEC)
    if (NOT HAVE_LIBFEC)
        unset(HAVE_FEC_H CACHE)
    endif ()
endif ()
if (HAVE_FFTW3_H)
    CHECK_LIBRARY_EXISTS(fftw3f fftwf_plan_dft_1d "" HAVE_LIBFFTW3F)
    if (NOT HAVE_LIBFFTW3F)
        unset(HAVE_FFTW3_H CACHE)
    endif ()
endif ()

CHECK_CPUID("mmx" HAVE_MMX ${LIQUID_FORCE_X86_MMX})
CHECK_CPUID("sse" HAVE_SSE ${LIQUID_FORCE_X86_SSE})
CHECK_CPUID("sse2" HAVE_SSE2 ${LIQUID_FORCE_X86_SSE2})
CHECK_CPUID("sse3" HAVE_SSE3 ${LIQUID_FORCE_X86_SSE3})
CHECK_CPUID("ssse3" HAVE_SSSE3 ${LIQUID_FORCE_X86_SSSE3})
CHECK_CPUID("sse41" HAVE_SSE41 ${LIQUID_FORCE_X86_SSE41})
CHECK_CPUID("sse42" HAVE_SSE42 ${LIQUID_FORCE_X86_SSE42})
CHECK_CPUID("avx" HAVE_AVX ${LIQUID_FORCE_X86_AVX})
CHECK_CPUID("vmx" HAVE_ALTIVEC ${LIQUID_FORCE_PPC_ALTIVEC})
CHECK_CPUID("neon" HAVE_NEON ${LIQUID_FORCE_ARM_NEON})
CHECK_CPUID("neon64" HAVE_NEON64 ${LIQUID_FORCE_ARM64_NEON})

#
# Build compilation extra flags
#
if (CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    if (HAVE_AVX)
        set(_EXTRA_C_FLAGS "-mavx")
    elseif (HAVE_SSE42)
        set(_EXTRA_C_FLAGS "-msse4.2")
    elseif (HAVE_SSE41)
        set(_EXTRA_C_FLAGS "-msse4.1")
    elseif (HAVE_SSSE3)
        set(_EXTRA_C_FLAGS "-mssse3")
    elseif (HAVE_SSE3)
        set(_EXTRA_C_FLAGS "-msse3")
    elseif (HAVE_SSE2)
        set(_EXTRA_C_FLAGS "-msse2")
    elseif (HAVE_SSE)
        set(_EXTRA_C_FLAGS "-msse")
    elseif (HAVE_MMX)
        set(_EXTRA_C_FLAGS "-mmmx")
    elseif (HAVE_ALTIVEC)
        if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
            set(_EXTRA_C_FLAGS "-fno-common -faltivec")
        else ()
            set(_EXTRA_C_FLAGS "-maltivec")
        endif ()
    elseif (HAVE_NEON)
        set(_EXTRA_C_FLAGS "-mfpu=neon-vfpv4")
    elseif (HAVE_NEON64)
        # No extra flags needed
    endif ()
    set(_EXTRA_C_FLAGS "${_EXTRA_C_FLAGS} -ffast-math")
endif ()

#
# MODULE : agc - automatic gain control
#

set(agc_SOURCES
    ${LIQUIDDSPSRC}/src/agc/src/agc_crcf.c
    ${LIQUIDDSPSRC}/src/agc/src/agc_rrrf.c
    )

#
# MODULE : audio
#

set(audio_SOURCES
    ${LIQUIDDSPSRC}/src/audio/src/cvsd.c
    )

#
# MODULE : buffer
#

set(buffer_SOURCES
    ${LIQUIDDSPSRC}/src/buffer/src/bufferf.c
    ${LIQUIDDSPSRC}/src/buffer/src/buffercf.c
    )

#
# MODULE : channel
#

set(channel_SOURCES
    ${LIQUIDDSPSRC}/src/channel/src/channel_cccf.c
    )

#
# MODULE : dotprod
#

set(dotprod_C_SOURCES
    ${LIQUIDDSPSRC}/src/dotprod/src/dotprod_cccf.c
    ${LIQUIDDSPSRC}/src/dotprod/src/dotprod_crcf.c
    ${LIQUIDDSPSRC}/src/dotprod/src/dotprod_rrrf.c
    ${LIQUIDDSPSRC}/src/dotprod/src/sumsq.c
    )

# PowerPC AltiVec
set(dotprod_ALTIVEC_SOURCES
    ${LIQUIDDSPSRC}/src/dotprod/src/dotprod_cccf.c
    ${LIQUIDDSPSRC}/src/dotprod/src/dotprod_crcf.av.c
    ${LIQUIDDSPSRC}/src/dotprod/src/dotprod_rrrf.av.c
    ${LIQUIDDSPSRC}/src/dotprod/src/sumsq.c
    )

# Intel MMX/SSE/AVX
set(dotprod_SSE_SOURCES
    ${LIQUIDDSPSRC}/src/dotprod/src/dotprod_cccf.mmx.c
    ${LIQUIDDSPSRC}/src/dotprod/src/dotprod_crcf.mmx.c
    ${LIQUIDDSPSRC}/src/dotprod/src/dotprod_rrrf.mmx.c
    ${LIQUIDDSPSRC}/src/dotprod/src/sumsq.mmx.c
    )

# ARM NEON
set(dotprod_NEON_SOURCES
    ${LIQUIDDSPSRC}/src/dotprod/src/dotprod_cccf.neon.c
    ${LIQUIDDSPSRC}/src/dotprod/src/dotprod_crcf.neon.c
    ${LIQUIDDSPSRC}/src/dotprod/src/dotprod_rrrf.neon.c
    ${LIQUIDDSPSRC}/src/dotprod/src/sumsq.c
    )

if (LIQUID_SIMDOVERRIDE)
    set(dotprod_SOURCES ${dotprod_C_SOURCES})
elseif (HAVE_SSE41 AND HAVE_SMMINTRIN_H)
    set(dotprod_SOURCES ${dotprod_SSE_SOURCES})
elseif (HAVE_SSE3 AND HAVE_PMMINTRIN_H)
    set(dotprod_SOURCES ${dotprod_SSE_SOURCES})
elseif (HAVE_SSE2 AND HAVE_EMMINTRIN_H)
    unset(HAVE_PMMINTRIN_H CACHE) # Unset otherwise SSE3 code kicks in.
    set(dotprod_SOURCES ${dotprod_SSE_SOURCES})
elseif (HAVE_ALTIVEC)
    set(dotprod_SOURCES ${dotprod_ALTIVEC_SOURCES})
elseif (HAVE_NEON OR HAVE_NEON64)
    set(dotprod_SOURCES ${dotprod_NEON_SOURCES})
else ()
    set(dotprod_SOURCES ${dotprod_C_SOURCES})
endif ()

#
# MODULE : equalization
#

set(equalization_SOURCES
    ${LIQUIDDSPSRC}/src/equalization/src/equalizer_cccf.c
    ${LIQUIDDSPSRC}/src/equalization/src/equalizer_rrrf.c
    )

#
# MODULE : fec - forward error-correction
#
set(fec_SOURCES
    ${LIQUIDDSPSRC}/src/fec/src/crc.c
    ${LIQUIDDSPSRC}/src/fec/src/fec.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_conv.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_conv_poly.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_conv_pmatrix.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_conv_punctured.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_golay2412.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_hamming74.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_hamming84.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_hamming128.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_hamming1511.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_hamming3126.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_hamming128_gentab.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_pass.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_rep3.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_rep5.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_rs.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_secded2216.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_secded3932.c
    ${LIQUIDDSPSRC}/src/fec/src/fec_secded7264.c
    ${LIQUIDDSPSRC}/src/fec/src/interleaver.c
    ${LIQUIDDSPSRC}/src/fec/src/packetizer.c
    ${LIQUIDDSPSRC}/src/fec/src/sumproduct.c
    )

#
# MODULE : fft - fast Fourier transforms, discrete sine/cosine transforms, etc.
#
set(fft_SOURCES
    ${LIQUIDDSPSRC}/src/fft/src/fftf.c
    ${LIQUIDDSPSRC}/src/fft/src/spgramcf.c
    ${LIQUIDDSPSRC}/src/fft/src/spgramf.c
    ${LIQUIDDSPSRC}/src/fft/src/fft_utilities.c
    )

#
# MODULE : filter
#
set(filter_SOURCES
	${LIQUIDDSPSRC}/src/filter/src/bessel.c
	${LIQUIDDSPSRC}/src/filter/src/butter.c
	${LIQUIDDSPSRC}/src/filter/src/cheby1.c
    ${LIQUIDDSPSRC}/src/filter/src/cheby2.c
	${LIQUIDDSPSRC}/src/filter/src/ellip.c
	${LIQUIDDSPSRC}/src/filter/src/filter_rrrf.c
	${LIQUIDDSPSRC}/src/filter/src/filter_crcf.c
	${LIQUIDDSPSRC}/src/filter/src/filter_cccf.c
	${LIQUIDDSPSRC}/src/filter/src/firdes.c
	${LIQUIDDSPSRC}/src/filter/src/firdespm.c
	${LIQUIDDSPSRC}/src/filter/src/fnyquist.c
	${LIQUIDDSPSRC}/src/filter/src/gmsk.c
	${LIQUIDDSPSRC}/src/filter/src/group_delay.c
	${LIQUIDDSPSRC}/src/filter/src/hM3.c
	${LIQUIDDSPSRC}/src/filter/src/iirdes.pll.c
	${LIQUIDDSPSRC}/src/filter/src/iirdes.c
	${LIQUIDDSPSRC}/src/filter/src/lpc.c
	${LIQUIDDSPSRC}/src/filter/src/rcos.c
	${LIQUIDDSPSRC}/src/filter/src/rkaiser.c
	${LIQUIDDSPSRC}/src/filter/src/rrcos.c
    )

#
# MODULE : framing
#
set(framing_SOURCES
    ${LIQUIDDSPSRC}/src/framing/src/bpacketgen.c
    ${LIQUIDDSPSRC}/src/framing/src/bpacketsync.c
    ${LIQUIDDSPSRC}/src/framing/src/bpresync_cccf.c
    ${LIQUIDDSPSRC}/src/framing/src/bsync_rrrf.c
    ${LIQUIDDSPSRC}/src/framing/src/bsync_crcf.c
    ${LIQUIDDSPSRC}/src/framing/src/bsync_cccf.c
    ${LIQUIDDSPSRC}/src/framing/src/detector_cccf.c
    ${LIQUIDDSPSRC}/src/framing/src/framedatastats.c
    ${LIQUIDDSPSRC}/src/framing/src/framesyncstats.c
    ${LIQUIDDSPSRC}/src/framing/src/framegen64.c
    ${LIQUIDDSPSRC}/src/framing/src/framesync64.c
    ${LIQUIDDSPSRC}/src/framing/src/flexframegen.c
    ${LIQUIDDSPSRC}/src/framing/src/flexframesync.c
    ${LIQUIDDSPSRC}/src/framing/src/gmskframegen.c
    ${LIQUIDDSPSRC}/src/framing/src/gmskframesync.c
    ${LIQUIDDSPSRC}/src/framing/src/msourcecf.c
    ${LIQUIDDSPSRC}/src/framing/src/ofdmflexframegen.c
    ${LIQUIDDSPSRC}/src/framing/src/ofdmflexframesync.c
    ${LIQUIDDSPSRC}/src/framing/src/presync_cccf.c
    ${LIQUIDDSPSRC}/src/framing/src/symstreamcf.c
    ${LIQUIDDSPSRC}/src/framing/src/symtrack_cccf.c
    ${LIQUIDDSPSRC}/src/framing/src/qdetector_cccf.c
    ${LIQUIDDSPSRC}/src/framing/src/qpacketmodem.c
    ${LIQUIDDSPSRC}/src/framing/src/qpilotgen.c
    ${LIQUIDDSPSRC}/src/framing/src/qpilotsync.c
    )
 
#
# MODULE : math
#
set(math_SOURCES
    ${LIQUIDDSPSRC}/src/math/src/poly.c
    ${LIQUIDDSPSRC}/src/math/src/polyc.c
    ${LIQUIDDSPSRC}/src/math/src/polyf.c
    ${LIQUIDDSPSRC}/src/math/src/polycf.c
    ${LIQUIDDSPSRC}/src/math/src/math.c
    ${LIQUIDDSPSRC}/src/math/src/math.bessel.c
    ${LIQUIDDSPSRC}/src/math/src/math.gamma.c
    ${LIQUIDDSPSRC}/src/math/src/math.complex.c
    ${LIQUIDDSPSRC}/src/math/src/math.trig.c
    ${LIQUIDDSPSRC}/src/math/src/modular_arithmetic.c
    ${LIQUIDDSPSRC}/src/math/src/windows.c
    )
 
#
# MODULE : matrix
#
set(matrix_SOURCES
    ${LIQUIDDSPSRC}/src/matrix/src/matrix.c
    ${LIQUIDDSPSRC}/src/matrix/src/matrixf.c
    ${LIQUIDDSPSRC}/src/matrix/src/matrixc.c
    ${LIQUIDDSPSRC}/src/matrix/src/matrixcf.c
    ${LIQUIDDSPSRC}/src/matrix/src/smatrix.common.c
    ${LIQUIDDSPSRC}/src/matrix/src/smatrixb.c
    ${LIQUIDDSPSRC}/src/matrix/src/smatrixf.c
    ${LIQUIDDSPSRC}/src/matrix/src/smatrixi.c
    )
 
#
# MODULE : modem
#
set(modem_SOURCES
    ${LIQUIDDSPSRC}/src/modem/src/ampmodem.c
    ${LIQUIDDSPSRC}/src/modem/src/cpfskdem.c
    ${LIQUIDDSPSRC}/src/modem/src/cpfskmod.c
    ${LIQUIDDSPSRC}/src/modem/src/fskdem.c
    ${LIQUIDDSPSRC}/src/modem/src/fskmod.c
    ${LIQUIDDSPSRC}/src/modem/src/gmskdem.c
    ${LIQUIDDSPSRC}/src/modem/src/gmskmod.c
    ${LIQUIDDSPSRC}/src/modem/src/modemf.c
    ${LIQUIDDSPSRC}/src/modem/src/modem_utilities.c
    ${LIQUIDDSPSRC}/src/modem/src/modem_apsk_const.c
    ${LIQUIDDSPSRC}/src/modem/src/modem_arb_const.c
    )
 
#
# MODULE : multichannel
#
set(multichannel_SOURCES
    ${LIQUIDDSPSRC}/src/multichannel/src/firpfbch_crcf.c
    ${LIQUIDDSPSRC}/src/multichannel/src/firpfbch_cccf.c
    ${LIQUIDDSPSRC}/src/multichannel/src/ofdmframe.common.c
    ${LIQUIDDSPSRC}/src/multichannel/src/ofdmframegen.c
    ${LIQUIDDSPSRC}/src/multichannel/src/ofdmframesync.c
    )
 
#
# MODULE : nco - numerically-controlled oscillator
#
set(nco_SOURCES
    ${LIQUIDDSPSRC}/src/nco/src/nco_crcf.c
    ${LIQUIDDSPSRC}/src/nco/src/nco.utilities.c
    )
 
#
# MODULE : optim - optimization
#
set(optim_SOURCES
    ${LIQUIDDSPSRC}/src/optim/src/chromosome.c
    ${LIQUIDDSPSRC}/src/optim/src/gasearch.c
    ${LIQUIDDSPSRC}/src/optim/src/gradsearch.c
    ${LIQUIDDSPSRC}/src/optim/src/optim.common.c
    ${LIQUIDDSPSRC}/src/optim/src/qnsearch.c
    ${LIQUIDDSPSRC}/src/optim/src/utilities.c
    )

#
# MODULE : quantization
#
set(quantization_SOURCES
    ${LIQUIDDSPSRC}/src/quantization/src/compand.c
    ${LIQUIDDSPSRC}/src/quantization/src/quantizercf.c
    ${LIQUIDDSPSRC}/src/quantization/src/quantizerf.c
    ${LIQUIDDSPSRC}/src/quantization/src/quantizer.inline.c
    )
    
#
# MODULE : random
#
set(random_SOURCES
    ${LIQUIDDSPSRC}/src/random/src/rand.c
    ${LIQUIDDSPSRC}/src/random/src/randn.c
    ${LIQUIDDSPSRC}/src/random/src/randexp.c
    ${LIQUIDDSPSRC}/src/random/src/randweib.c
    ${LIQUIDDSPSRC}/src/random/src/randgamma.c
    ${LIQUIDDSPSRC}/src/random/src/randnakm.c
    ${LIQUIDDSPSRC}/src/random/src/randricek.c
    ${LIQUIDDSPSRC}/src/random/src/scramble.c
    )

#
# MODULE : sequence
#
set(sequence_SOURCES
    ${LIQUIDDSPSRC}/src/sequence/src/bsequence.c
    ${LIQUIDDSPSRC}/src/sequence/src/msequence.c
    )

#
# MODULE : utility
#
set(utility_SOURCES
    ${LIQUIDDSPSRC}/src/utility/src/bshift_array.c
    ${LIQUIDDSPSRC}/src/utility/src/byte_utilities.c
    ${LIQUIDDSPSRC}/src/utility/src/msb_index.c
    ${LIQUIDDSPSRC}/src/utility/src/pack_bytes.c
    ${LIQUIDDSPSRC}/src/utility/src/shift_array.c
    )

#
# MODULE : vector
#
set(vector_SOURCES
    ${LIQUIDDSPSRC}/src/vector/src/vectorf_add.port.c
    ${LIQUIDDSPSRC}/src/vector/src/vectorf_norm.port.c
    ${LIQUIDDSPSRC}/src/vector/src/vectorf_mul.port.c
    ${LIQUIDDSPSRC}/src/vector/src/vectorf_trig.port.c
    ${LIQUIDDSPSRC}/src/vector/src/vectorcf_add.port.c
    ${LIQUIDDSPSRC}/src/vector/src/vectorcf_norm.port.c
    ${LIQUIDDSPSRC}/src/vector/src/vectorcf_mul.port.c
    ${LIQUIDDSPSRC}/src/vector/src/vectorcf_trig.port.c
    )

#
# Library
#
set(liquiddsp_SOURCES
    ${LIQUIDDSPSRC}/src/libliquid.c
    ${agc_SOURCES}
    ${audio_SOURCES}
    ${buffer_SOURCES}
    ${channel_SOURCES}
    ${dotprod_SOURCES}
    ${equalization_SOURCES}
    ${fec_SOURCES}
    ${fft_SOURCES}
    ${filter_SOURCES}
    ${framing_SOURCES}
    ${math_SOURCES}
    ${matrix_SOURCES}
    ${modem_SOURCES}
    ${multichannel_SOURCES}
    ${nco_SOURCES}
    ${optim_SOURCES}
    ${quantization_SOURCES}
    ${random_SOURCES}
    ${sequence_SOURCES}
    ${utility_SOURCES}
    ${vector_SOURCES}
    )
    

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)
        
add_library(liquiddsp SHARED 
    ${liquiddsp_SOURCES}     
    ${liquiddsp_HEADERS_MOC}
)

if (HAVE_LIBM)
    target_link_libraries(liquiddsp m)
endif ()

if (HAVE_LIBFEC)
    target_link_libraries(liquiddsp fec)
endif ()

if (NOT LIQUID_FFTOVERRIDE AND HAVE_LIBFFTW3F)
    target_link_libraries(liquiddsp fftw3f)
endif ()

#set_property(TARGET liquid PROPERTY OUTPUT_NAME "liquid")
#set_property(TARGET liquid PROPERTY SOVERSION "${LIQUID_VERSION}")
set_property(TARGET liquiddsp PROPERTY COMPILE_FLAGS "${_EXTRA_C_FLAGS}")
