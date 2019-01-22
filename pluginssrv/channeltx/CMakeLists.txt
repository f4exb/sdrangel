project(mod)

add_subdirectory(modam)
add_subdirectory(modnfm)
add_subdirectory(modssb)
add_subdirectory(modwfm)
add_subdirectory(udpsource)

find_package(CM256cc)
if(CM256CC_FOUND)
    add_subdirectory(remotesource)
endif()

find_package(OpenCV)
if (OpenCV_FOUND)
	add_subdirectory(modatv)
endif()
