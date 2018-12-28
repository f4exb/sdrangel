project (swagger)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

file(GLOB swagger_SOURCES
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client/*.cpp
)

file(GLOB swagger_HEADERS
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client/*.h
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)

add_library(swagger SHARED
    ${swagger_SOURCES}
    ${swagger_HEADERS_MOC}
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    .
    ${CMAKE_SOURCE_DIR}/exports    
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set_target_properties(swagger PROPERTIES COMPILE_FLAGS "-Wno-conversion-null -Wno-unused-variable -Wno-unused-parameter")
endif()

target_link_libraries(swagger
    ${QT_LIBRARIES}
)

set_target_properties(swagger PROPERTIES DEFINE_SYMBOL "swagger_EXPORTS")

target_link_libraries(swagger Qt5::Core Qt5::Network)

install(TARGETS swagger DESTINATION lib)
