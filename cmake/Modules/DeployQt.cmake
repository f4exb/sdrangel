if (Qt6_FOUND)
    find_package(Qt6Core REQUIRED)
else()
    find_package(Qt5Core REQUIRED)
endif()

get_target_property(_qmake_executable Qt::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)

find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
if(WIN32 AND NOT WINDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "windeployqt not found")
endif()

# Add commands that copy the required Qt files to ${bindir} as well as including
# them in final installation (by first copying them to a winqt subdir)
# We need to specify ${bindir} as we run this on plugins as well as the main .exe
# Preferably, it would be nicer to skip the extra copy to winqt subdir, but how?
# Also, we should possibly only call install once, after all deployments are made
function(windeployqt target bindir qmldir)

    # Run windeployqt after build
    # First deploy in to bin directory, so we can run from the build bin directory
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E
            env PATH="${_qt_bin_dir}" "${WINDEPLOYQT_EXECUTABLE}"
                --verbose 2
                --no-compiler-runtime
                --dir "${bindir}"
                --qmldir "${qmldir}"
                --multimedia
                --websockets
                --opengl
                \"$<TARGET_FILE:${target}>\"
        COMMENT "Deploying Qt..."
    )

    # Then, deploy again in to separate directory for install to pick up
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E
            env PATH="${_qt_bin_dir}" "${WINDEPLOYQT_EXECUTABLE}"
                --verbose 1
                --no-compiler-runtime
                --dir "${bindir}/winqt"
                --qmldir "${qmldir}"
                --multimedia
                --websockets
                --opengl
                \"$<TARGET_FILE:${target}>\"
        COMMENT "Deploying Qt..."
    )

    install(DIRECTORY "${bindir}/winqt/" DESTINATION .)

endfunction()

mark_as_advanced(WINDEPLOYQT_EXECUTABLE)
