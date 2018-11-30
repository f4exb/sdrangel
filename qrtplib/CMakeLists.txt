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
    rtpinternalsourcedata.h
    rtpkeyhashtable.h
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
    rtpudptransmitter.h
    rtpsocketutil.h
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
    rtpudptransmitter.cpp
    )

include_directories(
    .
    ${CMAKE_SOURCE_DIR}/exports
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

target_link_libraries(qrtplib Qt5::Core Qt5::Network)

install(TARGETS qrtplib DESTINATION lib)
