///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Benjamin Menkuec, DJ4LF                                    //
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
#include "devicejupitersdr.h"
#include "devicejupitersdrbox.h"

DeviceJupiterSDRBox::DeviceJupiterSDRBox(const std::string& uri) :
        m_devSampleRate(0),
        m_LOppmTenths(0),
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
        m_devPhy = iio_context_find_device(m_ctx, "adrv9002-phy");
        m_devRx = iio_context_find_device(m_ctx, "axi-adrv9002-rx-lpc");
        // TODO: what about axi-adrv9002-rx2-lpc ?
        m_devTx = iio_context_find_device(m_ctx, "axi-adrv9002-tx-lpc");
        // TODO: what about axi-adrv9002-tx2-lpc ?
    }
    else
    {
        qCritical("DeviceJupiterSDRBox::DeviceJupiterSDRBox: cannot create context for uri: %s", uri.c_str());
    }

    m_valid = m_ctx && m_devPhy && m_devRx && m_devTx;

    if (m_valid)
    {
        std::regex channelIdReg("voltage([0-9]+)_[iq]");

        // getXO(); // this is not supported by jupiterSDR
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
                qDebug("DeviceJupiterSDRBox::DeviceJupiterSDRBox: Rx: %s #Attrs: %d", channelId.c_str(), nbAttributes);
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
                qDebug("DeviceJupiterSDRBox::DeviceJupiterSDRBox: Tx: %s #Attrs: %d", channelId.c_str(), nbAttributes);
            }
        }
    }
}

DeviceJupiterSDRBox::~DeviceJupiterSDRBox()
{
    deleteRxBuffer();
    deleteTxBuffer();
    closeRx();
    closeTx();
    if (m_ctx) { iio_context_destroy(m_ctx); }
}

bool DeviceJupiterSDRBox::probeURI(const std::string& uri)
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

void DeviceJupiterSDRBox::set_params(DeviceType devType,
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
            std::cerr << "DeviceJupiterSDRBox::set_params: Malformed line: " << *it << std::endl;
            continue;
        }

        std::string key = it->substr(0, pos);
        std::string val = it->substr(pos + 1, std::string::npos);

        ret = iio_device_identify_filename(dev, key.c_str(), &chn, &attr);

        if (ret)
        {
            std::cerr << "DeviceJupiterSDRBox::set_params: Parameter not recognized: " << key << std::endl;
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
            std::cerr << "DeviceJupiterSDRBox::set_params: Unable to write " << item << " attribute " << key << "=" << val << ": " << errstr << " (" << ret << ") " << std::endl;
        }
        else
        {
            std::cerr << "DeviceJupiterSDRBox::set_params: set attribute " << key << "=" << val << ": " << ret << std::endl;
        }
    }
}

bool DeviceJupiterSDRBox::get_param(DeviceType devType, const std::string &param, std::string &value)
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
        std::cerr << "DeviceJupiterSDRBox::get_param: Parameter not recognized: " << param << std::endl;
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
        std::cerr << "DeviceJupiterSDRBox::get_param: Unable to read attribute " << param <<  ": " << nchars << std::endl;
        return false;
    }
    else
    {
        value.assign(valuestr);
        return true;
    }
}

