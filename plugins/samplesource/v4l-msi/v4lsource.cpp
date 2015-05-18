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

#include "v4linput.h"
#include "v4lthread.h"
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <uapi/linux/videodev2.h>
#include "libv4l2.h"
#include <fcntl.h>
#include <sys/mman.h>

/* Control classes */
#define V4L2_CTRL_CLASS_USER          0x00980000 /* Old-style 'user' controls */
/* User-class control IDs */
#define V4L2_CID_BASE                 (V4L2_CTRL_CLASS_USER | 0x900)
#define V4L2_CID_USER_BASE            V4L2_CID_BASE

#define CID_SAMPLING_MODE             ((V4L2_CID_USER_BASE | 0xf000) +  0)
#define CID_SAMPLING_RATE             ((V4L2_CID_USER_BASE | 0xf000) +  1)
#define CID_SAMPLING_RESOLUTION       ((V4L2_CID_USER_BASE | 0xf000) +  2)
#define CID_TUNER_RF                  ((V4L2_CID_USER_BASE | 0xf000) + 10)
#define CID_TUNER_BW                  ((V4L2_CID_USER_BASE | 0xf000) + 11)
#define CID_TUNER_IF                  ((V4L2_CID_USER_BASE | 0xf000) + 12)
#define CID_TUNER_GAIN                ((V4L2_CID_USER_BASE | 0xf000) + 13)

#define V4L2_PIX_FMT_SDR_U8     v4l2_fourcc('C', 'U', '0', '8') /* unsigned 8-bit Complex*/
#define V4L2_PIX_FMT_SDR_U16LE  v4l2_fourcc('C', 'U', '1', '6') /* unsigned 16-bit Complex*/
#define V4L2_PIX_FMT_SDR_CS14LE	v4l2_fourcc('C', 'S', '1', '4') /*  signed  14-bit Complex*/

#define CLEAR(x) memset(&(x), 0, sizeof(x))

static void xioctl(int fh, unsigned long int request, void *arg)
{
	int ret;

	do {
		ret = v4l2_ioctl(fh, request, arg);
	} while (ret == -1 && ((errno == EINTR) || (errno == EAGAIN)));
	if (ret == -1) {
		qCritical("V4L2 ioctl error: %d", errno);
	}
}

void
V4LThread::OpenSource(const char *filename)
	{
		struct v4l2_format fmt;
		struct v4l2_buffer buf;
		struct v4l2_requestbuffers req;
		enum v4l2_buf_type type;
		unsigned int i;

		recebuf_len = 0;

		// fd = v4l2_open(filename, O_RDWR | O_NONBLOCK, 0);
		fd = open(filename, O_RDWR | O_NONBLOCK, 0);
		if (fd < 0) {
			qCritical("Cannot open /dev/swradio0 :%d", fd);
			return;
		}

		pixelformat = V4L2_PIX_FMT_SDR_CS14LE;
		qCritical("Want Pixelformat : S14");
		CLEAR(fmt);
		fmt.type = V4L2_BUF_TYPE_SDR_CAPTURE;
		fmt.fmt.sdr.pixelformat = pixelformat;
		xioctl(fd, VIDIOC_S_FMT, &fmt);
		qCritical("Got Pixelformat : %4.4s", (char *)&fmt.fmt.sdr.pixelformat);

		CLEAR(req);
		req.count = 8;
		req.type = V4L2_BUF_TYPE_SDR_CAPTURE;
		req.memory = V4L2_MEMORY_MMAP;
		xioctl(fd, VIDIOC_REQBUFS, &req);

		buffers = (struct v4l_buffer*) calloc(req.count, sizeof(*buffers));
		for (n_buffers = 0; n_buffers < req.count; n_buffers++) {
			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_SDR_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = n_buffers;
			xioctl(fd, VIDIOC_QUERYBUF, &buf);

			buffers[n_buffers].length = buf.length;
			buffers[n_buffers].start = v4l2_mmap(NULL, buf.length,
					PROT_READ | PROT_WRITE, MAP_SHARED,
					fd, buf.m.offset);

			if (buffers[n_buffers].start == MAP_FAILED) {
				qCritical("V4L2 buffer mmap failed");
			}
		}

		for (i = 0; i < n_buffers; i++) {
			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_SDR_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = i;
			xioctl(fd, VIDIOC_QBUF, &buf);
		}

		set_sample_rate((double)SAMPLERATE);
		set_center_freq( (double)centerFreq );
		// start streaming
		type = V4L2_BUF_TYPE_SDR_CAPTURE;
		xioctl(fd, VIDIOC_STREAMON, &type);
	}

void
V4LThread::CloseSource()
	{
		unsigned int i;
		enum v4l2_buf_type type;

		// stop streaming
		type = V4L2_BUF_TYPE_SDR_CAPTURE;
		xioctl(fd, VIDIOC_STREAMOFF, &type);

		for (i = 0; i < n_buffers; i++)
			v4l2_munmap(buffers[i].start, buffers[i].length);

		v4l2_close(fd);
		fd = -1;
	}

