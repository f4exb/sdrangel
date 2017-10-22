project(chanalyzerng)

set(chanalyzerng_SOURCES
	chanalyzerng.cpp
	chanalyzernggui.cpp
	chanalyzerngplugin.cpp
)

set(chanalyzerng_HEADERS
	chanalyzerng.h
	chanalyzernggui.h
	chanalyzerngplugin.h
)

set(chanalyzerng_FORMS
	chanalyzernggui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt5_wrap_cpp(chanalyzer_HEADERS_MOC ${chanalyzer_HEADERS})
qt5_wrap_ui(chanalyzerng_FORMS_HEADERS ${chanalyzerng_FORMS})

add_library(chanalyzerng SHARED
	${chanalyzerng_SOURCES}
	${chanalyzerng_HEADERS_MOC}
	${chanalyzerng_FORMS_HEADERS}
)

target_link_libraries(chanalyzerng
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

qt5_use_modules(chanalyzerng Core Widgets )

install(TARGETS chanalyzerng DESTINATION lib/plugins/channelrx)
