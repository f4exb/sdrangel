project(freedv)

set(freedv_SOURCES
	freedv_api.cpp
)

set(freedv_HEADERS
	codec2_cohpsk.h
	codec2_fdmdv.h
	codec2_fft.h
	codec2_ofdm.h
	defines.h
	fdmdv_internal.h
	fdv_arm_math.h
	fmfsk.h
	freedv_api_internal.h
	freedv_data_channel.h
	freedv_filter.h
	freedv_vhf_framing.h
	fsk.h
	gp_interleaver.h
	interldpc.h
	kiss_fft.h
	kiss_fftr.h
	libfreedv.h
	modem_probe.h
	modem_stats.h
	mpdecode_core.h
	ofdm_internal.h
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