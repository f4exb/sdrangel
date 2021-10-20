///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#define BOOST_CHRONO_HEADER_ONLY
#include <boost/chrono/chrono.hpp>

//#include <time.h>

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QUdpSocket>
#include <QHostAddress>
#include <QNetworkDatagram>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "util/db.h"
#include "util/messagequeue.h"

#include "datvmodreport.h"
#include "datvmodsource.h"

#ifdef _WIN32
#include <winsock.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

const int DATVModSource::m_levelNbSamples = 10000; // every 10ms

// Get transport stream bitrate from file
int DATVModSource::getTSBitrate(const QString& filename)
{
    AVFormatContext *fmtCtx = nullptr;
    QByteArray ba = filename.toLocal8Bit();
    const char *filenameChars = ba.data();

    if (avformat_open_input(&fmtCtx, filenameChars, nullptr, nullptr) < 0)
    {
        qCritical() << "DATVModSource: Could not open source file " << filename;
        return -1;
    }
    if (avformat_find_stream_info(fmtCtx, nullptr) < 0)
    {
        qCritical() << "DATVModSource: Could not find stream information for " << filename;
        avformat_close_input(&fmtCtx);
        return -1;
    }

    int bitrate = fmtCtx->bit_rate;

    avformat_close_input(&fmtCtx);

    return bitrate;
}

// Get data bitrate (i.e. excluding FEC overhead)
int DATVModSource::getDVBSDataBitrate(const DATVModSettings& settings) const
{
    float fecFactor;
    float plFactor;
    float bitsPerSymbol;

    switch (settings.m_modulation)
    {
    case DATVModSettings::BPSK:
        bitsPerSymbol = 1.0f;
        break;
    case DATVModSettings::QPSK:
        bitsPerSymbol = 2.0f;
        break;
    case DATVModSettings::PSK8:
        bitsPerSymbol = 3.0f;
        break;
    case DATVModSettings::APSK16:
        bitsPerSymbol = 4.0f;
        break;
    case DATVModSettings::APSK32:
        bitsPerSymbol = 5.0f;
        break;
    }

    if (settings.m_standard == DATVModSettings::DVB_S)
    {
        float rsFactor;
        float convFactor;

        rsFactor = DVBS::tsPacketLen/(float)DVBS::rsPacketLen;
        switch (settings.m_fec)
        {
        case DATVModSettings::FEC12:
            convFactor = 1.0f/2.0f;
            break;
        case DATVModSettings::FEC23:
            convFactor = 2.0f/3.0f;
            break;
        case DATVModSettings::FEC34:
            convFactor = 3.0f/4.0f;
            break;
        case DATVModSettings::FEC56:
            convFactor = 5.0f/6.0f;
            break;
        case DATVModSettings::FEC78:
            convFactor = 7.0f/8.0f;
            break;
        case DATVModSettings::FEC45:
            convFactor = 4.0f/5.0f;
            break;
        case DATVModSettings::FEC89:
            convFactor = 8.0f/9.0f;
            break;
        case DATVModSettings::FEC910:
            convFactor = 9.0f/10.0f;
            break;
        case DATVModSettings::FEC14:
            convFactor = 1.0f/4.0f;
            break;
        case DATVModSettings::FEC13:
            convFactor = 1.0f/3.0f;
            break;
        case DATVModSettings::FEC25:
            convFactor = 2.0f/5.0f;
            break;
        case DATVModSettings::FEC35:
            convFactor = 3.0f/5.0f;
            break;
        }
        fecFactor = rsFactor * convFactor;
        plFactor = 1.0f;
    }
    else
    {
        // For normal frames
        int codedBlockSize = 64800;
        int uncodedBlockSize;
        int bbHeaderBits = 80;
        // See table 5a in DVBS2 spec
        switch (settings.m_fec)
        {
        case DATVModSettings::FEC12:
            uncodedBlockSize = 32208;
            break;
        case DATVModSettings::FEC23:
            uncodedBlockSize = 43040;
            break;
        case DATVModSettings::FEC34:
            uncodedBlockSize = 48408;
            break;
        case DATVModSettings::FEC56:
            uncodedBlockSize = 53840;
            break;
        case DATVModSettings::FEC45:
            uncodedBlockSize = 51648;
            break;
        case DATVModSettings::FEC89:
            uncodedBlockSize = 57472;
            break;
        case DATVModSettings::FEC910:
            uncodedBlockSize = 58192;
            break;
        case DATVModSettings::FEC14:
            uncodedBlockSize = 16008;
            break;
        case DATVModSettings::FEC13:
            uncodedBlockSize = 21408;
            break;
        case DATVModSettings::FEC25:
            uncodedBlockSize = 25728;
            break;
        case DATVModSettings::FEC35:
            uncodedBlockSize = 38688;
            break;
        default:
            qDebug() << "DATVModSource::getDVBSDataBitrate: Unsupported DVB-S2 code rate";
            break;
        }
        fecFactor = (uncodedBlockSize-bbHeaderBits)/(float)codedBlockSize;
        float symbolsPerFrame = codedBlockSize/bitsPerSymbol;
        // 90 symbols for PL header
        plFactor = symbolsPerFrame / (symbolsPerFrame + 90.0f);
    }

    return std::round(settings.m_symbolRate * bitsPerSymbol * fecFactor * plFactor);
}

