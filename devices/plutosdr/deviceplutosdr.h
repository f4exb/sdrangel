///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef DEVICES_PLUTOSDR_DEVICEPLUTOSDR_H_
#define DEVICES_PLUTOSDR_DEVICEPLUTOSDR_H_

#include <stdint.h>

#include "deviceplutosdrscan.h"
#include "deviceplutosdrbox.h"

#include "export.h"

class DEVICES_API DevicePlutoSDR
{
public:
    static DevicePlutoSDR& instance();
    void scan() { m_scan.scan(); }
    void getSerials(std::vector<std::string>& serials) const { m_scan.getSerials(serials); }
    int getNbDevices() const { return m_scan.getNbDevices(); }
    const std::string* getURIAt(unsigned int index) const { return m_scan.getURIAt(index); }
    const std::string* getSerialAt(unsigned int index) const { return m_scan.getSerialAt(index); }
    DevicePlutoSDRBox* getDeviceFromURI(const std::string& uri);
    DevicePlutoSDRBox* getDeviceFromSerial(const std::string& serial);
    void enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices) {
        m_scan.enumOriginDevices(hardwareId, originDevices);
    }

    static const uint64_t rxLOLowLimitFreq;  //!< Rx LO hard coded lower frequency limit (Hz)
    static const uint64_t rxLOHighLimitFreq; //!< Rx LO hard coded lower frequency limit (Hz)
    static const uint64_t txLOLowLimitFreq;  //!< Tx LO hard coded lower frequency limit (Hz)
    static const uint64_t txLOHighLimitFreq; //!< Tx LO hard coded lower frequency limit (Hz)
    static const uint32_t srLowLimitFreq;  //!< Device sample rate lower limit in S/s
    static const uint32_t srHighLimitFreq; //!< Device sample rate higher limit in S/s
    static const uint32_t bbLPRxLowLimitFreq;   //!< Analog base band Rx low pass filter lower frequency limit (Hz)
    static const uint32_t bbLPRxHighLimitFreq;  //!< Analog base band Rx high pass filter lower frequency limit (Hz)
    static const uint32_t bbLPTxLowLimitFreq;   //!< Analog base band Tx low pass filter lower frequency limit (Hz)
    static const uint32_t bbLPTxHighLimitFreq;  //!< Analog base band Tx high pass filter lower frequency limit (Hz)
    static const float firBWLowLimitFactor;  //!< Factor by which the FIR working sample rate is multiplied to yield bandwidth lower limit
    static const float firBWHighLimitFactor; //!< Factor by which the FIR working sample rate is multiplied to yield bandwidth higher limit
protected:
    DevicePlutoSDR();
    ~DevicePlutoSDR();

private:
    DevicePlutoSDRScan m_scan;
};

#endif /* DEVICES_PLUTOSDR_DEVICEPLUTOSDR_H_ */
