project(feature)

if (Qt5SerialPort_FOUND)
    add_subdirectory(gs232controller)
endif()
add_subdirectory(afc)
add_subdirectory(rigctlserver)
add_subdirectory(simpleptt)