void DATVModSource::checkBitrates()
{
    int dataBitrate = getDVBSDataBitrate(m_settings);
    qDebug() << "MPEG-TS bitrate: " << m_mpegTSBitrate;
    qDebug() << "DVB data bitrate: " << dataBitrate;
    if (dataBitrate < m_mpegTSBitrate)
        qWarning() << "DVB data bitrate is lower than the bitrate of the MPEG transport stream";
    m_tsRatio = m_mpegTSBitrate/(float)dataBitrate;
}

DATVModSource::DATVModSource() :
    m_mpegTSBitrate(0),
    m_mpegTSSize(0),
    m_sampleIdx(0),
    m_frameIdx(0),
    m_frameCount(0),
    m_tsRatio(0.0f),
    m_symbolCount(0),
    m_symbolSel(0),
    m_symbolIdx(0),
    m_samplesPerSymbol(1),
    m_udpSocket(nullptr),
    m_udpByteCount(0),
    m_udpAbsByteCount(0),
    m_udpBufferIdx(0),
    m_udpBufferCount(0),
    m_udpMaxBufferUtilization(0),
    m_sampleRate(0),
    m_channelSampleRate(1000000),
    m_channelFrequencyOffset(0),
    m_tsFileOK(false),
    m_messageQueueToGUI(nullptr)
{
    m_interpolatorDistanceRemain = 0.0f;
    m_interpolatorDistance = 1.0f;

    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
    applySettings(m_settings, true);
}

DATVModSource::~DATVModSource()
{
}

void DATVModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void DATVModSource::prefetch(unsigned int nbSamples)
{
    (void) nbSamples;
}

void DATVModSource::pullOne(Sample& sample)
{
    if (m_settings.m_channelMute)
    {
        sample.m_real = 0.0f;
        sample.m_imag = 0.0f;
        return;
    }

    Complex ci;

    if (m_sampleRate == m_channelSampleRate) // no interpolation nor decimation
    {
        modulateSample();
        pullFinalize(m_modSample, sample);
    }
    else
    {
        if (m_interpolatorDistance > 1.0f) // decimate
        {
            modulateSample();

            while (!m_interpolator.decimate(&m_interpolatorDistanceRemain, m_modSample, &ci))
                modulateSample();
        }
        else
        {
            if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, m_modSample, &ci))
                modulateSample();
        }

        m_interpolatorDistanceRemain += m_interpolatorDistance;
        pullFinalize(ci, sample);
    }
}