void
V4LThread::set_sample_rate(double samp_rate)
	{
		struct v4l2_frequency frequency;

		memset (&frequency, 0, sizeof(frequency));
		frequency.tuner = 0;
		frequency.type = V4L2_TUNER_ADC;
		frequency.frequency = samp_rate;

		xioctl(fd, VIDIOC_S_FREQUENCY, &frequency);

		return;
	}

// Cannot change freq while streaming in Linux 4.0
void
V4LThread::set_center_freq(double freq)
	{
		struct v4l2_frequency frequency;

		if (fd <= 0)
			return;
		memset (&frequency, 0, sizeof(frequency));
		frequency.tuner = 1;
		frequency.type = V4L2_TUNER_RF;
		frequency.frequency = freq;

		xioctl(fd, VIDIOC_S_FREQUENCY, &frequency);

		return;
	}

void
V4LThread::set_bandwidth(double bandwidth)
	{
		struct v4l2_ext_controls ext_ctrls;
		struct v4l2_ext_control ext_ctrl;

		memset (&ext_ctrl, 0, sizeof(ext_ctrl));
		ext_ctrl.id = CID_TUNER_BW;
		ext_ctrl.value = bandwidth;

		memset (&ext_ctrls, 0, sizeof(ext_ctrls));
		ext_ctrls.ctrl_class = V4L2_CTRL_CLASS_USER;
		ext_ctrls.count = 1;
		ext_ctrls.controls = &ext_ctrl;

		xioctl(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls);

		return;
	}

void
V4LThread::set_tuner_gain(double gain)
	{
		struct v4l2_ext_controls ext_ctrls;
		struct v4l2_ext_control ext_ctrl;

		if (fd <= 0)
			return;
		memset (&ext_ctrl, 0, sizeof(ext_ctrl));
		ext_ctrl.id = CID_TUNER_GAIN;
		ext_ctrl.value = gain;

		memset (&ext_ctrls, 0, sizeof(ext_ctrls));
		ext_ctrls.ctrl_class = V4L2_CTRL_CLASS_USER;
		ext_ctrls.count = 1;
		ext_ctrls.controls = &ext_ctrl;

		xioctl(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls);

		return;
	}

#define CASTUP (int)(qint16)
int
V4LThread::work(int noutput_items)
{
	int ret;
	struct timeval tv;
	struct v4l2_buffer buf;
	fd_set fds;
	int xreal, yimag;
	uint16_t* b;
	SampleVector::iterator it;

	unsigned int pos = 0;
	// MSI format is 252 sample pairs :( 63 * 4) * 4bytes
	it = m_convertBuffer.begin();
	if (recebuf_len >= 16) { // in bytes
		b = (uint16_t *) recebuf_ptr;
		unsigned int len = 8 * noutput_items; // decimation (i+q * 4 : cmplx)
		if (len * 2 > recebuf_len)
			len = recebuf_len / 2;
		// Decimate by two for lower cpu usage
		for (pos = 0; pos < len - 7; pos += 8) {
			xreal = CASTUP(b[pos+0]<<2) + CASTUP(b[pos+2]<<2)
				+ CASTUP(b[pos+4]<<2) + CASTUP(b[pos+6]<<2);
			yimag = CASTUP(b[pos+1]<<2) + CASTUP(b[pos+3]<<2)
				+ CASTUP(b[pos+5]<<2) + CASTUP(b[pos+7]<<2);
			Sample s( (qint16)(xreal >> 2) , (qint16)(yimag >> 2) );
			*it = s;
			it++;
		}
		m_sampleFifo->write(m_convertBuffer.begin(), it);
		recebuf_len -= pos * 2; // size of int16
		recebuf_ptr = (void*)(b + pos);	
	}
	// return now if there is still data in buffer, else free buffer and get another.
	if (recebuf_len >= 16)
		return pos / 8;
	{	// free buffer, if there was one.
		if (pos > 0) { 
			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_SDR_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = recebuf_mmap_index;
			xioctl(fd, VIDIOC_QBUF, &buf);
		}

		/* Read data from device */
		do {
			FD_ZERO(&fds);
			FD_SET(fd, &fds);

			// Timeout
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			ret = select(fd + 1, &fds, NULL, NULL, &tv);
		} while ((ret == -1 && (errno = EINTR)));
		if (ret == -1) {
			perror("select");
			return errno;
		}

		/* dequeue mmap buf (receive data) */
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_SDR_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		xioctl(fd, VIDIOC_DQBUF, &buf);

		/* store buffer in order to handle it during next call */
		recebuf_ptr = buffers[buf.index].start;
		recebuf_len = buf.bytesused;
		recebuf_mmap_index = buf.index;
	}
	return pos / 8;
}
