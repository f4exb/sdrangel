///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QtGlobal>
#include "perseusthread.h"

void PerseusThread::run()
{
	m_running = true;
	m_startWaiter.wakeAll();

	int rc = perseus_start_async_input(m_dev, 6*1024, rx_callback, 0);

	if (rc < 0) {
		qCritical("PerseusThread::run: failed to start Perseus Rx: %s", perseus_errorstr());
	}
	else
	{
		while (m_running) {
			sleep(1);
		}
	}

	rc = perseus_stop_async_input(m_dev);

	if (rc < 0) {
		qCritical("PerseusThread::run: failed to stop Perseus Rx: %s", perseus_errorstr());
	} else {
		qDebug("PerseusThread::run: stopped Perseus Rx");
	}

	m_running = false;
}

void PerseusThread::callback(const uint8_t* buf, qint32 len)
{

}

int PerseusThread::rx_callback(void *buf, int buf_size, void *extra)
{
	qint32 nbIAndQ = buf_size / 3; // 3 bytes per I or Q
	m_this->callback((uint8_t*) buf, nbIAndQ);
}

