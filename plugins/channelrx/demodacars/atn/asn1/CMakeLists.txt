include (CMakeLists.inc)
if (EMIT_ASN_DEBUG)
	set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DEMIT_ASN_DEBUG=1")
endif()

# force inclusion of alloca.h from stdlib.h on macos
remove_definitions(-D_XOPEN_SOURCE=600)

add_library (asn1 OBJECT ${ASN_MODULE_SOURCES})

target_include_directories (asn1 PUBLIC
	${asn1_include_dirs}
)
