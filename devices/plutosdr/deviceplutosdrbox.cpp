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

#include <iostream>
#include <cstdio>
#include <cstring>
#include <regex>
#include <iio.h>
#include <boost/lexical_cast.hpp>
#include <QtGlobal>

#include "dsp/dsptypes.h"
#include "dsp/wfir.h"
#include "deviceplutosdr.h"
#include "deviceplutosdrbox.h"

DevicePlutoSDRBox::DevicePlutoSDRBox(const std::string& uri) :
        m_devSampleRate(0),
        m_LOppmTenths(0),
        m_lpfFIREnable(false),
        m_lpfFIRBW(100.0f),
        m_lpfFIRlog2Decim(0),
        m_lpfFIRRxGain(0),
        m_lpfFIRTxGain(0),
        m_ctx(nullptr),
        m_devPhy(nullptr),
        m_devRx(nullptr),
        m_devTx(nullptr),
        m_rxBuf(nullptr),
        m_txBuf(nullptr),
        m_xoInitial(0),
        m_temp(0.0f),
        m_rxSampleBytes(0),
        m_txSampleBytes(0)
{
    m_ctx = iio_create_context_from_uri(uri.c_str());

    if (m_ctx)
    {
        m_devPhy = iio_context_find_device(m_ctx, "ad9361-phy");
        m_devRx = iio_context_find_device(m_ctx, "cf-ad9361-lpc");
        m_devTx = iio_context_find_device(m_ctx, "cf-ad9361-dds-core-lpc");
    }
    else
    {
        qCritical("DevicePlutoSDRBox::DevicePlutoSDRBox: cannot create context for uri: %s", uri.c_str());
    }

    m_valid = m_ctx && m_devPhy && m_devRx && m_devTx;

    if (m_valid)
    {
        std::regex channelIdReg("voltage([0-9]+)");

        getXO();
        int nbRxChannels = iio_device_get_channels_count(m_devRx);

        for (int i = 0; i < nbRxChannels; i++)
        {
            iio_channel *chn = iio_device_get_channel(m_devRx, i);
            std::string channelId(iio_channel_get_id(chn));

            if (std::regex_match(channelId, channelIdReg))
            {
                int nbAttributes = iio_channel_get_attrs_count(chn);
                m_rxChannelIds.append(QString(channelId.c_str()));
                m_rxChannels.append(chn);
                qDebug("DevicePlutoSDRBox::DevicePlutoSDRBox: Rx: %s #Attrs: %d", channelId.c_str(), nbAttributes);
            }
        }

        int nbTxChannels = iio_device_get_channels_count(m_devTx);

        for (int i = 0; i < nbTxChannels; i++)
        {
            iio_channel *chn = iio_device_get_channel(m_devTx, i);
            std::string channelId(iio_channel_get_id(chn));

            if (std::regex_match(channelId, channelIdReg))
            {
                int nbAttributes = iio_channel_get_attrs_count(chn);
                m_txChannelIds.append(QString(channelId.c_str()));
                m_txChannels.append(chn);
                qDebug("DevicePlutoSDRBox::DevicePlutoSDRBox: Tx: %s #Attrs: %d", channelId.c_str(), nbAttributes);
            }
        }
    }
}

DevicePlutoSDRBox::~DevicePlutoSDRBox()
{
    deleteRxBuffer();
    deleteTxBuffer();
    closeRx();
    closeTx();
    if (m_ctx) { iio_context_destroy(m_ctx); }
}

bool DevicePlutoSDRBox::probeURI(const std::string& uri)
{
    bool retVal;
    struct iio_context *ctx;

    ctx = iio_create_context_from_uri(uri.c_str());
    retVal = (ctx != 0);

    if (ctx) {
        iio_context_destroy(ctx);
    }

    return retVal;
}

