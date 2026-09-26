# Mirrors the headers of one directory into another, so that sources including a library by a
# subdirectory name it does not have in its own tree - <libairspy/airspy.h>, <dsp/dvcontroller.h> -
# resolve. Only the files directly in SRC are copied and no subdirectory is descended into, so a
# destination inside SRC does not end up copying itself.
#
# Run as: cmake -DSRC=<dir> -DDEST=<dir> -P copy_headers.cmake

file(GLOB headers "${SRC}/*.h" "${SRC}/*.hpp")

file(MAKE_DIRECTORY "${DEST}")

if (headers)
    # file(COPY) skips a file that is already there with the same timestamp, so repeating this on
    # every build costs no rebuilds
    file(COPY ${headers} DESTINATION "${DEST}")
endif ()
