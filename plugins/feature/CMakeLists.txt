project(feature)

if (ENABLE_FEATURE_GS232CONTROLLER AND Qt5SerialPort_FOUND)
    add_subdirectory(gs232controller)
endif()

if (ENABLE_FEATURE_MAP AND Qt5Quick_FOUND AND Qt5QuickWidgets_FOUND AND Qt5Positioning_FOUND AND Qt5Location_FOUND AND Qt5WebEngine_FOUND)
    add_subdirectory(map)
endif()

if (ENABLE_FEATURE_VORLOCALIZER AND Qt5Quick_FOUND AND Qt5QuickWidgets_FOUND AND Qt5Positioning_FOUND)
    add_subdirectory(vorlocalizer)
endif()

if (ENABLE_FEATURE_AFC)
    add_subdirectory(afc)
endif()

if (ENABLE_FEATURE_AIS)
    add_subdirectory(ais)
endif()

if (ENABLE_FEATURE_ANTENNATOOLS)
    add_subdirectory(antennatools)
endif()

if (ENABLE_FEATURE_APRS)
    add_subdirectory(aprs)
endif()

if (ENABLE_FEATURE_DEMODANALYZER)
    add_subdirectory(demodanalyzer)
endif()

if (ENABLE_FEATURE_PERTESTER)
    add_subdirectory(pertester)
endif()

if (ENABLE_FEATURE_RIGCTLSERVER)
    add_subdirectory(rigctlserver)
endif()

if (ENABLE_FEATURE_SIMPLEPTT)
    add_subdirectory(simpleptt)
endif()

if (NOT SERVER_MODE)
    if (ENABLE_FEATURE_SATELLITETRACKER AND SGP4_FOUND AND Qt5Positioning_FOUND AND Qt5Charts_FOUND AND Qt5TextToSpeech_FOUND)
        add_subdirectory(satellitetracker)
    endif()
    if (ENABLE_FEATURE_JOGDIALCONTROLLER)
        add_subdirectory(jogdialcontroller)
    endif()
else()
    if (ENABLE_FEATURE_SATELLITETRACKER AND SGP4_FOUND AND Qt5Positioning_FOUND AND Qt5Charts_FOUND)
        add_subdirectory(satellitetracker)
    endif()
endif()

if (Qt5Charts_FOUND)
    if (ENABLE_FEATURE_RADIOSONDE)
        add_subdirectory(radiosonde)
    endif()
    if (ENABLE_FEATURE_STARTRACKER)
        add_subdirectory(startracker)
    endif()
endif()

if (ENABLE_FEATURE_LIMERFE AND ENABLE_LIMESUITE AND LIMESUITE_FOUND)
    add_subdirectory(limerfe)
endif()

if (ENABLE_FEATURE_AMBE AND LIBSERIALDV_FOUND)
    add_subdirectory(ambe)
endif()
