project(demod)

add_subdirectory(nfm)
add_subdirectory(tcpsrc)
add_subdirectory(ssb)

if (IS96KHZ)
  add_subdirectory(usb)
endif()
#add_subdirectory(tetra)
