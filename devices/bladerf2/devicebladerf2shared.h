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

#ifndef DEVICES_BLADERF2_DEVICEBLADERF2SHARED_H_
#define DEVICES_BLADERF2_DEVICEBLADERF2SHARED_H_

#include "devicebladerf2.h"

class SampleSinkFifo;
class SampleSourceFifo;

/**
 * Structure shared by a buddy with other buddies
 */
class DEVICES_API DeviceBladeRF2Shared
{
public:
    class InputThreadInterface
    {
    public:
        virtual void startWork() = 0;
        virtual void stopWork() = 0;
        virtual bool isRunning() const = 0;
        virtual void setFifo(unsigned int channel, SampleSinkFifo *fifo) = 0;
        virtual SampleSinkFifo *getFifo(unsigned int channel) = 0;
    };

    class OutputThreadInterface
    {
    public:
        virtual void startWork() = 0;
        virtual void stopWork() = 0;
        virtual bool isRunning() = 0;
        virtual void setFifo(unsigned int channel, SampleSourceFifo *fifo) = 0;
        virtual SampleSourceFifo *getFifo(unsigned int channel) = 0;
    };

    DeviceBladeRF2Shared();
    ~DeviceBladeRF2Shared();

    DeviceBladeRF2 *m_dev;
    int m_channel; //!< allocated channel (-1 if none)
    InputThreadInterface *m_inputThread;   //!< The SISO/MIMO input thread
    OutputThreadInterface *m_outputThread; //!< The SISO/MIMO output thread
};



#endif /* DEVICES_BLADERF2_DEVICEBLADERF2SHARED_H_ */