void DevicePlutoSDRBox::set_params(DeviceType devType,
        const std::vector<std::string>& params)
{
    iio_device *dev;

    switch (devType)
    {
    case DEVICE_PHY:
        dev = m_devPhy;
        break;
    case DEVICE_RX:
        dev = m_devRx;
        break;
    case DEVICE_TX:
        dev = m_devTx;
        break;
    default:
        dev = m_devPhy;
        break;
    }

    for (std::vector<std::string>::const_iterator it = params.begin(); it != params.end(); ++it)
    {
        struct iio_channel *chn = 0;
        const char *attr = 0;
        std::size_t pos;
        int ret;
        int type;

        pos = it->find('=');

        if (pos == std::string::npos)
        {
            std::cerr << "DevicePlutoSDRBox::set_params: Malformed line: " << *it << std::endl;
            continue;
        }

        std::string key = it->substr(0, pos);
        std::string val = it->substr(pos + 1, std::string::npos);

        ret = iio_device_identify_filename(dev, key.c_str(), &chn, &attr);

        if (ret)
        {
            std::cerr << "DevicePlutoSDRBox::set_params: Parameter not recognized: " << key << std::endl;
            continue;
        }

        if (chn) {
            ret = iio_channel_attr_write(chn, attr, val.c_str());
            type = 0;
        } else if (iio_device_find_attr(dev, attr)) {
            ret = iio_device_attr_write(dev, attr, val.c_str());
            type = 1;
        } else {
            ret = iio_device_debug_attr_write(dev, attr, val.c_str());
            type = 2;
        }

        if (ret < 0)
        {
            std::string item;
           char errstr[256];

            switch (type)
            {
            case 0:
                item = "channel";
                break;
            case 1:
                item = "device";
                break;
            case 2:
                item = "debug";
                break;
            default:
                item = "unknown";
                break;
            }
            iio_strerror(-ret, errstr, 255);
            std::cerr << "DevicePlutoSDRBox::set_params: Unable to write " << item << " attribute " << key << "=" << val << ": " << errstr << " (" << ret << ") " << std::endl;
        }
        else
        {
            std::cerr << "DevicePlutoSDRBox::set_params: set attribute " << key << "=" << val << ": " << ret << std::endl;
        }
    }
}

bool DevicePlutoSDRBox::get_param(DeviceType devType, const std::string &param, std::string &value)
{
    struct iio_channel *chn = 0;
    const char *attr = 0;
    char valuestr[256];
    int ret;
    ssize_t nchars;
    iio_device *dev;

    switch (devType)
    {
    case DEVICE_PHY:
        dev = m_devPhy;
        break;
    case DEVICE_RX:
        dev = m_devRx;
        break;
    case DEVICE_TX:
        dev = m_devTx;
        break;
    default:
        dev = m_devPhy;
        break;
    }

    ret = iio_device_identify_filename(dev, param.c_str(), &chn, &attr);

    if (ret)
    {
        std::cerr << "DevicePlutoSDRBox::get_param: Parameter not recognized: " << param << std::endl;
        return false;
    }

    if (chn) {
        nchars = iio_channel_attr_read(chn, attr, valuestr, 256);
    } else if (iio_device_find_attr(dev, attr)) {
        nchars = iio_device_attr_read(dev, attr, valuestr, 256);
    } else {
        nchars = iio_device_debug_attr_read(dev, attr, valuestr, 256);
    }

    if (nchars < 0)
    {
        std::cerr << "DevicePlutoSDRBox::get_param: Unable to read attribute " << param <<  ": " << nchars << std::endl;
        return false;
    }
    else
    {
        value.assign(valuestr);
        return true;
    }
}

void DevicePlutoSDRBox::setFilter(const std::string &filterConfigStr)
{
    int ret;

    ret = iio_device_attr_write_raw(m_devPhy, "filter_fir_config", filterConfigStr.c_str(), filterConfigStr.size());

    if (ret < 0)
    {
        std::cerr << "DevicePlutoSDRBox::set_filter: Unable to set: " <<  filterConfigStr <<  ": " << ret << std::endl;
    }
}

