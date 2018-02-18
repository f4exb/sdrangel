#
# External resource support for CMake's Qt5 support, version 1.0
# written by Grzegorz Antoniak <ga -at- anadoxin -dot- org>
# http://github.com/antekone/qt5-external-resources-cmake
#
# Tested on CMake 3.2.2

# qt5_parse_rcc_depends function
#
# Based on original Qt5CoreMacros.cmake/qt5_add_resources function.

function(QT5_PARSE_RCC_DEPENDS infile)
    set(_RC_DEPENDS)
    if(EXISTS "${infile}")
        #  parse file for dependencies
        #  all files are absolute paths or relative to the location of the qrc file
        file(READ "${infile}" _RC_FILE_CONTENTS)
        string(REGEX MATCHALL "<file[^<]+" _RC_FILES "${_RC_FILE_CONTENTS}")
        foreach(_RC_FILE ${_RC_FILES})
            string(REGEX REPLACE "^<file[^>]*>" "" _RC_FILE "${_RC_FILE}")
            if(NOT IS_ABSOLUTE "${_RC_FILE}")
                set(_RC_FILE "${rc_path}/${_RC_FILE}")
            endif()
            set(_RC_DEPENDS ${_RC_DEPENDS} "${_RC_FILE}")
        endforeach()
        # Since this cmake macro is doing the dependency scanning for these files,
        # let's make a configured file and add it as a dependency so cmake is run
        # again when dependencies need to be recomputed.
        qt5_make_output_file("${infile}" "" "qrc.depends" out_depends)
        configure_file("${infile}" "${out_depends}" COPYONLY)

        set(out_depends_return ${out_depends} PARENT_SCOPE)
        set(_RC_DEPENDS_RETURN ${_RC_DEPENDS} PARENT_SCOPE)
    else()
        # The .qrc file does not exist (yet). Let's add a dependency and hope
        # that it will be generated later
        set(out_depends)
    endif()
endfunction()

# qt5_add_external_resources function
#
# Usage:
#
#    qt5_add_external_resources(outfile res/infile.qrc)
#
# This should generate ${CMAKE_BINARY_DIR}/outfile.rcc, ready to be used inside
# the application. You can also use it like this:
#
#    qt5_add_external_resources(outfile res/infile.qrc OPTIONS -someoption)
#
# if you would like to add some option to the RCC's command line.

function(QT5_ADD_EXTERNAL_RESOURCES rccfilename qrcfilename)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs OPTIONS)

    cmake_parse_arguments(_RCC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(rcc_files ${_RCC_UNPARSED_ARGUMENTS})
    set(rcc_options ${_RCC_OPTIONS})

    get_filename_component(outfilename ${rccfilename} NAME_WE)
    get_filename_component(infile ${qrcfilename} ABSOLUTE)
    get_filename_component(rc_path ${infile} PATH)
    set(outfile ${CMAKE_CURRENT_BINARY_DIR}/${outfilename}.rcc)

    qt5_parse_rcc_depends(${infile})

    add_custom_command(OUTPUT ${outfile}
                       COMMAND ${Qt5Core_RCC_EXECUTABLE}
                       ARGS ${rcc_options} -name ${outfilename} -o ${outfile} ${infile} -binary
                       MAIN_DEPENDENCY ${infile}
                       DEPENDS ${_RC_DEPENDS_RETURN} "${out_depends_return}" VERBATIM)
    set(_RC_TARGETNAME "qrc_external_${outfilename}")
    add_custom_target(${_RC_TARGETNAME} ALL DEPENDS ${outfile})
endfunction()

# vim:set et:
