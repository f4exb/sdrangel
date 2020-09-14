///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef DEVICES_BLADERF2_DEVICEBLADERF2_H_
#define DEVICES_BLADERF2_DEVICEBLADERF2_H_

#include <stdint.h>
#include <libbladeRF.h>

#include "plugin/plugininterface.h"
#include "export.h"

class DEVICES_API DeviceBladeRF2
{
public:
    DeviceBladeRF2();
    ~DeviceBladeRF2();

    static void enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices);
    bool open(const char *serial);
    void close();

    bladerf *getDev() { return m_dev; }

    bool openRx(int channel);
    bool openTx(int channel);
    void closeRx(int channel);
    void closeTx(int channel);

    void getFrequencyRangeRx(uint64_t& min, uint64_t& max, int& step, float& scale);
    void getFrequencyRangeTx(uint64_t& min, uint64_t& max, int& step, float& scale);
    void getSampleRateRangeRx(int& min, int& max, int& step, float& scale);
    void getSampleRateRangeTx(int& min, int& max, int& step, float& scale);
    void getBandwidthRangeRx(int& min, int& max, int& step, float& scale);
    void getBandwidthRangeTx(int& min, int& max, int& step, float& scale);
    void getGlobalGainRangeRx(int& min, int& max, int& step, float& scale);
    void getGlobalGainRangeTx(int& min, int& max, int& step, float& scale);
    int  getGainModesRx(const bladerf_gain_modes**);
    void setBiasTeeRx(bool enable);
    void setBiasTeeTx(bool enable);

    static const unsigned int blockSize = (1<<14);

private:
    bladerf *m_dev;
    int m_nbRxChannels;
    int m_nbTxChannels;
    bool *m_rxOpen;
    bool *m_txOpen;

    static struct bladerf *open_bladerf_from_serial(const char *serial);
};



#endif /* DEVICES_BLADERF2_DEVICEBLADERF2_H_ */