bool DevicePlutoSDRBox::openRx()
{
    if (!m_valid) { return false; }

    if (m_rxChannels.size() > 0)
    {
        iio_channel_enable(m_rxChannels.at(0));

        const struct iio_data_format *df = iio_channel_get_data_format(m_rxChannels.at(0));
        qDebug("DevicePlutoSDRBox::openRx channel I: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
                df->length,
                df->bits,
                df->shift,
                df->is_signed ? "true" : "false",
                df->is_be ? "true" : "false",
                df->with_scale? "true" : "false",
                df->scale,
                df->repeat);
        m_rxSampleBytes = df->length / 8;
    }
    else
    {
        qWarning("DevicePlutoSDRBox::openRx: open channel I failed");
        return false;
    }

    if (m_rxChannels.size() > 1)
    {
        iio_channel_enable(m_rxChannels.at(1));

        const struct iio_data_format* df = iio_channel_get_data_format(m_rxChannels.at(1));
        qDebug("DevicePlutoSDRBox::openRx channel Q: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
            df->length,
            df->bits,
            df->shift,
            df->is_signed ? "true" : "false",
            df->is_be ? "true" : "false",
            df->with_scale ? "true" : "false",
            df->scale,
            df->repeat);
        return true;
    }
    else
    {
        qWarning("DevicePlutoSDRBox::openRx: open channel Q failed");
        return false;
    }
}

bool DevicePlutoSDRBox::openSecondRx()
{
    if (!m_valid) { return false; }

    if (m_rxChannels.size() > 2)
    {
        iio_channel_enable(m_rxChannels.at(2));

        const struct iio_data_format *df = iio_channel_get_data_format(m_rxChannels.at(2));
        qDebug("DevicePlutoSDRBox::openSecondRx channel I: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
                df->length,
                df->bits,
                df->shift,
                df->is_signed ? "true" : "false",
                df->is_be ? "true" : "false",
                df->with_scale? "true" : "false",
                df->scale,
                df->repeat);
        m_rxSampleBytes = df->length / 8;
    }
    else
    {
        qWarning("DevicePlutoSDRBox::openSecondRx: open channel I failed");
        return false;
    }

    if (m_rxChannels.size() > 3)
    {
        iio_channel_enable(m_rxChannels.at(3));

        const struct iio_data_format* df = iio_channel_get_data_format(m_rxChannels.at(3));
        qDebug("DevicePlutoSDRBox::openSecondRx channel Q: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
            df->length,
            df->bits,
            df->shift,
            df->is_signed ? "true" : "false",
            df->is_be ? "true" : "false",
            df->with_scale ? "true" : "false",
            df->scale,
            df->repeat);
        return true;
    }
    else
    {
        qWarning("DevicePlutoSDRBox::openSecondRx: open channel Q failed");
        return false;
    }
}

bool DevicePlutoSDRBox::openTx()
{
    if (!m_valid) { return false; }

    if (m_txChannels.size() > 0)
    {
        iio_channel_enable(m_txChannels.at(0));
        const struct iio_data_format *df = iio_channel_get_data_format(m_txChannels.at(0));
        qDebug("DevicePlutoSDRBox::openTx: channel I: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
                df->length,
                df->bits,
                df->shift,
                df->is_signed ? "true" : "false",
                df->is_be ? "true" : "false",
                df->with_scale? "true" : "false",
                df->scale,
                df->repeat);
        m_txSampleBytes = df->length / 8;
    }
    else
    {
        std::cerr << "DevicePlutoSDRBox::openTx: failed to open I channel" << std::endl;
        return false;
    }

    if (m_txChannels.size() > 1)
    {
        iio_channel_enable(m_txChannels.at(1));
        const struct iio_data_format *df = iio_channel_get_data_format(m_txChannels.at(1));
        qDebug("DevicePlutoSDRBox::openTx: channel Q: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
                df->length,
                df->bits,
                df->shift,
                df->is_signed ? "true" : "false",
                df->is_be ? "true" : "false",
                df->with_scale? "true" : "false",
                df->scale,
                df->repeat);
        return true;
    }
    else
    {
        std::cerr << "DevicePlutoSDRBox::openTx: failed to open Q channel" << std::endl;
        return false;
    }
}

