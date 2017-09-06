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

#ifndef DEVICES_PLUTOSDR_DEVICEPLUTOSDRBOX_H_
#define DEVICES_PLUTOSDR_DEVICEPLUTOSDRBOX_H_

#include <stdint.h>
#include <sys/types.h>
#include "deviceplutosdrscan.h"

class DevicePlutoSDRBox
{
public:
    typedef enum
    {
        DEVICE_PHY,
        DEVICE_RX,
        DEVICE_TX
    } DeviceType;

    struct Sample {
        int16_t i;
        int16_t q;
    };

    DevicePlutoSDRBox(const std::string& uri);
    ~DevicePlutoSDRBox();
    bool isValid() const { return m_valid; }

    void set_params(DeviceType devType, const std::vector<std::string> &params);
    bool get_param(DeviceType devType, const std::string &param, std::string &value);
    bool openRx();
    bool openTx();
    void closeRx();
    void closeTx();
    struct iio_buffer *createRxBuffer(unsigned int size, bool cyclic);
    struct iio_buffer *createTxBuffer(unsigned int size, bool cyclic);
    void deleteRxBuffer();
    void deleteTxBuffer();
    ssize_t getRxSampleSize();
    ssize_t getTxSampleSize();
    struct iio_channel *getRxChannel0() { return m_chnRx0; }
    struct iio_channel *getTxChannel0() { return m_chnTx0; }
    ssize_t rxBufferRefill();
    ssize_t txBufferPush();
    std::ptrdiff_t rxBufferStep();
    char* rxBufferEnd();
    char* rxBufferFirst();
    std::ptrdiff_t txBufferStep();
    char* txBufferEnd();
    char* txBufferFirst();

private:
    struct iio_context *m_ctx;
    struct iio_device  *m_devPhy;
    struct iio_device  *m_devRx;
    struct iio_device  *m_devTx;
    struct iio_channel *m_chnRx0;
    struct iio_channel *m_chnTx0;
    struct iio_buffer  *m_rxBuf;
    struct iio_buffer  *m_txBuf;
    bool m_valid;
};

#endif /* DEVICES_PLUTOSDR_DEVICEPLUTOSDRBOX_H_ */
