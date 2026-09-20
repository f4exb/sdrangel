############################################################################
# FindOpus.txt
# Copyright (C) 2014  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################
#
# - Find the opus include file and library
#
#  OPUS_FOUND - system has opus
#  OPUS_INCLUDE_DIRS - the opus include directory
#  OPUS_LIBRARIES - The libraries needed to use opus

find_path(OPUS_INCLUDE_DIRS
	NAMES opus/opus.h
	PATH_SUFFIXES include
)
if(OPUS_INCLUDE_DIRS)
	set(HAVE_OPUS_OPUS_H 1)
endif()

find_library(OPUS_LIBRARIES NAMES opus)

if(OPUS_LIBRARIES)
	find_library(LIBM NAMES m)
	if(LIBM)
		list(APPEND OPUS_LIBRARIES ${LIBM})
	endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Opus
	DEFAULT_MSG
	OPUS_INCLUDE_DIRS OPUS_LIBRARIES HAVE_OPUS_OPUS_H
)

mark_as_advanced(OPUS_INCLUDE_DIRS OPUS_LIBRARIES HAVE_OPUS_OPUS_H)