bool DevicePlutoSDRBox::openSecondTx()
{
    if (!m_valid) { return false; }

    if (m_txChannels.size() > 2)
    {
        iio_channel_enable(m_txChannels.at(2));
        const struct iio_data_format *df = iio_channel_get_data_format(m_txChannels.at(2));
        qDebug("DevicePlutoSDRBox::openSecondTx: channel I: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
                df->length,
                df->bits,
                df->shift,
                df->is_signed ? "true" : "false",
                df->is_be ? "true" : "false",
                df->with_scale? "true" : "false",
                df->scale,
                df->repeat);
        m_txSampleBytes = df->length / 8;
    }
    else
    {
        qWarning("DevicePlutoSDRBox::openSecondTx: failed to open I channel");
        return false;
    }

    if (m_txChannels.size() > 3)
    {
        iio_channel_enable(m_txChannels.at(3));
        const struct iio_data_format *df = iio_channel_get_data_format(m_txChannels.at(3));
        qDebug("DevicePlutoSDRBox::openSecondTx: channel Q: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
                df->length,
                df->bits,
                df->shift,
                df->is_signed ? "true" : "false",
                df->is_be ? "true" : "false",
                df->with_scale? "true" : "false",
                df->scale,
                df->repeat);
        return true;
    }
    else
    {
        qWarning("DevicePlutoSDRBox::openSecondTx: failed to open Q channel");
        return false;
    }
}

void DevicePlutoSDRBox::closeRx()
{
    if (m_rxChannels.size() > 0) { iio_channel_disable(m_rxChannels.at(0)); }
    if (m_rxChannels.size() > 1) { iio_channel_disable(m_rxChannels.at(1)); }
}

void DevicePlutoSDRBox::closeSecondRx()
{
    if (m_rxChannels.size() > 2) { iio_channel_disable(m_rxChannels.at(2)); }
    if (m_rxChannels.size() > 3) { iio_channel_disable(m_rxChannels.at(3)); }
}

void DevicePlutoSDRBox::closeTx()
{
    if (m_txChannels.size() > 0) { iio_channel_disable(m_txChannels.at(0)); }
    if (m_txChannels.size() > 1) { iio_channel_disable(m_txChannels.at(1)); }
}

void DevicePlutoSDRBox::closeSecondTx()
{
    if (m_txChannels.size() > 2) { iio_channel_disable(m_txChannels.at(2)); }
    if (m_txChannels.size() > 3) { iio_channel_disable(m_txChannels.at(3)); }
}

struct iio_buffer *DevicePlutoSDRBox::createRxBuffer(unsigned int size, bool cyclic)
{
    if (m_devRx) {
        m_rxBuf = iio_device_create_buffer(m_devRx, size, cyclic ? '\1' : '\0');
    } else {
        m_rxBuf = nullptr;
    }

    return m_rxBuf;
}

struct iio_buffer *DevicePlutoSDRBox::createTxBuffer(unsigned int size, bool cyclic)
{
    if (m_devTx) {
        m_txBuf =  iio_device_create_buffer(m_devTx, size, cyclic ? '\1' : '\0');
    } else {
        m_txBuf = nullptr;
    }

    return m_txBuf;
}

void DevicePlutoSDRBox::deleteRxBuffer()
{
    if (m_rxBuf)
    {
        iio_buffer_destroy(m_rxBuf);
        m_rxBuf = nullptr;
    }
}

void DevicePlutoSDRBox::deleteTxBuffer()
{
    if (m_txBuf)
    {
        iio_buffer_destroy(m_txBuf);
        m_txBuf = nullptr;
    }
}

ssize_t DevicePlutoSDRBox::getRxSampleSize()
{
    if (m_devRx) {
        return iio_device_get_sample_size(m_devRx);
    } else {
        return 0;
    }
}