void DATVModSource::pullFinalize(Complex& ci, Sample& sample)
{
    ci *= m_carrierNco.nextIQ(); // shift to carrier frequency

    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    m_movingAverage(magsq);

    sample.m_real = (FixReal) (ci.real() * SDR_TX_SCALEF);
    sample.m_imag = (FixReal) (ci.imag() * SDR_TX_SCALEF);
}

void DATVModSource::modulateSample()
{
    Real i, q;

    if (m_sampleIdx == 0)
    {
        while (m_symbolCount == 0)
        {
            bool tsFileReady = (m_settings.m_source == DATVModSettings::SourceFile)
                && m_settings.m_tsFilePlay
                && m_tsFileOK
                && !m_mpegTSStream.eof();

            if (tsFileReady
                && (m_frameIdx/(m_frameCount+1.0f) < m_tsRatio)  // Insert null packets if file rate is lower than DVB-S data rate
            )
            {
                // Read transport stream packet from file
                m_mpegTSStream.read((char *)m_mpegTS, sizeof(m_mpegTS));
                m_frameIdx++;
                m_frameCount++;
            }
            else if ((m_settings.m_source == DATVModSettings::SourceUDP)
                && (m_udpBufferIdx < m_udpBufferCount)
            )
            {
                // Copy transport stream packet from UDP buffer
                memcpy(m_mpegTS, &m_udpBuffer[m_udpBufferIdx*sizeof(m_mpegTS)], sizeof(m_mpegTS));
                m_udpBufferIdx++;
            }
            else if ((m_settings.m_source == DATVModSettings::SourceUDP)
                && ((m_udpSocket != nullptr) && m_udpSocket->hasPendingDatagrams())
            )
            {
                updateUDPBufferUtilization();

                // Get transport stream packets from UDP - buffer if more than one
                QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
                QByteArray ba = datagram.data();
                int size = ba.size();
                char *data = ba.data();

                if (size <= (int)sizeof(m_udpBuffer))
                {
                    memcpy(m_mpegTS, data, sizeof(m_mpegTS));

                    if (size >= (int)sizeof(m_mpegTS)) {
                        memcpy(&m_udpBuffer[0], &data[sizeof(m_mpegTS)], size - sizeof(m_mpegTS));
                    }

                    m_udpBufferIdx = 0;
                    m_udpBufferCount = (size / sizeof(m_mpegTS)) - 1;

                    if (size % sizeof(m_mpegTS) != 0) {
                        qWarning() << "DATVModSource::modulateSample: UDP packet size (" << size << ") is not a multiple of " << sizeof(m_mpegTS);
                    }
                }
                else
                {
                    qWarning() << "DATVModSource::modulateSample: UDP packet size (" << size << ") exceeds buffer size " << sizeof(m_udpBuffer) << ")";
                }

                m_udpByteCount += ba.size();
                m_udpAbsByteCount += ba.size();
            }
            else
            {
                // Insert null packet. PID=0x1fff
                memset(m_mpegTS, 0, sizeof(m_mpegTS));
                m_mpegTS[0] = 0x47; // Sync byte
                m_mpegTS[1] = 0x01;
                m_mpegTS[2] = 0xff;
                m_mpegTS[3] = 0xff;
                m_mpegTS[4] = 0x10;

                if (tsFileReady) {
                    m_frameCount++;
                }
                //qDebug() << "null " << tsFileReady << " " << (m_frameIdx/(m_frameCount+1.0f)) << " " << m_tsRatio;
            }

            if (m_settings.m_standard == DATVModSettings::DVB_S)
            {
                // Encode using DVB-S
                m_symbolCount = m_dvbs.encode(m_mpegTS, m_iqSymbols);
            }
            else
            {
                // Encode using DVB-S2
                m_symbolCount = m_dvbs2.s2_add_ts_frame((u8 *)m_mpegTS);
                m_plFrame = m_dvbs2.pl_get_frame();
            }

            // Loop file if we reach the end
            if ((m_settings.m_source == DATVModSettings::SourceFile)
                && (m_frameIdx*DVBS::tsPacketLen >= m_mpegTSSize)
                && m_settings.m_tsFilePlayLoop
            )
            {
                m_mpegTSStream.clear();
                m_mpegTSStream.seekg(0, std::ios::beg);
                m_frameIdx = 0;
                m_frameCount = 0;
            }

            m_symbolIdx = 0;
        }

        if (m_settings.m_modulation == DATVModSettings::BPSK)
        {
            // BPSK
            i = m_pulseShapeI.filter(m_iqSymbols[m_symbolIdx*2+m_symbolSel] ? -1.0f : 1.0f);
            q = 0.0f;

            if (m_symbolSel == 1)
            {
                m_symbolIdx++;
                m_symbolCount--;
                m_symbolSel = 0;
            }
            else
            {
                m_symbolSel++;
            }
        }
        else
        {
            // QPSK
            if (m_settings.m_standard == DATVModSettings::DVB_S)
            {
                // Does the 45-degree rotation matter?
                // Makes a little difference to amplitude of filter output, but we could scale that
                i = m_pulseShapeI.filter(m_iqSymbols[m_symbolIdx*2] ? -1.0f : 1.0f);
                q = m_pulseShapeQ.filter(m_iqSymbols[m_symbolIdx*2+1] ? -1.0f : 1.0f);
                /*
                int sym = (m_iqSymbols[m_symbolIdx*2] << 1) | m_iqSymbols[m_symbolIdx*2+1];
                if (sym == 0)
                {
                    i = m_pulseShapeI.filter(cos(M_PI/4));
                    q = m_pulseShapeQ.filter(sin(M_PI/4));
                }
                else if (sym == 1)
                {
                    i = m_pulseShapeI.filter(cos(7*M_PI/4));
                    q = m_pulseShapeQ.filter(sin(7*M_PI/4));
                }
                else if (sym == 2)
                {
                    i = m_pulseShapeI.filter(cos(3*M_PI/4));
                    q = m_pulseShapeQ.filter(sin(3*M_PI/4));
                }
                else if (sym == 3)
                {
                    i = m_pulseShapeI.filter(cos(5*M_PI/4));
                    q = m_pulseShapeQ.filter(sin(5*M_PI/4));
                }
                */
                m_symbolIdx++;
                m_symbolCount--;
            }
            else
            {
                // First 90 symbols of DVB-S2 are pi/2 BPSK, then remaining symbols are in specified modulation
                i = m_pulseShapeI.filter(m_plFrame[m_symbolIdx].re/32767.0);
                q = m_pulseShapeQ.filter(m_plFrame[m_symbolIdx].im/32767.0);
                m_symbolIdx++;
                m_symbolCount--;
            }
        }
    }
    else
    {
        i = m_pulseShapeI.filter(0.0f);
        q = m_pulseShapeQ.filter(0.0f);
    }

    m_sampleIdx++;

    if (m_sampleIdx >= m_samplesPerSymbol) {
        m_sampleIdx = 0;
    }

    m_modSample.real(i);
    m_modSample.imag(q);

    // These levels aren't currently used in the GUI
    Real t = std::abs(m_modSample);
    calculateLevel(t);
}

