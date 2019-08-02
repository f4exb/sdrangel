#
# Check if the file system is case sensitive or not
# Inspired by Andreas Lauser's cmake at:
# https://github.com/OPM/opm-parser/blob/master/cmake/Modules/CheckCaseSensitiveFileSystem.cmake
# Included in sdrangle (LGPL3) - with permission.
#
# Sets the following variable:
# HAVE_CASE_SENSITIVE_FILESYSTEM   True if the file system honors the case of files
#
# I dislike that we have to emit a file from CMake, but I can't think of a better way.

message(STATUS "Check for case-sensitive file systems")
string(RANDOM LENGTH 6 ALPHABET abcdefghijklmnopqrstuvwxyz TMP_FILE_L)
set(TMP_FILE_L "${TMP_FILE_L}.tmp")
string(TOUPPER ${TMP_FILE_L} TMP_FILE_U)
string(TIMESTAMP TMP_TIME)
set(TMP_FILE_CONTENTS "${TMP_FILE_L} ${TMP_TIME}")
# create a uppercase file
file(WRITE "${CMAKE_BINARY_DIR}/${TMP_FILE_U}" "${TMP_FILE_CONTENTS}")

# test if lowercase file can be opened
set(FileContents "")
if (EXISTS "${CMAKE_BINARY_DIR}/${TMP_FILE_L}")
	file(READ "${CMAKE_BINARY_DIR}/${TMP_FILE_L}" FileContents)
endif()

# remove the file
file(REMOVE "${CMAKE_BINARY_DIR}/${TMP_FILE_U}")

# check the contents
# If it is empty, the file system is case sensitive.
if ("${FileContents}" STREQUAL "${TMP_FILE_CONTENTS}")
	message(STATUS "File system is not case-sensitive")
	set(HAVE_CASE_SENSITIVE_FILESYSTEM 0)
else()
	message(STATUS "File system is case-sensitive")
	set(HAVE_CASE_SENSITIVE_FILESYSTEM 1)
endif()