ssize_t DevicePlutoSDRBox::getTxSampleSize()
{
    if (m_devTx) {
        return iio_device_get_sample_size(m_devTx);
    } else {
        return 0;
    }
}

ssize_t DevicePlutoSDRBox::rxBufferRefill()
{
    if (m_rxBuf) {
        return iio_buffer_refill(m_rxBuf);
    } else {
        return 0;
    }
}

ssize_t DevicePlutoSDRBox::txBufferPush()
{
    if (m_txBuf) {
        return iio_buffer_push(m_txBuf);
    } else {
        return 0;
    }
}

std::ptrdiff_t DevicePlutoSDRBox::rxBufferStep()
{
    if (m_rxBuf) {
        return iio_buffer_step(m_rxBuf);
    } else {
        return 0;
    }
}

char* DevicePlutoSDRBox::rxBufferEnd()
{
    if (m_rxBuf) {
        return (char *) iio_buffer_end(m_rxBuf);
    } else {
        return nullptr;
    }
}

char* DevicePlutoSDRBox::rxBufferFirst()
{
    if (m_rxBuf) {
        return (char *) iio_buffer_first(m_rxBuf, m_rxChannels.at(0));
    } else {
        return nullptr;
    }
}

std::ptrdiff_t DevicePlutoSDRBox::txBufferStep()
{
    if (m_txBuf) {
        return iio_buffer_step(m_txBuf);
    } else {
        return 0;
    }
}

char* DevicePlutoSDRBox::txBufferEnd()
{
    if (m_txBuf) {
        return (char *) iio_buffer_end(m_txBuf);
    } else {
        return nullptr;
    }
}

char* DevicePlutoSDRBox::txBufferFirst()
{
    if (m_txBuf) {
        return (char *) iio_buffer_first(m_txBuf, m_txChannels.at(0));
    } else {
        return nullptr;
    }
}

void DevicePlutoSDRBox::txChannelConvert(int16_t *dst, int16_t *src)
{
    if (m_txChannels.size() > 0) {
        iio_channel_convert_inverse(m_txChannels.at(0), &dst[0], &src[0]);
    }
    if (m_txChannels.size() > 1) {
        iio_channel_convert_inverse(m_txChannels.at(1), &dst[1], &src[1]);
    }
}

void DevicePlutoSDRBox::txChannelConvert(int chanIndex, int16_t *dst, int16_t *src)
{
    if (m_txChannels.size() > 2*chanIndex) {
        iio_channel_convert_inverse(m_txChannels.at(2*chanIndex), &dst[0], &src[0]);
    }
    if (m_txChannels.size() > 2*chanIndex+1) {
        iio_channel_convert_inverse(m_txChannels.at(2*chanIndex+1), &dst[1], &src[1]);
    }
}

bool DevicePlutoSDRBox::getRxSampleRates(SampleRates& sampleRates)
{
    std::string srStr;

    if (get_param(DEVICE_PHY, "rx_path_rates", srStr)) {
        qDebug("DevicePlutoSDRBox::getRxSampleRates: %s", srStr.c_str());
        return parseSampleRates(srStr, sampleRates);
    } else {
        return false;
    }
}

bool DevicePlutoSDRBox::getTxSampleRates(SampleRates& sampleRates)
{
    std::string srStr;

    if (get_param(DEVICE_PHY, "tx_path_rates", srStr)) {
        return parseSampleRates(srStr, sampleRates);
    } else {
        return false;
    }
}