void DATVModSource::calculateLevel(Real& sample)
{
    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), sample);
        m_levelSum += sample * sample;
        m_levelCalcCount++;
    }
    else
    {
        m_rmsLevel = std::sqrt(m_levelSum / m_levelNbSamples);
        m_peakLevelOut = m_peakLevel;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void DATVModSource::openTsFile(const QString& fileName)
{
    m_tsFileOK = false;
    m_mpegTSBitrate = getTSBitrate(fileName);

    if (m_mpegTSBitrate > 0)
    {
        m_mpegTSStream.open(qPrintable(fileName), std::ifstream::binary);
        if (m_mpegTSStream.is_open())
        {
            m_mpegTSStream.seekg (0, m_mpegTSStream.end);
            m_mpegTSSize = m_mpegTSStream.tellg();
            m_mpegTSStream.seekg (0, m_mpegTSStream.beg);
            m_frameIdx = 0;
            m_frameCount = 0;
            m_tsFileOK = true;
        }
        checkBitrates();
    }
    else
        qDebug() << "DATVModSource::openTsFile: Failed to get bitrate for transport stream file: " << fileName;

    if (m_tsFileOK)
    {
        m_settings.m_tsFileName = fileName;

        if (getMessageQueueToGUI())
        {
            DATVModReport::MsgReportTsFileSourceStreamData *report;
            report = DATVModReport::MsgReportTsFileSourceStreamData::create(m_mpegTSBitrate, m_mpegTSSize);
            getMessageQueueToGUI()->push(report);
        }
    }
    else
    {
        m_settings.m_tsFileName.clear();
        qDebug() << "DATVModSource::openTsFile: Cannot open file: " << fileName;
    }
}

void DATVModSource::seekTsFileStream(int seekPercentage)
{
    if (m_tsFileOK)
    {
        m_frameIdx = ((m_mpegTSSize / DVBS::tsPacketLen) * seekPercentage) / 100;
        m_mpegTSStream.seekg (m_frameIdx * (std::streampos)DVBS::tsPacketLen, m_mpegTSStream.beg);
        m_frameCount = (int)(m_frameIdx / m_tsRatio);
    }
}

void DATVModSource::reportTsFileSourceStreamTiming()
{
    int framesCount = m_tsFileOK ? m_frameIdx : 0;

    if (getMessageQueueToGUI())
        getMessageQueueToGUI()->push(DATVModReport::MsgReportTsFileSourceStreamTiming::create(framesCount));
}

void DATVModSource::reportUDPBitrate()
{
    boost::chrono::duration<double> sec = boost::chrono::steady_clock::now() - m_udpTimingStart;
    double seconds = sec.count();
    int bitrate = seconds > 0.0 ? m_udpByteCount * 8 / seconds : 0;
    m_udpTimingStart = boost::chrono::steady_clock::now();
    m_udpByteCount = 0;

    if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(DATVModReport::MsgReportUDPBitrate::create(bitrate));
    }
}

