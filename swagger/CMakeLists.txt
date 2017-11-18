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
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

set_target_properties(swagger PROPERTIES COMPILE_FLAGS "-Wno-conversion-null -Wno-unused-variable -Wno-unused-parameter")

target_link_libraries(swagger
    ${QT_LIBRARIES}
)

set_target_properties(swagger PROPERTIES DEFINE_SYMBOL "sdrangel_EXPORTS")
target_compile_features(sdrbase PRIVATE cxx_generalized_initializers) # cmake >= 3.1.0

qt5_use_modules(swagger Core Network)

install(TARGETS swagger DESTINATION lib)