bool DevicePlutoSDRBox::parseSampleRates(const std::string& rateStr, SampleRates& sampleRates)
{
    // Rx: "BBPLL:983040000 ADC:245760000 R2:122880000 R1:61440000 RF:30720000 RXSAMP:30720000"
    // Tx: "BBPLL:983040000 DAC:122880000 T2:122880000 T1:61440000 TF:30720000 TXSAMP:30720000"
    std::regex desc_regex("BBPLL:(.+) ..C:(.+) .2:(.+) .1:(.+) .F:(.+) .XSAMP:(.+)");
    std::smatch desc_match;
    std::regex_search(rateStr, desc_match, desc_regex);
    std::string valueStr;

    if (desc_match.size() == 7)
    {
        try
        {
            sampleRates.m_bbRateHz = boost::lexical_cast<uint32_t>(desc_match[1]);
            sampleRates.m_addaConnvRate = boost::lexical_cast<uint32_t>(desc_match[2]);
            sampleRates.m_hb3Rate = boost::lexical_cast<uint32_t>(desc_match[3]);
            sampleRates.m_hb2Rate = boost::lexical_cast<uint32_t>(desc_match[4]);
            sampleRates.m_hb1Rate = boost::lexical_cast<uint32_t>(desc_match[5]);
            sampleRates.m_firRate = boost::lexical_cast<uint32_t>(desc_match[6]);
            return true;
        }
        catch (const boost::bad_lexical_cast &e)
        {
            qWarning("DevicePlutoSDRBox::parseSampleRates: bad conversion to numeric");
            return false;
        }
    }
    else
    {
        return false;
    }
}

void DevicePlutoSDRBox::setSampleRate(uint32_t sampleRate)
{
    char buff[100];
    std::vector<std::string> params;
    snprintf(buff, sizeof(buff), "in_voltage_sampling_frequency=%d", sampleRate);
    params.push_back(std::string(buff));
    snprintf(buff, sizeof(buff), "out_voltage_sampling_frequency=%d", sampleRate);
    params.push_back(std::string(buff));
    set_params(DEVICE_PHY, params);
    m_devSampleRate = sampleRate;
}

/**
 * @param sampleRate baseband sample rate (S/s)
 * @param log2IntDec FIR interpolation or decimation factor
 * @param use Rx or Tx. Applies to the rest of the parameters
 * @param bw FIR filter bandwidth at approximately -6 dB cutoff (Hz)
 * @param gain FIR filter gain (dB)
 */
void DevicePlutoSDRBox::setFIR(uint32_t sampleRate, uint32_t log2IntDec, DeviceUse use, uint32_t bw, int gain)
{
    SampleRates sampleRates;
    std::ostringstream ostr;

    uint32_t nbTaps;
    double normalizedBW;
    uint32_t intdec = 1<<(log2IntDec > 2 ? 2 : log2IntDec);

    // update gain parameter

    if (use == USE_RX) {
        m_lpfFIRRxGain = gain;
    } else {
        m_lpfFIRTxGain = gain;
    }

    // set a dummy minimal filter first to get the sample rates right

    setFIREnable(false); // disable first

    formatFIRHeader(ostr, intdec);
    formatFIRCoefficients(ostr, 16, 0.5);
    setFilter(ostr.str());
    ostr.str(""); // reset string stream

    setFIREnable(true);        // re-enable
    setSampleRate(sampleRate); // set to new sample rate

    if (!getRxSampleRates(sampleRates)) {
        return;
    }

    setFIREnable(false); // disable again

    uint32_t nbGroups = sampleRates.m_addaConnvRate / 16;
    nbTaps = nbGroups*8 > 128 ? 128 : nbGroups*8;
    nbTaps = intdec == 1 ? (nbTaps > 64 ? 64 : nbTaps) : nbTaps;
    normalizedBW = ((float) bw) / sampleRates.m_hb1Rate;
    normalizedBW = normalizedBW < DevicePlutoSDR::firBWLowLimitFactor ?
            DevicePlutoSDR::firBWLowLimitFactor :
            normalizedBW > DevicePlutoSDR::firBWHighLimitFactor ? DevicePlutoSDR::firBWHighLimitFactor : normalizedBW;

    qDebug("DevicePlutoSDRBox::setFIR: intdec: %u gain: %d nbTaps: %u BWin: %u BW: %f (%f)",
            intdec,
            gain,
            nbTaps,
            bw,
            normalizedBW*sampleRates.m_hb1Rate,
            normalizedBW);

    // set the right filter

    formatFIRHeader(ostr, intdec);
    formatFIRCoefficients(ostr, nbTaps, normalizedBW);
    setFilter(ostr.str());

    m_lpfFIRlog2Decim = log2IntDec;
    m_lpfFIRBW = bw;

    // enable and set sample rate will be done by the caller
}