void DATVModSource::updateUDPBufferUtilization()
{
#ifdef _WIN32
    u_long count;
    ioctlsocket(m_udpSocket->socketDescriptor(), FIONREAD, &count);
    if (count > m_udpMaxBufferUtilization) {
        m_udpMaxBufferUtilization = count;
    }
#else
    // On linux, ioctl(s, SIOCINQ, &count); only returns length of first datagram, so we can't support this
#endif
}

void DATVModSource::reportUDPBufferUtilization()
{
    // Report maximum utilization since last call
    updateUDPBufferUtilization();

    if (getMessageQueueToGUI())
    {
        getMessageQueueToGUI()->push(DATVModReport::MsgReportUDPBufferUtilization::create(
            m_udpMaxBufferUtilization / (float)DATVModSettings::m_udpBufferSize * 100.0));
    }

    m_udpMaxBufferUtilization = 0;
}

void DATVModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "DATVModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        if (m_settings.m_symbolRate > 0)
        {
            m_sampleRate = (channelSampleRate/m_settings.m_symbolRate)*m_settings.m_symbolRate;

            // Create interpolator if not integer multiple
            if (m_sampleRate != channelSampleRate)
            {
                m_interpolatorDistanceRemain = 0;
                m_interpolatorDistance = (Real) m_sampleRate / (Real) channelSampleRate;
                m_interpolator.create(32, m_sampleRate, m_settings.m_rfBandwidth / 2.2f, 3.0);
            }
            if (getMessageQueueToGUI())
            {
                getMessageQueueToGUI()->push(DATVModReport::MsgReportRates::create(
                    channelSampleRate,
                    m_sampleRate,
                    getDVBSDataBitrate(m_settings)));
            }
        }
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;

    if (m_settings.m_symbolRate > 0)
        m_samplesPerSymbol = m_channelSampleRate/m_settings.m_symbolRate;

    m_pulseShapeI.create(m_settings.m_rollOff, 8, m_samplesPerSymbol, false);
    m_pulseShapeQ.create(m_settings.m_rollOff, 8, m_samplesPerSymbol, false);
}

