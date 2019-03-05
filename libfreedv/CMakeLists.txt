project(freedv)

set(freedv_SOURCES
	codec2_fft.cpp
	cohpsk.cpp
	fdmdv.cpp
	freedv_api.cpp
	freedv_data_channel.cpp
	freedv_filter.cpp
	freedv_vhf_framing.cpp
	fsk.cpp
	kiss_fft.cpp
	linreg.cpp
)

set(freedv_HEADERS
	codec2_cohpsk.h
	codec2_fdmdv.h
	codec2_fft.h
	codec2_ofdm.h
	cohpsk_defs.h
	cohpsk_internal.h
	defines.h
	fdmdv_internal.h
	fdv_arm_math.h
	fmfsk.h
	freedv_api_internal.h
	freedv_data_channel.h
	freedv_filter_coef.h
	freedv_filter.h
	freedv_vhf_framing.h
	fsk.h
	gp_interleaver.h
	hanning.h
	interldpc.h
	_kiss_fft_guts.h
	kiss_fft.h
	kiss_fftr.h
	libfreedv.h
	linreg.h
	machdep.h
	modem_probe.h
	modem_stats.h
	mpdecode_core.h
	ofdm_internal.h
	os.h
	pilot_coeff.h
	pilots_coh.h
	rn_coh.h
	rn.h
	rxdec_coeff.h
	test_bits_coh.h
	test_bits.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CODEC2_INCLUDE_DIR}
)

add_definitions(-DQT_SHARED)

add_library(freedv SHARED
	${freedv_SOURCES}
)

target_link_libraries(freedv
	${CODEC2_LIBRARIES}
)

install(TARGETS freedv DESTINATION lib)