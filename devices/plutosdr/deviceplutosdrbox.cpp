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

#include <iostream>
#include <cstdio>
#include <cstring>
#include <regex>
#include <iio.h>
#include <boost/lexical_cast.hpp>
#include <QtGlobal>

#include "dsp/wfir.h"
#include "deviceplutosdrbox.h"

DevicePlutoSDRBox::DevicePlutoSDRBox(const std::string& uri) :
        m_chnRx0(0),
        m_chnTx0(0),
        m_rxBuf(0),
        m_txBuf(0)
{
    m_ctx = iio_create_context_from_uri(uri.c_str());
    m_devPhy = iio_context_find_device(m_ctx, "ad9361-phy");
    m_devRx = iio_context_find_device(m_ctx, "cf-ad9361-lpc");
    m_devTx = iio_context_find_device(m_ctx, "cf-ad9361-dds-core-lpc");
    m_valid = m_ctx && m_devPhy && m_devRx && m_devTx;
}

DevicePlutoSDRBox::~DevicePlutoSDRBox()
{
    deleteRxBuffer();
    deleteTxBuffer();
    closeRx();
    closeTx();
    if (m_ctx) { iio_context_destroy(m_ctx); }
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

        pos = it->find('=');

        if (pos == std::string::npos)
        {
            std::cerr << "DevicePlutoSDRBox::set_params: Misformed line: " << *it << std::endl;
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
        } else if (iio_device_find_attr(dev, attr)) {
            ret = iio_device_attr_write(dev, attr, val.c_str());
        } else {
            ret = iio_device_debug_attr_write(dev, attr, val.c_str());
        }

        if (ret < 0)
        {
            std::cerr << "DevicePlutoSDRBox::set_params: Unable to write attribute " << key <<  ": " << ret << std::endl;
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

void DevicePlutoSDRBox::set_filter(const std::string &filterConfigStr)
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
    if (!m_chnRx0) {
        m_chnRx0 = iio_device_find_channel(m_devRx, "voltage0", false);
    }

    if (m_chnRx0) {
        iio_channel_enable(m_chnRx0);
        return true;
    } else {
        std::cerr << "DevicePlutoSDRBox::openRx: failed" << std::endl;
        return false;
    }
}

bool DevicePlutoSDRBox::openTx()
{
    if (!m_chnTx0) {
        m_chnTx0 = iio_device_find_channel(m_devTx, "voltage0", true);
    }

    if (m_chnTx0) {
        iio_channel_enable(m_chnTx0);
        return true;
    } else {
        std::cerr << "DevicePlutoSDRBox::openTx: failed" << std::endl;
        return false;
    }
}

void DevicePlutoSDRBox::closeRx()
{
    if (m_chnRx0) { iio_channel_disable(m_chnRx0); }
}

void DevicePlutoSDRBox::closeTx()
{
    if (m_chnTx0) { iio_channel_disable(m_chnTx0); }
}

struct iio_buffer *DevicePlutoSDRBox::createRxBuffer(unsigned int size, bool cyclic)
{
    if (m_devRx) {
        m_rxBuf = iio_device_create_buffer(m_devRx, size, cyclic ? '\1' : '\0');
    } else {
        m_rxBuf = 0;
    }

    return m_rxBuf;
}

struct iio_buffer *DevicePlutoSDRBox::createTxBuffer(unsigned int size, bool cyclic)
{
    if (m_devTx) {
        m_txBuf =  iio_device_create_buffer(m_devTx, size, cyclic ? '\1' : '\0');
    } else {
        m_txBuf = 0;
    }

    return m_txBuf;
}

void DevicePlutoSDRBox::deleteRxBuffer()
{
    if (m_rxBuf) {
        iio_buffer_destroy(m_rxBuf);
        m_rxBuf = 0;
    }
}

void DevicePlutoSDRBox::deleteTxBuffer()
{
    if (m_txBuf) {
        iio_buffer_destroy(m_txBuf);
        m_txBuf = 0;
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
        return 0;
    }
}

char* DevicePlutoSDRBox::rxBufferFirst()
{
    if (m_rxBuf) {
        return (char *) iio_buffer_first(m_rxBuf, m_chnRx0);
    } else {
        return 0;
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
        return 0;
    }
}

char* DevicePlutoSDRBox::txBufferFirst()
{
    if (m_txBuf) {
        return (char *) iio_buffer_first(m_txBuf, m_chnTx0);
    } else {
        return 0;
    }
}

bool DevicePlutoSDRBox::getRxSampleRates(SampleRates& sampleRates)
{
    std::string srStr;

    if (get_param(DEVICE_PHY, "rx_path_rates", srStr)) {
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
            sampleRates.m_bbRate = boost::lexical_cast<uint32_t>(desc_match[1]) + 1;
            sampleRates.m_addaConnvRate = boost::lexical_cast<uint32_t>(desc_match[2]) + 1;
            sampleRates.m_hb3Rate = boost::lexical_cast<uint32_t>(desc_match[3]) + 1;
            sampleRates.m_hb2Rate = boost::lexical_cast<uint32_t>(desc_match[4]) + 1;
            sampleRates.m_hb1Rate = boost::lexical_cast<uint32_t>(desc_match[5]) + 1;
            sampleRates.m_firRate = boost::lexical_cast<uint32_t>(desc_match[6]) + 1;
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

void DevicePlutoSDRBox::setFIR(DeviceUse use, uint32_t intdec, uint32_t bw, int gain)
{
    SampleRates sampleRates;
    std::ostringstream ostr;

    uint32_t nbTaps;
    double normalizedBW;

    // set a dummy minimal filter first to get the sample rates right

    formatFIRHeader(ostr, use, intdec, gain);
    formatFIRCoefficients(ostr, 16, 0.5);
    setFilter(ostr.str());
    ostr.str(""); // reset string stream

    if (use == USE_RX)
    {
        if (!getRxSampleRates(sampleRates)) {
            return;
        }

        uint32_t nbGroups = sampleRates.m_addaConnvRate / 16;
        nbTaps = nbGroups*8 > 128 ? 128 : nbGroups*8;
        normalizedBW = ((float) bw) / sampleRates.m_hb1Rate;
        normalizedBW = normalizedBW < 0.1 ? 0.1 : normalizedBW > 0.9 ? 0.9 : normalizedBW;
    }
    else
    {
        if (!getTxSampleRates(sampleRates)) {
            return;
        }

        uint32_t nbGroups = sampleRates.m_addaConnvRate / 16;
        nbTaps = nbGroups*8 > 128 ? 128 : nbGroups*8;
        nbTaps = intdec == 1 ? (nbTaps > 64 ? 64 : nbTaps) : nbTaps;
        normalizedBW = ((float) bw) / sampleRates.m_hb1Rate;
        normalizedBW = normalizedBW < 0.1 ? 0.1 : normalizedBW > 0.9 ? 0.9 : normalizedBW;
    }

    // set the right filter

    formatFIRHeader(ostr, use, intdec, gain);
    formatFIRCoefficients(ostr, nbTaps, normalizedBW);
    setFilter(ostr.str());
}

void DevicePlutoSDRBox::formatFIRHeader(std::ostringstream& ostr, DeviceUse use, uint32_t intdec, int32_t gain)
{
    ostr << use == USE_RX ? "RX 1" : "TX 1" << " GAIN " << gain << " DEC " << intdec << std::endl;
}

void DevicePlutoSDRBox::formatFIRCoefficients(std::ostringstream& ostr, uint32_t nbTaps, double normalizedBW)
{
    double fcoeffs = new double[nbTaps];
    WFIR::BasicFIR(fcoeffs, nbTaps, WFIR::LPF, 0.0, normalizedBW, WFIR::wtBLACKMAN_HARRIS, 0.0);

    for (int i = 0; i < nbTaps; i++) {
        ostr << (uint16_t) (fcoeffs[i] * 32768.0) << std::endl;
    }
}


