project(qrtplib)

set (qrtplib_HEADERS
	rtcpapppacket.h
	rtcpbyepacket.h
	rtcpcompoundpacket.h
	rtcpcompoundpacketbuilder.h
	rtcppacket.h
	rtcppacketbuilder.h
	rtcprrpacket.h
	rtcpscheduler.h
	rtcpsdesinfo.h
	rtcpsdespacket.h
	rtcpsrpacket.h
	rtcpunknownpacket.h
	rtpaddress.h
	rtpcollisionlist.h
	rtpconfig.h
	rtpdefines.h
	rtpendian.h
	rtperrors.h
	rtphashtable.h
	rtpinternalsourcedata.h
#	rtpipv4address.h
#	rtpipv4destination.h
	rtpkeyhashtable.h
	rtplibraryversion.h
	rtppacket.h
	rtppacketbuilder.h
	rtprandom.h
	rtprandomrand48.h
	rtprandomrands.h
	rtprandomurandom.h
	rtprawpacket.h
	rtpsession.h
	rtpsessionparams.h
	rtpsessionsources.h
	rtpsourcedata.h
	rtpsources.h
	rtpstructs.h
	rtptimeutilities.h
	rtptransmitter.h
	rtptypes_win.h
	rtptypes.h
#	rtpudpv4transmitter.h
#   rtpudpv4transmitternobind.h
#	rtpexternaltransmitter.h
	rtpsocketutil.h
	rtpabortdescriptors.h
	rtpselect.h
#	rtptcpaddress.h
#	rtptcptransmitter.h
	)

set(qrtplib_SOURCES
	rtcpapppacket.cpp
	rtcpbyepacket.cpp
	rtcpcompoundpacket.cpp
	rtcpcompoundpacketbuilder.cpp
	rtcppacketbuilder.cpp
	rtcprrpacket.cpp
	rtcpscheduler.cpp
	rtcpsdesinfo.cpp
	rtcpsdespacket.cpp
	rtcpsrpacket.cpp
    rtpaddress.cpp	
	rtpcollisionlist.cpp
	rtperrors.cpp
	rtpinternalsourcedata.cpp
#	rtpipv4address.cpp
#	rtpipv4destination.cpp
	rtplibraryversion.cpp
	rtppacket.cpp
	rtppacketbuilder.cpp
	rtprandom.cpp
	rtprandomrand48.cpp
	rtprandomrands.cpp
	rtprandomurandom.cpp
	rtpsession.cpp
	rtpsessionparams.cpp
	rtpsessionsources.cpp
	rtpsourcedata.cpp
	rtpsources.cpp
	rtptimeutilities.cpp
#	rtpudpv4transmitter.cpp
#   rtpudpv4transmitternobind.cpp
#	rtpexternaltransmitter.cpp
	rtpabortdescriptors.cpp
#	rtptcpaddress.cpp
#	rtptcptransmitter.cpp
	)

include_directories(
    .
    ${CMAKE_SOURCE_DIR}/sdrbase 
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)

add_library(qrtplib SHARED
    ${qrtplib_SOURCES}
    ${qrtplib_HEADERS_MOC}
)

target_link_libraries(qrtplib
    ${QT_LIBRARIES}
)

qt5_use_modules(qrtplib Core Network)

install(TARGETS qrtplib DESTINATION lib)