void DATVModSource::applySettings(const DATVModSettings& settings, bool force)
{
    qDebug() << "DATVModSource::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_source: " << (int) settings.m_source
            << " m_standard: " << (int) settings.m_standard
            << " m_modulation: " << (int) settings.m_modulation
            << " m_fec: " << (int) settings.m_fec
            << " m_symbolRate: " << (int) settings.m_symbolRate
            << " m_rollOff: " << (int) settings.m_rollOff
            << " m_tsFilePlayLoop: " << settings.m_tsFilePlayLoop
            << " m_tsFilePlay: " << settings.m_tsFilePlay
            << " m_udpAddress: " << settings.m_udpAddress
            << " m_udpPort: " << settings.m_udpPort
            << " m_channelMute: " << settings.m_channelMute
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth)
        || (settings.m_modulation != m_settings.m_modulation)
        || (settings.m_symbolRate != m_settings.m_symbolRate)
        || force)
    {
        if (settings.m_symbolRate > 0)
        {
            m_sampleRate = (m_channelSampleRate/settings.m_symbolRate)*settings.m_symbolRate;

            if (m_sampleRate != m_channelSampleRate)
            {
                m_interpolatorDistanceRemain = 0;
                m_interpolatorDistance = (Real) m_sampleRate / (Real) m_channelSampleRate;
                m_interpolator.create(32, m_sampleRate, settings.m_rfBandwidth / 2.2f, 3.0);
            }
            if (getMessageQueueToGUI())
            {
                getMessageQueueToGUI()->push(DATVModReport::MsgReportRates::create(
                                                    m_channelSampleRate, m_sampleRate,
                                                    getDVBSDataBitrate(settings)));
            }
        }
        else
            qWarning() << "DATVModSource::applySettings: symbolRate must be greater than 0.";
    }

    if ((settings.m_source != m_settings.m_source)
        || (settings.m_udpAddress != m_settings.m_udpAddress)
        || (settings.m_udpPort != m_settings.m_udpPort)
        || force)
    {
        if (m_udpSocket)
        {
            m_udpSocket->close();
            delete m_udpSocket;
            m_udpSocket = nullptr;
        }
        if (settings.m_source == DATVModSettings::SourceUDP)
        {
            m_udpSocket = new QUdpSocket();
            m_udpSocket->bind(QHostAddress(settings.m_udpAddress), settings.m_udpPort);
            m_udpSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, DATVModSettings::m_udpBufferSize);
            m_udpTimingStart = boost::chrono::steady_clock::now();
            m_udpByteCount = 0;
            m_udpAbsByteCount = 0;
        }
    }

    if ((settings.m_standard != m_settings.m_standard)
        || (settings.m_modulation != m_settings.m_modulation)
        || force)
    {
        m_symbolSel = 0;
        m_symbolIdx = 0;
        m_symbolCount = 0;
        m_sampleIdx = 0;
    }

    if ((settings.m_standard != m_settings.m_standard)
        || (settings.m_modulation != m_settings.m_modulation)
        || (settings.m_fec != m_settings.m_fec)
        || (settings.m_rollOff != m_settings.m_rollOff)
        || force)
    {
        if (settings.m_standard == DATVModSettings::DVB_S)
        {
            switch (settings.m_fec)
            {
            case DATVModSettings::FEC12:
                m_dvbs.setCodeRate(DVBS::RATE_1_2);
                break;
            case DATVModSettings::FEC23:
                m_dvbs.setCodeRate(DVBS::RATE_2_3);
                break;
            case DATVModSettings::FEC34:
                m_dvbs.setCodeRate(DVBS::RATE_3_4);
                break;
            case DATVModSettings::FEC56:
                m_dvbs.setCodeRate(DVBS::RATE_5_6);
                break;
            case DATVModSettings::FEC78:
                m_dvbs.setCodeRate(DVBS::RATE_7_8);
                break;
            default:
                qCritical() << "DATVModSource::applySettings: Unsupported FEC code rate for DVB-S: " << settings.m_fec;
                break;
            }
        }
        else
        {
            m_dvbs2Format.frame_type = FRAME_NORMAL;
            m_dvbs2Format.pilots = 0; // PILOTS_OFF;
            m_dvbs2Format.dummy_frame = 0;
            m_dvbs2Format.null_deletion = 0;
            m_dvbs2Format.intface = M_ACM;  // Unused?
            m_dvbs2Format.broadcasting = 1;

            switch (settings.m_modulation)
            {
            case DATVModSettings::QPSK:
                m_dvbs2Format.constellation = M_QPSK;
                break;
            case DATVModSettings::PSK8:
                m_dvbs2Format.constellation = M_8PSK;
                break;
            case DATVModSettings::APSK16:
                m_dvbs2Format.constellation = M_16APSK;
                break;
            case DATVModSettings::APSK32:
                m_dvbs2Format.constellation = M_32APSK;
                break;
            default:
                qDebug() << "DATVModSource::applySettings: Unsupported modulation for DVB-S2";
                break;
            }

            switch (settings.m_fec)
            {
            case DATVModSettings::FEC12:
                m_dvbs2Format.code_rate = CR_1_2;
                break;
            case DATVModSettings::FEC23:
                m_dvbs2Format.code_rate = CR_2_3;
                break;
            case DATVModSettings::FEC34:
                m_dvbs2Format.code_rate = CR_3_4;
                break;
            case DATVModSettings::FEC56:
                m_dvbs2Format.code_rate = CR_5_6;
                break;
            case DATVModSettings::FEC45:
                m_dvbs2Format.code_rate = CR_4_5;
                break;
            case DATVModSettings::FEC89:
                m_dvbs2Format.code_rate = CR_8_9;
                break;
            case DATVModSettings::FEC910:
                m_dvbs2Format.code_rate = CR_9_10;
                break;
            case DATVModSettings::FEC14:
                m_dvbs2Format.code_rate = CR_1_4;
                break;
            case DATVModSettings::FEC13:
                m_dvbs2Format.code_rate = CR_1_3;
                break;
            case DATVModSettings::FEC25:
                m_dvbs2Format.code_rate = CR_2_5;
                break;
            case DATVModSettings::FEC35:
                m_dvbs2Format.code_rate = CR_3_5;
                break;
            default:
                qDebug() << "DATVModSource::getDVBSDataBitrate: Unsupported code rate for DVB-S2";
                break;
            }

            if (settings.m_rollOff == 0.35f)
                m_dvbs2Format.roll_off = RO_0_35;
            else if (settings.m_rollOff == 0.25f)
                m_dvbs2Format.roll_off = RO_0_25;
            else
                m_dvbs2Format.roll_off = RO_0_20;

            m_dvbs2.s2_set_configure(&m_dvbs2Format);
        }
        if (getMessageQueueToGUI())
        {
            getMessageQueueToGUI()->push(DATVModReport::MsgReportRates::create(
                                                m_channelSampleRate, m_sampleRate,
                                                getDVBSDataBitrate(settings)));
        }
    }

    m_settings = settings;

    if (m_settings.m_symbolRate > 0)
        m_samplesPerSymbol = m_channelSampleRate/m_settings.m_symbolRate;

    m_pulseShapeI.create(m_settings.m_rollOff, 8, m_samplesPerSymbol, false);
    m_pulseShapeQ.create(m_settings.m_rollOff, 8, m_samplesPerSymbol, false);

    checkBitrates();
}
