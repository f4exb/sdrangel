project(modemdvb)

set(modemdvb_SOURCES
    dvbt.cpp
    cpufeatures.cpp
    dvbt2.cpp
    dvbt2demap.cpp
    dvbt2fec.cpp
    dvbt2ldpc_generic.cpp
    dvbt2ldpctables.cpp
    dvbt2modulator.cpp
    dvbt2ofdm.cpp
    dvbt2receiver.cpp
    dvbt2tables.cpp
    dvbs2ldpctables.cpp
    dvbs2xldpctables.cpp
    dvbs.cpp
    dvbs2.cpp
    dvbs2modulator.cpp
    dvbs2pl.cpp
    dvbs2receiver.cpp
    ldpcpool.cpp
    dvbsfec.cpp
    dvbsfrontend.cpp
    dvbssharedfrontend.cpp
    dvbssymbolrate.cpp
    dvbsmodulator.cpp
    dvbsreceiver.cpp
    dvbtfec.cpp
    dvbtinner.cpp
    dvbtmodulator.cpp
    dvbtofdm.cpp
    cwcanceller.cpp
    dvbtouter.cpp
    dvbtreceiver.cpp
    viterbi27.cpp
    resampler.cpp
)

set(modemdvb_HEADERS
    dvbs.h
    dvbs2.h
    dvbs2modulator.h
    dvbs2pl.h
    dvbs2receiver.h
    ldpcpool.h
    dvbsfec.h
    dvbsfrontend.h
    dvbssharedfrontend.h
    dvbssymbolrate.h
    dvbsmodulator.h
    dvbsreceiver.h
    dvbt.h
    cpufeatures.h
    dvbt2.h
    dvbt2demap.h
    dvbt2fec.h
    dvbt2modulator.h
    dvbt2ofdm.h
    dvbt2receiver.h
    dvbt2tables.h
    dvbt2ldpc.h
    dvbt2ldpckernel.h
    dvbt2ldpctables.h
    dvbtfec.h
    dvbtinner.h
    dvbtmodulator.h
    dvbtofdm.h
    cwcanceller.h
    dvbtouter.h
    dvbtreceiver.h
    viterbi27.h
    viterbi27kernel.h
    resamplerkernel.h
    samplethread.h
    resampler.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrbase
)

# The DVB-T2 LDPC decoder, built for SSE4.1 and AVX2, chosen at run time for the CPU, or for NEON on ARM (the file
# is empty if the compiler isn't targeting NEON). dvbt2ldpc_generic.cpp is for other CPUs. Likewise the resampler
# and Viterbi kernels (NEON in resampler.cpp and viterbi27.cpp). The architecture is the target's, as the compiler
# says (DetectArchitecture.cmake), not the host's (CMAKE_SYSTEM_PROCESSOR), which differs when cross compiling, as
# with Visual Studio's -A ARM64 on x64
if(ARCHITECTURE_x86_64 OR ARCHITECTURE_x86)
    set(modemdvb_SSE41_SOURCES dvbt2ldpc_sse41.cpp viterbi27_sse41.cpp)
    set(modemdvb_AVX2_SOURCES dvbt2ldpc_avx2.cpp viterbi27_avx2.cpp)
    set(modemdvb_AVX2_FMA_SOURCES resampler_avx2.cpp dvbtofdm_avx2.cpp)
    list(APPEND modemdvb_SOURCES ${modemdvb_SSE41_SOURCES} ${modemdvb_AVX2_SOURCES} ${modemdvb_AVX2_FMA_SOURCES})
    # These files are compiled for instruction sets the CPU mightn't have, so only use raw buffers and their own
    # (internal) functions: any library code they instantiated (std::vector's...) would be compiled for them too, and
    # the linker keeps one copy of each for every caller, so code meant to run anywhere could crash
    if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # MSVC compiles the AVX2 and FMA intrinsics without /arch:AVX2. Without it, PSB1's decoding is bit-identical
        # and the kernels within a few percent
    else()
        if(MSVC)
            # clang-cl: its intrinsics need the instruction sets enabled, with the GCC style options
            set(modemdvb_SSE41_OPTIONS "/clang:-msse4.1")
            set(modemdvb_AVX2_OPTIONS "/clang:-mavx2")
            set(modemdvb_AVX2_FMA_OPTIONS "/clang:-mavx2;/clang:-mfma")
        else()
            set(modemdvb_SSE41_OPTIONS "-msse4.1")
            set(modemdvb_AVX2_OPTIONS "-mavx2")
            set(modemdvb_AVX2_FMA_OPTIONS "-mavx2;-mfma")
            if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
                # MinGW's GCC assumes the stack is 32-byte aligned for AVX spills, which Windows doesn't guarantee
                # (GCC bug 54412): the assembler can make its aligned moves unaligned ones (binutils 2.38)
                include(CheckCXXCompilerFlag)
                check_cxx_compiler_flag("-Wa,-muse-unaligned-vector-move" MODEMDVB_UNALIGNED_VECTOR_MOVE)
                if(MODEMDVB_UNALIGNED_VECTOR_MOVE)
                    list(APPEND modemdvb_AVX2_OPTIONS "-Wa,-muse-unaligned-vector-move")
                    list(APPEND modemdvb_AVX2_FMA_OPTIONS "-Wa,-muse-unaligned-vector-move")
                else()
                    message(WARNING "modemdvb: the assembler lacks -muse-unaligned-vector-move, so the AVX2 code may crash (GCC bug 54412)")
                endif()
            endif()
        endif()
        set_source_files_properties(${modemdvb_SSE41_SOURCES} PROPERTIES COMPILE_OPTIONS "${modemdvb_SSE41_OPTIONS}")
        set_source_files_properties(${modemdvb_AVX2_SOURCES} PROPERTIES COMPILE_OPTIONS "${modemdvb_AVX2_OPTIONS}")
        set_source_files_properties(${modemdvb_AVX2_FMA_SOURCES} PROPERTIES COMPILE_OPTIONS "${modemdvb_AVX2_FMA_OPTIONS}")
    endif()
    set(modemdvb_LDPC_X86 ON)
elseif(ARCHITECTURE_ARM64 OR ARCHITECTURE_ARM)
    list(APPEND modemdvb_SOURCES dvbt2ldpc_neon.cpp)
endif()

add_library(modemdvb
    ${modemdvb_SOURCES}
)

if(modemdvb_LDPC_X86)
    target_compile_definitions(modemdvb PRIVATE DVBT2_LDPC_X86 MODEMDVB_X86)
endif()

target_link_libraries(modemdvb
    Qt::Core
    sdrbase
)

install(TARGETS modemdvb DESTINATION ${INSTALL_LIB_DIR})
