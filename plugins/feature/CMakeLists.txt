project(feature)

add_subdirectory(afc)
if (Qt5SerialPort_FOUND)
    add_subdirectory(gs232controller)
endif()
add_subdirectory(rigctlserver)
add_subdirectory(simpleptt)
