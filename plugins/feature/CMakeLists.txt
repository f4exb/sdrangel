project(feature)

if (Qt5SerialPort_FOUND)
    add_subdirectory(gs232controller)
endif()

if (Qt5Quick_FOUND AND Qt5QuickWidgets_FOUND AND Qt5Positioning_FOUND)
    add_subdirectory(map)
    add_subdirectory(vorlocalizer)
endif()

add_subdirectory(afc)
add_subdirectory(aprs)
add_subdirectory(demodanalyzer)
add_subdirectory(rigctlserver)
add_subdirectory(simpleptt)
add_subdirectory(startracker)
