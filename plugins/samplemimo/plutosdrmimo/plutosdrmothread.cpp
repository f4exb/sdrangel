///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
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

#include "plutosdr/deviceplutosdrbox.h"
#include "dsp/samplemofifo.h"

#include "plutosdrmimosettings.h"
#include "plutosdrmothread.h"

PlutoSDRMOThread::PlutoSDRMOThread(DevicePlutoSDRBox* plutoBox, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_plutoBox(plutoBox),
    m_log2Interp(0)
{
    qDebug("PlutoSDRMOThread::PlutoSDRMOThread");
    m_buf[0] = new qint16[2*PlutoSDRMIMOSettings::m_plutoSDRBlockSizeSamples];
    m_buf[1] = new qint16[2*PlutoSDRMIMOSettings::m_plutoSDRBlockSizeSamples];
}

PlutoSDRMOThread::~PlutoSDRMOThread()
{
    qDebug("PlutoSDRMOThread::~PlutoSDRMOThread");

    if (m_running) {
        stopWork();
    }

    delete[] m_buf[0];
    delete[] m_buf[1];
}

void PlutoSDRMOThread::startWork()
{
    m_startWaitMutex.lock();
    start();

    while(!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void PlutoSDRMOThread::stopWork()
{
    m_running = false;
    wait();
}

void PlutoSDRMOThread::setLog2Interpolation(unsigned int log2Interp)
{
    qDebug("PlutoSDRMOThread::setLog2Interpolation: %u", log2Interp);
    m_log2Interp = log2Interp;
}

unsigned int PlutoSDRMOThread::getLog2Interpolation() const
{
    return m_log2Interp;
}

void PlutoSDRMOThread::setFcPos(int fcPos)
{
    m_fcPos = fcPos;
}

int PlutoSDRMOThread::getFcPos() const
{
    return m_fcPos;
}

void PlutoSDRMOThread::run()
{
    std::ptrdiff_t p_inc = m_plutoBox->txBufferStep();
    int sampleSize = 2*m_plutoBox->getTxSampleBytes(); // I/Q sample size in bytes
    int nbChan = p_inc / sampleSize; // number of I/Q channels

    qDebug("PlutoSDRMOThread::run: nbChan: %d", nbChan);
    qDebug("PlutoSDRMOThread::run: I+Q bytes %d", sampleSize);
    qDebug("PlutoSDRMOThread::run: txBufferStep: %ld bytes", p_inc);
    qDebug("PlutoSDRMOThread::run: Rx all samples size is %ld bytes", m_plutoBox->getRxSampleSize());
    qDebug("PlutoSDRMOThread::run: Tx all samples size is %ld bytes", m_plutoBox->getTxSampleSize());
    qDebug("PlutoSDRMOThread::run: nominal nbytes_tx is %ld bytes", PlutoSDRMIMOSettings::m_plutoSDRBlockSizeSamples*p_inc);

    m_running = true;
    m_startWaiter.wakeAll();

    while (m_running)
    {
        ssize_t nbytes_tx;
        char *p_dat, *p_end;
        int ihs = 0; // half sample index (I then Q to make a sample)
        // WRITE: Get pointers to TX buf and number of bytes to read from FIFO
        p_dat = m_plutoBox->txBufferFirst();
        p_end = m_plutoBox->txBufferEnd();
        int nbOutSamples = (p_end - p_dat) / (4*nbChan);

        callback(m_buf, nbOutSamples);

        // p_inc is 2 on a char* buffer therefore each iteration processes only the I or Q sample
        // I and Q samples are processed one after the other
        // conversion is not needed as samples are little endian

        for (p_dat = m_plutoBox->txBufferFirst(), ihs = 0; p_dat < p_end; p_dat += p_inc, ihs += 2)
        {
            m_plutoBox->txChannelConvert((int16_t*) p_dat, &m_buf[0][ihs]);

            if (nbChan > 1) { // interleave with second chanel
                m_plutoBox->txChannelConvert(1, (int16_t*) (p_dat+sampleSize), &m_buf[1][ihs]);
            }
        }

        // Schedule TX buffer for sending
        nbytes_tx = m_plutoBox->txBufferPush();

        if (nbytes_tx != nbChan*sampleSize*PlutoSDRMIMOSettings::m_plutoSDRBlockSizeSamples)
        {
            qDebug("PlutoSDRMOThread::run: error pushing buf %d / %d",
                (int) nbytes_tx, (int) sampleSize*PlutoSDRMIMOSettings::m_plutoSDRBlockSizeSamples);
            usleep(200000);
            continue;
        }
    }

    m_running = false;
}

void PlutoSDRMOThread::callback(qint16* buf[2], qint32 samplesPerChannel)
{
    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
    m_sampleFifo->readSync(samplesPerChannel/(1<<m_log2Interp), iPart1Begin, iPart1End, iPart2Begin, iPart2End);

    if (iPart1Begin != iPart1End) {
        callbackPart(buf, (iPart1End - iPart1Begin)*(1<<m_log2Interp), iPart1Begin);
    }

    if (iPart2Begin != iPart2End)
    {
        unsigned int shift = (iPart1End - iPart1Begin)*(1<<m_log2Interp);
        qint16 *buf2[2];
        buf2[0] = buf[0] + 2*shift;
        buf2[1] = buf[1] + 2*shift;
        callbackPart(buf2, (iPart2End - iPart2Begin)*(1<<m_log2Interp), iPart2Begin);
    }
}

//  Interpolate according to specified log2 (ex: log2=4 => decim=16). len is a number of samples (not a number of I or Q)
void PlutoSDRMOThread::callbackPart(qint16* buf[2], qint32 nSamples, int iBegin)
{
    for (unsigned int channel = 0; channel < 2; channel++)
    {
        SampleVector::iterator begin = m_sampleFifo->getData(channel).begin() + iBegin;

        if (m_log2Interp == 0)
        {
            m_interpolators[channel].interpolate1(&begin, buf[channel], 2*nSamples);
        }
        else
        {
            if (m_fcPos == 0) // Infra
            {
                switch (m_log2Interp)
                {
                case 1:
                    m_interpolators[channel].interpolate2_inf(&begin, buf[channel], 2*nSamples);
                    break;
                case 2:
                    m_interpolators[channel].interpolate4_inf(&begin, buf[channel], 2*nSamples);
                    break;
                case 3:
                    m_interpolators[channel].interpolate8_inf(&begin, buf[channel], 2*nSamples);
                    break;
                case 4:
                    m_interpolators[channel].interpolate16_inf(&begin, buf[channel], 2*nSamples);
                    break;
                case 5:
                    m_interpolators[channel].interpolate32_inf(&begin, buf[channel], 2*nSamples);
                    break;
                case 6:
                    m_interpolators[channel].interpolate64_inf(&begin, buf[channel], 2*nSamples);
                    break;
                default:
                    break;
                }
            }
            else if (m_fcPos == 1) // Supra
            {
                switch (m_log2Interp)
                {
                case 1:
                    m_interpolators[channel].interpolate2_sup(&begin, buf[channel], 2*nSamples);
                    break;
                case 2:
                    m_interpolators[channel].interpolate4_sup(&begin, buf[channel], 2*nSamples);
                    break;
                case 3:
                    m_interpolators[channel].interpolate8_sup(&begin, buf[channel], 2*nSamples);
                    break;
                case 4:
                    m_interpolators[channel].interpolate16_sup(&begin, buf[channel], 2*nSamples);
                    break;
                case 5:
                    m_interpolators[channel].interpolate32_sup(&begin, buf[channel], 2*nSamples);
                    break;
                case 6:
                    m_interpolators[channel].interpolate64_sup(&begin, buf[channel], 2*nSamples);
                    break;
                default:
                    break;
                }
            }
            else if (m_fcPos == 2) // Center
            {
                switch (m_log2Interp)
                {
                case 1:
                    m_interpolators[channel].interpolate2_cen(&begin, buf[channel], 2*nSamples);
                    break;
                case 2:
                    m_interpolators[channel].interpolate4_cen(&begin, buf[channel], 2*nSamples);
                    break;
                case 3:
                    m_interpolators[channel].interpolate8_cen(&begin, buf[channel], 2*nSamples);
                    break;
                case 4:
                    m_interpolators[channel].interpolate16_cen(&begin, buf[channel], 2*nSamples);
                    break;
                case 5:
                    m_interpolators[channel].interpolate32_cen(&begin, buf[channel], 2*nSamples);
                    break;
                case 6:
                    m_interpolators[channel].interpolate64_cen(&begin, buf[channel], 2*nSamples);
                    break;
                default:
                    break;
                }
            }

        }
    }
}