void DevicePlutoSDRBox::setFIREnable(bool enable)
{
    char buff[100];
    std::vector<std::string> params;
    snprintf(buff, sizeof(buff), "in_out_voltage_filter_fir_en=%d", enable ? 1 : 0);
    params.push_back(std::string(buff));
    set_params(DEVICE_PHY, params);
    m_lpfFIREnable = enable;
}

void DevicePlutoSDRBox::setLOPPMTenths(int ppmTenths)
{
    char buff[100];
    std::vector<std::string> params;
    int64_t newXO = m_xoInitial + ((m_xoInitial*ppmTenths) / 10000000L);
    snprintf(buff, sizeof(buff), "xo_correction=%ld", (long int) newXO);
    params.push_back(std::string(buff));
    set_params(DEVICE_PHY, params);
    m_LOppmTenths = ppmTenths;
}

void DevicePlutoSDRBox::formatFIRHeader(std::ostringstream& ostr,uint32_t intdec)
{
    ostr << "RX 3 GAIN " << m_lpfFIRRxGain << " DEC " << intdec << std::endl;
    ostr << "TX 3 GAIN " << m_lpfFIRTxGain << " INT " << intdec << std::endl;
}

void DevicePlutoSDRBox::formatFIRCoefficients(std::ostringstream& ostr, uint32_t nbTaps, double normalizedBW)
{
    double *fcoeffs = new double[nbTaps];
    WFIR::BasicFIR(fcoeffs, nbTaps, WFIR::LPF, normalizedBW, 0.0, normalizedBW < 0.2 ? WFIR::wtHAMMING : WFIR::wtBLACKMAN_HARRIS, 0.0);

    for (unsigned int i = 0; i < nbTaps; i++) {
        ostr << (int16_t) (fcoeffs[i] * 32768.0f) << ", " <<  (int16_t) (fcoeffs[i] * 32768.0f) << std::endl;
    }

    delete[] fcoeffs;
}

void DevicePlutoSDRBox::getXO()
{
    std::string valueStr;
    get_param(DEVICE_PHY, "xo_correction", valueStr);
    try
    {
        m_xoInitial = boost::lexical_cast<quint64>(valueStr);
        qDebug("DevicePlutoSDRBox::getXO: %ld", m_xoInitial);
    }
    catch (const boost::bad_lexical_cast &e)
    {
        qWarning("DevicePlutoSDRBox::getXO: cannot get initial XO correction");
    }
}

