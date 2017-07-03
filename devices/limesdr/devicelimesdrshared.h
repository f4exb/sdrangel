///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef DEVICES_LIMESDR_DEVICELIMESDRSHARED_H_
#define DEVICES_LIMESDR_DEVICELIMESDRSHARED_H_

#include <cstddef>
#include "devicelimesdrparam.h"
#include "util/message.h"

/**
 * Structure shared by a buddy with other buddies
 */
class DeviceLimeSDRShared
{
public:
    class MsgCrossReportToGUI : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }

        static MsgCrossReportToGUI* create(int sampleRate)
        {
            return new MsgCrossReportToGUI(sampleRate);
        }

    private:
        int m_sampleRate;

        MsgCrossReportToGUI(int sampleRate) :
            Message(),
            m_sampleRate(sampleRate)
        { }
    };

    class MsgReportDeviceInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getTemperature() const { return m_temperature; }

        static MsgReportDeviceInfo* create(float temperature)
        {
            return new MsgReportDeviceInfo(temperature);
        }

    private:
        float    m_temperature;

        MsgReportDeviceInfo(float temperature) :
            Message(),
            m_temperature(temperature)
        { }
    };

    class ThreadInterface
    {
    public:
        virtual void startWork() = 0;
        virtual void stopWork() = 0;
        virtual void setDeviceSampleRate(int sampleRate) = 0;
    };

    DeviceLimeSDRParams *m_deviceParams; //!< unique hardware device parameters
    std::size_t         m_channel;       //!< logical device channel number (-1 if none)
    ThreadInterface     *m_thread;       //!< holds the thread address if started else 0
    int                 m_ncoFrequency;
    uint64_t            m_centerFrequency;
    uint32_t            m_log2Soft;

    DeviceLimeSDRShared() :
        m_deviceParams(0),
        m_channel(-1),
        m_thread(0),
        m_ncoFrequency(0),
        m_centerFrequency(0),
        m_log2Soft(0)
    {}

    ~DeviceLimeSDRShared()
    {}
};

#endif /* DEVICES_LIMESDR_DEVICELIMESDRSHARED_H_ */
