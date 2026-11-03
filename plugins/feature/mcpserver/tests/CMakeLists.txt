cmake_minimum_required(VERSION 3.17)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(mcpdiscoverytests LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_AUTOMOC ON)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Test)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Test)
    set(MCP_TEST_QT_MAJOR ${QT_VERSION_MAJOR})
    enable_testing()
endif()

add_executable(mcp-catalog-test catalogtest.cpp ../mcptoolcatalog.cpp)
target_include_directories(mcp-catalog-test PRIVATE ..)
target_link_libraries(mcp-catalog-test PRIVATE Qt${MCP_TEST_QT_MAJOR}::Core Qt${MCP_TEST_QT_MAJOR}::Test)
add_test(NAME mcp-catalog COMMAND mcp-catalog-test)

if(TARGET sdrbase)
    add_executable(mcp-discovery-test
        discoverytest.cpp
        ../mcprequesthandler.cpp ../mcpprotocol.cpp ../mcptools.cpp ../mcptoolcatalog.cpp ../mcpdocs.cpp
        ../mcpdatafeed.cpp ../mcpcapture.cpp ../mcpaudiocapture.cpp ../mcpstreams.cpp
    )
    target_include_directories(mcp-discovery-test PRIVATE ..
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
        ${CMAKE_SOURCE_DIR}/httpserver)
    target_link_libraries(mcp-discovery-test PRIVATE Qt::Core Qt${MCP_TEST_QT_MAJOR}::Test sdrbase httpserver)
    add_test(NAME mcp-discovery COMMAND mcp-discovery-test)
    set_tests_properties(mcp-discovery PROPERTIES TIMEOUT 30)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME mcp-discovery-http COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/httptest.py $<TARGET_FILE:mcp-discovery-test>)
        set_tests_properties(mcp-discovery-http PROPERTIES TIMEOUT 60)
    endif()
endif()
