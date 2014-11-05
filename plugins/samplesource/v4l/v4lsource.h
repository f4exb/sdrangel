/* 
 * Copyright 2013 Antti Palosaari <crope@iki.fi>
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifndef INCLUDED_KERNEL_LIBV4L2_X_IMPL_H
#define INCLUDED_KERNEL_LIBV4L2_X_IMPL_H

#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <uapi/linux/videodev2.h>
#include "/usr/local/include/libv4l2.h"
#include "fcntl.h"
#include <sys/mman.h>

namespace kernel {
	// v4l2_mmap
	struct buffer {
		void *start;
		size_t length;
	};

	class v4l //: public v4l
	{
	private:
		// v4l2 device file handle
		int fd;

		// stream / sample format
		uint32_t pixelformat;

		struct buffer *buffers;
		unsigned int n_buffers;

		// for processing mmap buffer
		void *recebuf_ptr;
		unsigned int recebuf_len;
		unsigned int recebuf_mmap_index;

	public:
		v4l(const char *filename);
		~v4l();

		void set_samp_rate(double samp_rate);
		void set_center_freq(double freq);
		void set_bandwidth(double bandwidth);
		void set_tuner_gain(double gain);

		// Where all the action really happens
		int work(int noutput_items,
				void* input_items,
				void* output_items);
	};
} // namespace kernel

#endif /* INCLUDED_KERNEL_LIBV4L2_X_IMPL_H */