bool DevicePlutoSDRBox::getRxGain(int& gaindB, unsigned int chan)
{
    chan = chan % 2;
    char buff[30];
    snprintf(buff, sizeof(buff), "in_voltage%d_hardwaregain", chan);
    std::string gainStr;
    get_param(DEVICE_PHY, buff, gainStr);

    std::regex gain_regex("(.+)\\.(.+) dB");
    std::smatch gain_match;
    std::regex_search(gainStr, gain_match, gain_regex);

    if (gain_match.size() == 3)
    {
        try
        {
            gaindB = boost::lexical_cast<int>(gain_match[1]);
            return true;
        }
        catch (const boost::bad_lexical_cast &e)
        {
            qWarning("DevicePlutoSDRBox::getRxGain: bad conversion to numeric");
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool DevicePlutoSDRBox::getRxRSSI(std::string& rssiStr, unsigned int chan)
{
    chan = chan % 2;
    char buff[20];
    snprintf(buff, sizeof(buff), "in_voltage%d_rssi", chan);
    return get_param(DEVICE_PHY, buff, rssiStr);
}

bool DevicePlutoSDRBox::getTxRSSI(std::string& rssiStr, unsigned int chan)
{
    chan = chan % 2;
    char buff[20];
    snprintf(buff, sizeof(buff), "out_voltage%d_rssi", chan);
    return get_param(DEVICE_PHY, buff, rssiStr);
}

void DevicePlutoSDRBox::getRxLORange(uint64_t& minLimit, uint64_t& maxLimit)
{
    // values are returned in Hz
    qint64 stepLimit;
    std::string rangeStr;

    char buff[50];
    snprintf(buff, sizeof(buff), "out_altvoltage0_RX_LO_frequency_available");

    if (get_param(DEVICE_PHY, buff, rangeStr))
    {
        std::istringstream instream(rangeStr.substr(1, rangeStr.size() - 2));
	    instream >> minLimit >> stepLimit >> maxLimit;
    }
    else
    {
        minLimit = DevicePlutoSDR::rxLOLowLimitFreq;
	    maxLimit = DevicePlutoSDR::rxLOHighLimitFreq;
    }
}

void DevicePlutoSDRBox::getTxLORange(uint64_t& minLimit, uint64_t& maxLimit)
{
    // values are returned in Hz
    qint64 stepLimit;
    std::string rangeStr;

    char buff[50];
    snprintf(buff, sizeof(buff), "out_altvoltage1_TX_LO_frequency_available");

    if (get_param(DEVICE_PHY, buff, rangeStr))
    {
        std::istringstream instream(rangeStr.substr(1, rangeStr.size() - 2));
	    instream >> minLimit >> stepLimit >> maxLimit;
    }
    else
    {
        minLimit = DevicePlutoSDR::txLOLowLimitFreq;
	    maxLimit = DevicePlutoSDR::txLOHighLimitFreq;
    }
}

void DevicePlutoSDRBox::getbbLPRxRange(uint32_t& minLimit, uint32_t& maxLimit)
{
    // values are returned in Hz of RF (complex channel) bandwidth
    qint32 stepLimit;
    std::string rangeStr;

    char buff[50];
    snprintf(buff, sizeof(buff), "in_voltage_rf_bandwidth_available");

    if (get_param(DEVICE_PHY, buff, rangeStr))
    {
	std::istringstream instream(rangeStr.substr(1, rangeStr.size() - 2));
	instream >> minLimit >> stepLimit >> maxLimit;
    }
    else
    {
	minLimit = DevicePlutoSDR::bbLPRxLowLimitFreq;
	maxLimit = DevicePlutoSDR::bbLPRxHighLimitFreq;
    }
}

void DevicePlutoSDRBox::getbbLPTxRange(uint32_t& minLimit, uint32_t& maxLimit)
{
    // values are returned in Hz
    qint32 stepLimit;
    std::string rangeStr;

    char buff[50];
    snprintf(buff, sizeof(buff), "out_voltage_rf_bandwidth_available");

    if (get_param(DEVICE_PHY, buff, rangeStr))
    {
        std::istringstream instream(rangeStr.substr(1, rangeStr.size() - 2));
        instream >> minLimit >> stepLimit >> maxLimit;
    }
    else
    {
	minLimit = DevicePlutoSDR::bbLPTxLowLimitFreq;
	maxLimit = DevicePlutoSDR::bbLPTxHighLimitFreq;
    }
}


bool DevicePlutoSDRBox::fetchTemp()
{
    std::string temp_mC_str;

    if (get_param(DEVICE_PHY, "in_temp0_input", temp_mC_str))
    {
        try
        {
            uint32_t temp_mC = boost::lexical_cast<uint32_t>(temp_mC_str);
            m_temp = temp_mC / 1000.0;
            return true;
        }
        catch (const boost::bad_lexical_cast &e)
        {
            std::cerr << "PlutoSDRDevice::getTemp: bad conversion to numeric" << std::endl;
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool DevicePlutoSDRBox::getRateGovernors(std::string& rateGovernors)
{
    return get_param(DEVICE_PHY, "trx_rate_governor", rateGovernors);
}
