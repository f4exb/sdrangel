project(kitiirfir)

set(kitiirfir_SOURCES
    FFTCode.cpp
	FIRFilterCode.cpp
	FreqSamplingCode.cpp
	IIRFilterCode.cpp
	LowPassPrototypes.cpp
	LowPassRoots.cpp
	NewParksMcClellan.cpp
	PFiftyOneRevE.cpp
	QuadRootsRevH.cpp
)

set(kitiirfir_HEADERS
    FFTCode.h
	FIRFilterCode.h
	FreqSamplingCode.h
	IIRFilterCode.h
	LowPassPrototypes.h
	LowPassRoots.h
	NewParksMcClellan.h
	PFiftyOneRevE.h
	QuadRootsRevH.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(-DQT_SHARED)

add_library(kitiirfir SHARED
    ${kitiirfir_SOURCES}
)

install(TARGETS kitiirfir DESTINATION lib)