bool DeviceJupiterSDRBox::openRx()
{
    if (!m_valid) { return false; }

    if (m_rxChannels.size() > 0)
    {
        iio_channel_enable(m_rxChannels.at(0));

        const struct iio_data_format *df = iio_channel_get_data_format(m_rxChannels.at(0));
        qDebug("DeviceJupiterSDRBox::openRx channel I: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
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
        qWarning("DeviceJupiterSDRBox::openRx: open channel I failed");
        return false;
    }

    if (m_rxChannels.size() > 1)
    {
        iio_channel_enable(m_rxChannels.at(1));

        const struct iio_data_format* df = iio_channel_get_data_format(m_rxChannels.at(1));
        qDebug("DeviceJupiterSDRBox::openRx channel Q: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
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
        qWarning("DeviceJupiterSDRBox::openRx: open channel Q failed");
        return false;
    }
}

bool DeviceJupiterSDRBox::openSecondRx()
{
    if (!m_valid) { return false; }

    if (m_rxChannels.size() > 2)
    {
        iio_channel_enable(m_rxChannels.at(2));

        const struct iio_data_format *df = iio_channel_get_data_format(m_rxChannels.at(2));
        qDebug("DeviceJupiterSDRBox::openSecondRx channel I: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
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
        qWarning("DeviceJupiterSDRBox::openSecondRx: open channel I failed");
        return false;
    }

    if (m_rxChannels.size() > 3)
    {
        iio_channel_enable(m_rxChannels.at(3));

        const struct iio_data_format* df = iio_channel_get_data_format(m_rxChannels.at(3));
        qDebug("DeviceJupiterSDRBox::openSecondRx channel Q: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
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
        qWarning("DeviceJupiterSDRBox::openSecondRx: open channel Q failed");
        return false;
    }
}

bool DeviceJupiterSDRBox::openTx()
{
    if (!m_valid) { return false; }

    if (m_txChannels.size() > 0)
    {
        iio_channel_enable(m_txChannels.at(0));
        const struct iio_data_format *df = iio_channel_get_data_format(m_txChannels.at(0));
        qDebug("DeviceJupiterSDRBox::openTx: channel I: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
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
        std::cerr << "DeviceJupiterSDRBox::openTx: failed to open I channel" << std::endl;
        return false;
    }

    if (m_txChannels.size() > 1)
    {
        iio_channel_enable(m_txChannels.at(1));
        const struct iio_data_format *df = iio_channel_get_data_format(m_txChannels.at(1));
        qDebug("DeviceJupiterSDRBox::openTx: channel Q: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
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
        std::cerr << "DeviceJupiterSDRBox::openTx: failed to open Q channel" << std::endl;
        return false;
    }
}

bool DeviceJupiterSDRBox::openSecondTx()
{
    if (!m_valid) { return false; }

    if (m_txChannels.size() > 2)
    {
        iio_channel_enable(m_txChannels.at(2));
        const struct iio_data_format *df = iio_channel_get_data_format(m_txChannels.at(2));
        qDebug("DeviceJupiterSDRBox::openSecondTx: channel I: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
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
        qWarning("DeviceJupiterSDRBox::openSecondTx: failed to open I channel");
        return false;
    }

    if (m_txChannels.size() > 3)
    {
        iio_channel_enable(m_txChannels.at(3));
        const struct iio_data_format *df = iio_channel_get_data_format(m_txChannels.at(3));
        qDebug("DeviceJupiterSDRBox::openSecondTx: channel Q: length: %u bits: %u shift: %u signed: %s be: %s with_scale: %s scale: %lf repeat: %u",
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
        qWarning("DeviceJupiterSDRBox::openSecondTx: failed to open Q channel");
        return false;
    }
}

void DeviceJupiterSDRBox::closeRx()
{
    if (m_rxChannels.size() > 0) { iio_channel_disable(m_rxChannels.at(0)); }
    if (m_rxChannels.size() > 1) { iio_channel_disable(m_rxChannels.at(1)); }
}

void DeviceJupiterSDRBox::closeSecondRx()
{
    if (m_rxChannels.size() > 2) { iio_channel_disable(m_rxChannels.at(2)); }
    if (m_rxChannels.size() > 3) { iio_channel_disable(m_rxChannels.at(3)); }
}

void DeviceJupiterSDRBox::closeTx()
{
    if (m_txChannels.size() > 0) { iio_channel_disable(m_txChannels.at(0)); }
    if (m_txChannels.size() > 1) { iio_channel_disable(m_txChannels.at(1)); }
}

void DeviceJupiterSDRBox::closeSecondTx()
{
    if (m_txChannels.size() > 2) { iio_channel_disable(m_txChannels.at(2)); }
    if (m_txChannels.size() > 3) { iio_channel_disable(m_txChannels.at(3)); }
}

struct iio_buffer *DeviceJupiterSDRBox::createRxBuffer(unsigned int size, bool cyclic)
{
    if (m_devRx) {
        m_rxBuf = iio_device_create_buffer(m_devRx, size, cyclic ? '\1' : '\0');
    } else {
        m_rxBuf = nullptr;
    }

    return m_rxBuf;
}

struct iio_buffer *DeviceJupiterSDRBox::createTxBuffer(unsigned int size, bool cyclic)
{
    if (m_devTx) {
        m_txBuf =  iio_device_create_buffer(m_devTx, size, cyclic ? '\1' : '\0');
    } else {
        m_txBuf = nullptr;
    }

    return m_txBuf;
}

void DeviceJupiterSDRBox::deleteRxBuffer()
{
    if (m_rxBuf)
    {
        iio_buffer_destroy(m_rxBuf);
        m_rxBuf = nullptr;
    }
}

void DeviceJupiterSDRBox::deleteTxBuffer()
{
    if (m_txBuf)
    {
        iio_buffer_destroy(m_txBuf);
        m_txBuf = nullptr;
    }
}

ssize_t DeviceJupiterSDRBox::getRxSampleSize()
{
    if (m_devRx) {
        return iio_device_get_sample_size(m_devRx);
    } else {
        return 0;
    }
}

ssize_t DeviceJupiterSDRBox::getTxSampleSize()
{
    if (m_devTx) {
        return iio_device_get_sample_size(m_devTx);
    } else {
        return 0;
    }
}

ssize_t DeviceJupiterSDRBox::rxBufferRefill()
{
    if (m_rxBuf) {
        return iio_buffer_refill(m_rxBuf);
    } else {
        return 0;
    }
}

ssize_t DeviceJupiterSDRBox::txBufferPush()
{
    if (m_txBuf) {
        return iio_buffer_push(m_txBuf);
    } else {
        return 0;
    }
}

std::ptrdiff_t DeviceJupiterSDRBox::rxBufferStep()
{
    if (m_rxBuf) {
        return iio_buffer_step(m_rxBuf);
    } else {
        return 0;
    }
}

char* DeviceJupiterSDRBox::rxBufferEnd()
{
    if (m_rxBuf) {
        return (char *) iio_buffer_end(m_rxBuf);
    } else {
        return nullptr;
    }
}

char* DeviceJupiterSDRBox::rxBufferFirst()
{
    if (m_rxBuf) {
        return (char *) iio_buffer_first(m_rxBuf, m_rxChannels.at(0));
    } else {
        return nullptr;
    }
}

std::ptrdiff_t DeviceJupiterSDRBox::txBufferStep()
{
    if (m_txBuf) {
        return iio_buffer_step(m_txBuf);
    } else {
        return 0;
    }
}

char* DeviceJupiterSDRBox::txBufferEnd()
{
    if (m_txBuf) {
        return (char *) iio_buffer_end(m_txBuf);
    } else {
        return nullptr;
    }
}

char* DeviceJupiterSDRBox::txBufferFirst()
{
    if (m_txBuf) {
        return (char *) iio_buffer_first(m_txBuf, m_txChannels.at(0));
    } else {
        return nullptr;
    }
}

void DeviceJupiterSDRBox::txChannelConvert(int16_t *dst, int16_t *src)
{
    if (m_txChannels.size() > 0) {
        iio_channel_convert_inverse(m_txChannels.at(0), &dst[0], &src[0]);
    }
    if (m_txChannels.size() > 1) {
        iio_channel_convert_inverse(m_txChannels.at(1), &dst[1], &src[1]);
    }
}

void DeviceJupiterSDRBox::txChannelConvert(int chanIndex, int16_t *dst, int16_t *src)
{
    if (m_txChannels.size() > 2*chanIndex) {
        iio_channel_convert_inverse(m_txChannels.at(2*chanIndex), &dst[0], &src[0]);
    }
    if (m_txChannels.size() > 2*chanIndex+1) {
        iio_channel_convert_inverse(m_txChannels.at(2*chanIndex+1), &dst[1], &src[1]);
    }
}

bool DeviceJupiterSDRBox::getRxSampleRates(SampleRates& sampleRates)
{
    std::string srStr;

    if (get_param(DEVICE_PHY, "rx_path_rates", srStr)) {
        qDebug("DeviceJupiterSDRBox::getRxSampleRates: %s", srStr.c_str());
        return parseSampleRates(srStr, sampleRates);
    } else {
        return false;
    }
}

bool DeviceJupiterSDRBox::getTxSampleRates(SampleRates& sampleRates)
{
    std::string srStr;

    if (get_param(DEVICE_PHY, "tx_path_rates", srStr)) {
        return parseSampleRates(srStr, sampleRates);
    } else {
        return false;
    }
}

bool DeviceJupiterSDRBox::parseSampleRates(const std::string& rateStr, SampleRates& sampleRates)
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
            return true;
        }
        catch (const boost::bad_lexical_cast &e)
        {
            qWarning("DeviceJupiterSDRBox::parseSampleRates: bad conversion to numeric");
            return false;
        }
    }
    else
    {
        return false;
    }
}

void DeviceJupiterSDRBox::setSampleRate(uint32_t sampleRate)
{
    char buff[100];
    std::vector<std::string> params;
    snprintf(buff, sizeof(buff), "in_voltage0_sampling_frequency=%d", sampleRate);
    params.push_back(std::string(buff));
    snprintf(buff, sizeof(buff), "out_voltage0_sampling_frequency=%d", sampleRate);
    params.push_back(std::string(buff));
    set_params(DEVICE_PHY, params);
    m_devSampleRate = sampleRate;
}


void DeviceJupiterSDRBox::setLOPPMTenths(int ppmTenths)
{
    // char buff[100];
    // std::vector<std::string> params;
    // int64_t newXO = m_xoInitial + ((m_xoInitial*ppmTenths) / 10000000L);
    // snprintf(buff, sizeof(buff), "xo_correction=%ld", (long int) newXO);
    // params.push_back(std::string(buff));
    // set_params(DEVICE_PHY, params);
    // m_LOppmTenths = ppmTenths;
}

void DeviceJupiterSDRBox::getXO()
{
    // std::string valueStr;
    // get_param(DEVICE_PHY, "xo_correction", valueStr);
    // try
    // {
    //     m_xoInitial = boost::lexical_cast<quint64>(valueStr);
    //     qDebug("DeviceJupiterSDRBox::getXO: %ld", m_xoInitial);
    // }
    // catch (const boost::bad_lexical_cast &e)
    // {
    //     qWarning("DeviceJupiterSDRBox::getXO: cannot get initial XO correction");
    // }
}

bool DeviceJupiterSDRBox::getRxGain(int& gaindB, unsigned int chan)
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
            qWarning("DeviceJupiterSDRBox::getRxGain: bad conversion to numeric");
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool DeviceJupiterSDRBox::getRxRSSI(std::string& rssiStr, unsigned int chan)
{
    chan = chan % 2;
    char buff[20];
    snprintf(buff, sizeof(buff), "in_voltage%d_rssi", chan);
    return get_param(DEVICE_PHY, buff, rssiStr);
}

bool DeviceJupiterSDRBox::getTxRSSI(std::string& rssiStr, unsigned int chan)
{
    chan = chan % 2;
    char buff[20];
    snprintf(buff, sizeof(buff), "out_voltage%d_rssi", chan);
    return get_param(DEVICE_PHY, buff, rssiStr);
}

void DeviceJupiterSDRBox::getRxLORange(uint64_t& minLimit, uint64_t& maxLimit)
{
    minLimit = DeviceJupiterSDR::rxLOLowLimitFreq;
    maxLimit = DeviceJupiterSDR::rxLOHighLimitFreq;
}

void DeviceJupiterSDRBox::getTxLORange(uint64_t& minLimit, uint64_t& maxLimit)
{
    minLimit = DeviceJupiterSDR::txLOLowLimitFreq;
    maxLimit = DeviceJupiterSDR::txLOHighLimitFreq;
}

void DeviceJupiterSDRBox::getbbLPRxRange(uint32_t& minLimit, uint32_t& maxLimit)
{
	minLimit = DeviceJupiterSDR::bbLPRxLowLimitFreq;
	maxLimit = DeviceJupiterSDR::bbLPRxHighLimitFreq;
}

void DeviceJupiterSDRBox::getbbLPTxRange(uint32_t& minLimit, uint32_t& maxLimit)
{
	minLimit = DeviceJupiterSDR::bbLPTxLowLimitFreq;
	maxLimit = DeviceJupiterSDR::bbLPTxHighLimitFreq;
}


bool DeviceJupiterSDRBox::fetchTemp()
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
            std::cerr << "JupiterSDRDevice::getTemp: bad conversion to numeric" << std::endl;
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool DeviceJupiterSDRBox::getRateGovernors(std::string& rateGovernors)
{
    return get_param(DEVICE_PHY, "trx_rate_governor", rateGovernors);
}
