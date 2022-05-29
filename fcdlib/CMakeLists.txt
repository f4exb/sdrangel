project(fcdlib)

set(fcdlib_SOURCES
	fcdtraits.cpp
	fcdproplusconst.cpp
	fcdproconst.cpp
)

set(fcdlib_HEADERS
	fcdtraits.h
	fcdproplusconst.h
	fcdproconst.h
    export.h
)

include_directories(
)

add_library(fcdlib SHARED
	${fcdlib_SOURCES}
)

target_link_libraries(fcdlib)

set_target_properties(fcdlib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS true)
if (MSVC)
    set_target_properties(fcdlib PROPERTIES INTERPROCEDURAL_OPTIMIZATION false)
    set_target_properties(fcdlib PROPERTIES DEFINE_SYMBOL "FCDLIB_EXPORTS")
endif()

install(TARGETS fcdlib DESTINATION ${INSTALL_LIB_DIR})
