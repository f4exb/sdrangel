SET(QT_MISSING True)
# msvc only; mingw will need different logic
IF(MSVC)
    # look for user-registry pointing to qtcreator
    GET_FILENAME_COMPONENT(QT_BIN [HKEY_CURRENT_USER\\Software\\Classes\\Applications\\QtProject.QtCreator.pro\\shell\\Open\\Command] PATH)

    # get root path so we can search for 5.3, 5.4, 5.5, etc
    STRING(REPLACE "/Tools" ";" QT_BIN "${QT_BIN}")
    LIST(GET QT_BIN 0 QT_BIN)
    FILE(GLOB QT_VERSIONS "${QT_BIN}/5.*")
    LIST(SORT QT_VERSIONS)

    # assume the latest version will be last alphabetically
    LIST(REVERSE QT_VERSIONS)

    LIST(GET QT_VERSIONS 0 QT_VERSION)

    # fix any double slashes which seem to be common
    STRING(REPLACE "//" "/"  QT_VERSION "${QT_VERSION}")

    # do some math trickery to guess folder
    # - qt uses (e.g.) "msvc2012"
    # - cmake uses (e.g.) "1800"
    # - see also https://cmake.org/cmake/help/v3.0/variable/MSVC_VERSION.html
    # checkcompiler version
    if(MSVC_VERSION GREATER 1910 AND MSVC_VERSION LESS 1919)
        set(QT_MSVC 2017)
    elseif(MSVC_VERSION GREATER 1899 AND MSVC_VERSION LESS 1910)
        set(QT_MSVC 2015)
    else()
        MATH(EXPR QT_MSVC "2000 + (${MSVC_VERSION} - 600) / 100")
    endif()    

    # check for 64-bit os
    # may need to be removed for older compilers as it wasn't always offered
    IF(CMAKE_SYSTEM_PROCESSOR MATCHES 64)
        SET(QT_MSVC "${QT_MSVC}_64")
    ENDIF()
    SET(QT_PATH "${QT_VERSION}/msvc${QT_MSVC}")
    SET(QT_MISSING False)
ENDIF()

# use Qt_DIR approach so you can find Qt after cmake has been invoked
IF(NOT QT_MISSING)
    MESSAGE("-- Qt found: ${QT_PATH}")
    SET(Qt5_DIR "${QT_PATH}/lib/cmake/Qt5/")
    SET(Qt5Test_DIR "${QT_PATH}/lib/cmake/Qt5Test")
ENDIF()
