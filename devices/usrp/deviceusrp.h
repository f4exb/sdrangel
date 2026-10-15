///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Jon Beniston, M7RCE <jon@beniston.com>                     //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef DEVICES_USRP_DEVICEUSRP_H_
#define DEVICES_USRP_DEVICEUSRP_H_

#include <QString>

#include <uhd/usrp/multi_usrp.hpp>

#include "plugin/plugininterface.h"
#include "export.h"

class DEVICES_API DeviceUSRP
{
public:

    /** Enumeration of USRP hardware devices */
    static void enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices);

    /** Wait for ref clock and LO to lock */
    static void waitForLock(uhd::usrp::multi_usrp::sptr usrp, const QString& clockSource, int channel, bool rx);

    /** Determine whether two Rx or two Tx channels share the same LO (and sample rate) */
    static bool channelsShareLO(uhd::usrp::multi_usrp::sptr usrp, bool rx, int channelA, int channelB);

    /** Get board temperature in degrees C. Returns false if the device doesn't have a temperature sensor */
    static bool getTemperature(uhd::usrp::multi_usrp::sptr usrp, bool rx, int channel, double& temperature);

    /** Get number of samples to read per recv() call, which is a power of two, as required by decimators */
    static size_t getRecvBufferSamples(size_t maxNumSamps, int sampleRate);

    /** Set priority of calling thread, so it can stream samples without overflowing / underflowing */
    static void setStreamingThreadPriority();

    /**
     * Call before setting all channels to the same sample rate, on devices where UHD chooses the master clock rate automatically (B2xx).
     * Sets a master clock rate suitable for the new sample rate and disables automatic selection.
     * Returns true if automatic selection should be re-enabled with enableAutoMasterClockRate() once the sample rates have been set.
     */
    static bool setMasterClockRateForSampleRate(uhd::usrp::multi_usrp::sptr usrp, double sampleRate, size_t nbChannels);
    static void enableAutoMasterClockRate(uhd::usrp::multi_usrp::sptr usrp);

    /**
     * Set sample rate of a single channel. If it can't be achieved with a master clock rate UHD chooses automatically
     * to also suit other channels' rates (E.g. a buddy's), the master clock rate is set for the requested rate,
     * and other channels' rates are coerced, so need to be read back.
     */
    static void setSampleRate(uhd::usrp::multi_usrp::sptr usrp, bool rx, int channel, double sampleRate);

    /** Get actual sample rate of a channel, which may have been changed by the master clock rate being changed for another channel */
    static double getSampleRate(uhd::usrp::multi_usrp::sptr usrp, bool rx, int channel);

    /** Timeout for recv() and send(), in seconds */
    static constexpr double m_streamTimeout = 0.5;
};

#endif /* DEVICES_USRP_DEVICEUSRP_H_ */
