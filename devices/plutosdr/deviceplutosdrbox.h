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

#include <sstream>
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

    typedef enum
    {
        USE_RX,
        USE_TX
    } DeviceUse;

    struct Sample {
        int16_t i;
        int16_t q;
    };

    struct SampleRates {
        uint32_t m_bbRateHz;      //!< Baseband PLL rate (Hz) - used internally
        uint32_t m_addaConnvRate; //!< A/D or D/A converter rat - this is the HB3 working sample rate
        uint32_t m_hb3Rate;       //!< Rate of the HB3/(DEC3 or INT3) filter (Rx: out, Tx: in) - this is the HB2 working sample rate
        uint32_t m_hb2Rate;       //!< Rate of the HB2 filter (Rx: out, Tx: in) - this is the HB1 working sample rate
        uint32_t m_hb1Rate;       //!< Rate of the HB1 filter (Rx: out, Tx: in) - this is the FIR working sample rate
        uint32_t m_firRate;       //!< Rate of FIR filter (Rx: out, Tx: in) - this is the host/device communication sample rate
    };

    uint64_t m_devSampleRate;      //!< Host interface sample rate
    int32_t  m_LOppmTenths;        //!< XO correction
    bool     m_lpfFIREnable;       //!< enable digital lowpass FIR filter
    float    m_lpfFIRBW;           //!< digital lowpass FIR filter bandwidth (Hz)
    uint32_t m_lpfFIRlog2Decim;    //!< digital lowpass FIR filter log2 of decimation factor (0..2)
    int      m_lpfFIRRxGain;       //!< digital lowpass FIR filter gain Rx side (dB)
    int      m_lpfFIRTxGain;       //!< digital lowpass FIR filter gain Tx side (dB)

    DevicePlutoSDRBox(const std::string& uri);
    ~DevicePlutoSDRBox();
    bool isValid() const { return m_valid; }
    static bool probeURI(const std::string& uri);

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
    struct iio_channel *getTxChannel0I() { return m_chnTx0i; }
    struct iio_channel *getTxChannel0Q() { return m_chnTx0q; }
    ssize_t rxBufferRefill();
    ssize_t txBufferPush();
    std::ptrdiff_t rxBufferStep();
    char* rxBufferEnd();
    char* rxBufferFirst();
    std::ptrdiff_t txBufferStep();
    char* txBufferEnd();
    char* txBufferFirst();
    void txChannelConvert(int16_t *dst, int16_t *src);
    bool getRxSampleRates(SampleRates& sampleRates);
    bool getTxSampleRates(SampleRates& sampleRates);
    void setSampleRate(uint32_t sampleRate);
    void setFIR(uint32_t sampleRate, uint32_t intdec, DeviceUse use, uint32_t bw, int gain);
    void setFIREnable(bool enable);
    void setLOPPMTenths(int ppmTenths);
    bool getRxGain(int& gaindB, unsigned int chan);
    bool getRxRSSI(std::string& rssiStr, unsigned int chan);
    bool getTxRSSI(std::string& rssiStr, unsigned int chan);
    bool fetchTemp();
    float getTemp() const { return m_temp; }
    bool getRateGovernors(std::string& rateGovernors);

private:
    struct iio_context *m_ctx;
    struct iio_device  *m_devPhy;
    struct iio_device  *m_devRx;
    struct iio_device  *m_devTx;
    struct iio_channel *m_chnRx0;
    struct iio_channel *m_chnTx0i;
    struct iio_channel *m_chnTx0q;
    struct iio_buffer  *m_rxBuf;
    struct iio_buffer  *m_txBuf;
    bool m_valid;
    int64_t m_xoInitial;
    float m_temp;

    bool parseSampleRates(const std::string& rateStr, SampleRates& sampleRates);
    void setFilter(const std::string& filterConfigStr);
    void formatFIRHeader(std::ostringstream& str, uint32_t intdec);
    void formatFIRCoefficients(std::ostringstream& str, uint32_t nbTaps, double normalizedBW);
    void getXO();
    void setTracking();
};

#endif /* DEVICES_PLUTOSDR_DEVICEPLUTOSDRBOX_H_ */
