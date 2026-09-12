project(sdrangelmcpbridge)

# A standalone executable, deliberately free of Qt and of the rest of SDRangel: it is
# shipped inside a Claude Desktop extension bundle, where nothing else is on the library
# path, so it must run with only the C++ runtime.

set(mcpbridge_SOURCES
    main.cpp
    httpclient.cpp
    jsonscan.cpp
    ssereader.cpp
)

set(mcpbridge_HEADERS
    httpclient.h
    jsonscan.h
    ssereader.h
)

add_executable(sdrangel-mcp-bridge ${mcpbridge_SOURCES} ${mcpbridge_HEADERS})

# There is no Qt here, so the project wide automatic moc, uic and rcc have nothing to do
set_target_properties(sdrangel-mcp-bridge PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

target_compile_definitions(sdrangel-mcp-bridge PRIVATE
    SDRANGEL_MCP_BRIDGE_VERSION="${CPACK_PACKAGE_VERSION}"
)

find_package(Threads REQUIRED)
target_link_libraries(sdrangel-mcp-bridge Threads::Threads)

if(WIN32)
    target_link_libraries(sdrangel-mcp-bridge ws2_32)
    # Static runtime, so the bundle does not depend on the redistributable being present
    set_property(TARGET sdrangel-mcp-bridge PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

install(TARGETS sdrangel-mcp-bridge DESTINATION ${INSTALL_BIN_DIR})

# A .mcpb is a zip with manifest.json at its root, so CMake can build one on its own. The
# mcpb CLI only adds validation and signing, and requiring npm to build SDRangel would be a
# poor trade for that.
if(BUILD_MCP_BUNDLE)
    if(WIN32)
        set(MCPBRIDGE_BUNDLE_PLATFORM "win32")
    elseif(APPLE)
        set(MCPBRIDGE_BUNDLE_PLATFORM "darwin")
    else()
        set(MCPBRIDGE_BUNDLE_PLATFORM "linux")
    endif()

    # A bundle claiming a version that is already installed is not unpacked again, which
    # silently leaves the previous one in place, so every build needs a version of its own.
    # A release is exactly its tag; anything else carries the git describe suffix and the time
    # it was configured, since rebuilds between commits would otherwise all claim the same
    # version. Both are valid semantic version prereleases, and the added identifier makes a
    # rebuild sort above what it replaces rather than below it
    if(sdrangel_VERSION_SUFFIX STREQUAL "")
        set(MCPBRIDGE_BUNDLE_VERSION "${CPACK_PACKAGE_VERSION}")
    else()
        string(TIMESTAMP MCPBRIDGE_BUILD_TIME "%Y%m%dT%H%M%S" UTC)
        set(MCPBRIDGE_BUNDLE_VERSION "${CPACK_PACKAGE_VERSION}.${MCPBRIDGE_BUILD_TIME}")
    endif()

    # The tool list the bundle carries comes from the server itself, built here by a host tool
    # that constructs the real registry. Sources are named by path rather than reusing the
    # plugin target, which is a Qt plugin module and cannot be linked into an executable
    set(MCPTOOLSDUMP_DIR "${CMAKE_SOURCE_DIR}/plugins/feature/mcpserver")

    add_executable(sdrangel-mcp-tools-dump
        "${MCPTOOLSDUMP_DIR}/toolsdump.cpp"
        "${MCPTOOLSDUMP_DIR}/mcptools.cpp"
        "${MCPTOOLSDUMP_DIR}/mcpdocs.cpp"
        "${MCPTOOLSDUMP_DIR}/mcpdatafeed.cpp"
        "${MCPTOOLSDUMP_DIR}/mcpcapture.cpp"
        "${MCPTOOLSDUMP_DIR}/mcpaudiocapture.cpp"
        "${MCPTOOLSDUMP_DIR}/mcpstreams.cpp"
    )

    target_include_directories(sdrangel-mcp-tools-dump PRIVATE
        "${MCPTOOLSDUMP_DIR}"
        "${CMAKE_SOURCE_DIR}/sdrbase"
        "${CMAKE_SOURCE_DIR}/exports"
        "${CMAKE_SOURCE_DIR}/httpserver"
        "${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client"
    )

    target_link_libraries(sdrangel-mcp-tools-dump PRIVATE Qt::Core sdrbase httpserver)

    set(MCPBRIDGE_BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/bundle")
    # Alongside the built binaries, which is bin/ on Windows and the build directory itself
    # elsewhere. That is also where the running application looks for it in a build tree
    set(MCPBRIDGE_BUNDLE_FILE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sdrangel-${MCPBRIDGE_BUNDLE_VERSION}-${MCPBRIDGE_BUNDLE_PLATFORM}.mcpb")

    configure_file(manifest.json.in "${MCPBRIDGE_BUNDLE_DIR}/manifest.json" @ONLY)

    # Part of ALL, so that turning the option on is enough: the packaging step installs the
    # bundle if it is there, and silently leaves it out if it is not, which would be a hard
    # thing to notice in a finished installer
    add_custom_target(mcpbundle ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MCPBRIDGE_BUNDLE_DIR}/server"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sdrangel-mcp-bridge> "${MCPBRIDGE_BUNDLE_DIR}/server/"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/cmake/cpack/sdrangel_icon.png" "${MCPBRIDGE_BUNDLE_DIR}/icon.png"
        # The tool list, straight from the server's own registry, beside the bridge that serves
        # it when SDRangel is not up
        COMMAND $<TARGET_FILE:sdrangel-mcp-tools-dump> "${MCPBRIDGE_BUNDLE_DIR}/server/tools.json"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${MCPBRIDGE_BUNDLE_FILE}"
        # Relative paths and a working directory, so that manifest.json lands at the root of
        # the archive, which is where the installer looks for it
        COMMAND ${CMAKE_COMMAND} -E tar cf "${MCPBRIDGE_BUNDLE_FILE}" --format=zip -- manifest.json icon.png server
        WORKING_DIRECTORY "${MCPBRIDGE_BUNDLE_DIR}"
        DEPENDS sdrangel-mcp-bridge sdrangel-mcp-tools-dump "${MCPBRIDGE_BUNDLE_DIR}/manifest.json"
        COMMENT "Packaging ${MCPBRIDGE_BUNDLE_FILE}"
        VERBATIM
    )

    # The installer needs the name, and cmake/cpack is configured after this directory
    set(MCPBRIDGE_BUNDLE_NAME "sdrangel-${MCPBRIDGE_BUNDLE_VERSION}-${MCPBRIDGE_BUNDLE_PLATFORM}.mcpb"
        CACHE INTERNAL "File name of the Claude Desktop extension bundle")

    # Not OPTIONAL: mcpbundle is part of ALL, so the bundle is always there by the time the
    # install rules run. Should packaging ever break, failing here is far better than an
    # installer that quietly ships without the extension
    install(FILES "${MCPBRIDGE_BUNDLE_FILE}" DESTINATION ${INSTALL_BIN_DIR})

    # Beside the installed bridge as well, for anyone pointing a client straight at it rather
    # than installing the extension
    install(FILES "${MCPBRIDGE_BUNDLE_DIR}/server/tools.json" DESTINATION ${INSTALL_BIN_DIR})
endif()
