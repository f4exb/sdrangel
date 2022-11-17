project (swagger)

file(GLOB swagger_SOURCES
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client/*.cpp
)

file(GLOB swagger_HEADERS
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client/*.h
)

add_library(swagger SHARED
    ${swagger_SOURCES}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if (NOT MSVC)
    set_target_properties(swagger PROPERTIES COMPILE_FLAGS "-Wno-conversion-null -Wno-unused-variable -Wno-unused-parameter")
endif()

target_link_libraries(swagger
    Qt::Core
    Qt::Network
)

install(TARGETS swagger DESTINATION ${INSTALL_LIB_DIR})
