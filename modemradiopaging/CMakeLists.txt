project(modemradiopaging)

set(modemradiopaging_SOURCES
    radiopagingbch.cpp
    radiopagingfskdemod.cpp
    radiopagingfskmod.cpp
    radiopagingpocsag.cpp
    radiopagingpocsagdecoder.cpp
    radiopagingpocsagreceiver.cpp
    radiopaginggsc.cpp
    radiopaginggscdecoder.cpp
    radiopaginggscreceiver.cpp
    radiopagingflex.cpp
    radiopagingflexscheduler.cpp
    radiopagingflexdecoder.cpp
    radiopagingflexdecodermsg.cpp
    radiopagingflexreceiver.cpp
    radiopagingtxstream.cpp
    radiopagingwav.cpp
    radiopagingvoiceprocessor.cpp
    radiopagingcharset.cpp
    radiopagingrecode.cpp
    charsets/pocsagascii.cpp
    charsets/pocsagasciiuppercase.cpp
    charsets/pocsaglatin.cpp
    charsets/pocsagmotorolaadvisorcyrillic.cpp
    charsets/pocsaglinguistlatincyrillic.cpp
    charsets/pocsagnec21alatincyrillic.cpp
    charsets/pocsagcyrillic.cpp
    charsets/flexlinguistlatin.cpp
    charsets/flexlinguistscripts.cpp
    charsets/gscascii.cpp
    charsets/flexbeepwearalarm.cpp
    charsets/rot13.cpp
    charsets/kanji.cpp
    radiopagingvoicestore.cpp
    radiopagingpage.cpp
    radiopagingpagerformat.cpp
)

set(modemradiopaging_HEADERS
    radiopagingbch.h
    radiopagingberwindow.h
    radiopagingdedup.h
    radiopagingfskdemod.h
    radiopagingfskmod.h
    radiopagingpocsag.h
    radiopagingpocsagdecoder.h
    radiopagingpocsagreceiver.h
    radiopaginggsc.h
    radiopaginggscdecoder.h
    radiopaginggscreceiver.h
    radiopagingflex.h
    radiopagingflexscheduler.h
    radiopagingflexdecoder.h
    radiopagingflexreceiver.h
    radiopagingtxstream.h
    radiopagingwav.h
    radiopagingvoiceprocessor.h
    radiopagingcharset.h
    radiopagingkanji.h
    radiopagingrecode.h
    charsets/radiopagingcharsettables.h
    charsets/radiopagingcharsetbuild.h
    charsets/radiopagingcyrillic.h
    radiopagingvoicestore.h
    radiopagingpage.h
    radiopagingpagerformat.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

add_library(modemradiopaging
    ${modemradiopaging_SOURCES}
)

target_link_libraries(modemradiopaging
    Qt::Core
    sdrbase
    swagger
)

install(TARGETS modemradiopaging DESTINATION ${INSTALL_LIB_DIR})

# Self tests of the codecs and the demodulator, not built by default:
#   cmake --build <build dir> --target radiopagingtest radiopaginggsctest radiopagingflextest radiopagingflexdecodertest radiopagingflexreceivertest radiopagingfskmodtest radiopagingtxstreamtest radiopagingpagetest radiopagingvoiceprocessortest radiopagingcharsettest
foreach(radiopaging_test radiopagingtest radiopaginggsctest radiopagingflextest radiopagingflexdecodertest radiopagingflexreceivertest radiopagingfskmodtest radiopagingtxstreamtest radiopagingpagetest radiopagingvoiceprocessortest radiopagingcharsettest)
    add_executable(${radiopaging_test} EXCLUDE_FROM_ALL test/${radiopaging_test}.cpp)
    target_link_libraries(${radiopaging_test} modemradiopaging)
    target_include_directories(${radiopaging_test} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test)
endforeach()
