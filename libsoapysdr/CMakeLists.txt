project(soapysdr)

file(GLOB soapysdr_SOURCES
	${LIBSOAPYSDRSRC}/lib/*.cpp
)

file(GLOB soapysdr_HEADERS
	${LIBSOAPYSDRSRC}/include/SoapySDR/*.hpp
	${LIBSOAPYSDRSRC}/include/SoapySDR/*.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBSOAPYSDRSRC}/include
    ${LIBSOAPYSDRSRC}/lib
)

add_definitions(-DQT_SHARED)

message( STATUS "soapysdr_SOURCES: ${soapysdr_SOURCES}" )

add_library(soapysdr SHARED
    ${soapysdr_SOURCES}
)

install(TARGETS soapysdr DESTINATION lib)
