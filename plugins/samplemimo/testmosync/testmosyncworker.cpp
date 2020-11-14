///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include <QTimer>
#include <QDebug>

#include "dsp/samplemofifo.h"
#include "dsp/basebandsamplesink.h"

#include "testmosyncsettings.h"
#include "testmosyncworker.h"

TestMOSyncWorker::TestMOSyncWorker(QObject* parent) :
    QObject(parent),
    m_running(false),
    m_buf(nullptr),
    m_log2Interp(0),
    m_throttlems(TestMOSyncSettings::m_msThrottle),
    m_throttleToggle(false),
    m_samplesRemainder(0),
    m_samplerate(0),
    m_feedSpectrumIndex(0),
    m_spectrumSink(nullptr)
{
    qDebug("TestMOSyncWorker::TestMOSyncWorker");
    setSamplerate(48000);
}

TestMOSyncWorker::~TestMOSyncWorker()
{
    qDebug("TestMOSyncWorker::~TestMOSyncWorker");

    if (m_running) {
        stopWork();
    }

    delete[] m_buf;
}

void TestMOSyncWorker::startWork()
{
    qDebug("TestMOSyncWorker::startWork");
    m_elapsedTimer.start();
    m_running = true;
}

void TestMOSyncWorker::stopWork()
{
    qDebug("TestMOSyncWorker::stopWork");
    m_running = false;
}

void TestMOSyncWorker::connectTimer(const QTimer& timer)
{
	qDebug() << "TestMOSyncWorker::connectTimer";
	connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void TestMOSyncWorker::setSamplerate(int samplerate)
{
	if (samplerate != m_samplerate)
	{
	    qDebug() << "TestMOSyncWorker::setSamplerate:"
	            << " new:" << samplerate
	            << " old:" << m_samplerate;

	    bool wasRunning = false;

		if (m_running)
		{
			stopWork();
			wasRunning = true;
		}

        m_samplerate = samplerate;
        m_samplesChunkSize = (m_samplerate * m_throttlems) / 1000;
        m_blockSize = (m_samplerate * 50) / 1000;

        if (m_buf) {
            delete[] m_buf;
        }

        m_buf = new qint16[2*m_blockSize*2];

        if (wasRunning) {
            startWork();
        }
	}
}

void TestMOSyncWorker::setLog2Interpolation(unsigned int log2Interpolation)
{
    if (log2Interpolation > 6) {
        return;
    }

    if (log2Interpolation != m_log2Interp)
    {
        qDebug() << "TestSinkThread::setLog2Interpolation:"
                << " new:" << log2Interpolation
                << " old:" << m_log2Interp;

        bool wasRunning = false;

        if (m_running)
        {
            stopWork();
            wasRunning = true;
        }

        m_log2Interp = log2Interpolation;

        if (wasRunning) {
            startWork();
        }
    }
}

unsigned int TestMOSyncWorker::getLog2Interpolation() const
{
    return m_log2Interp;
}

void TestMOSyncWorker::setFcPos(int fcPos)
{
    m_fcPos = fcPos;
}

int TestMOSyncWorker::getFcPos() const
{
    return m_fcPos;
}

void TestMOSyncWorker::callback(qint16* buf, qint32 samplesPerChannel)
{
    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
    m_sampleFifo->readSync(samplesPerChannel/(1<<m_log2Interp), iPart1Begin, iPart1End, iPart2Begin, iPart2End);

    if (iPart1Begin != iPart1End)
    {
        callbackPart(buf, (iPart1End - iPart1Begin)*(1<<m_log2Interp), iPart1Begin);
    }

    if (iPart2Begin != iPart2End)
    {
        unsigned int shift = (iPart1End - iPart1Begin)*(1<<m_log2Interp);
        callbackPart(buf + 2*shift, (iPart2End - iPart2Begin)*(1<<m_log2Interp), iPart2Begin);
    }
}

//  Interpolate according to specified log2 (ex: log2=4 => decim=16). len is a number of samples (not a number of I or Q)
void TestMOSyncWorker::callbackPart(qint16* buf, qint32 nSamples, int iBegin)
{
    for (unsigned int channel = 0; channel < 2; channel++)
    {
        SampleVector::iterator begin = m_sampleFifo->getData(channel).begin() + iBegin;

        if (m_log2Interp == 0)
        {
            m_interpolators[channel].interpolate1(&begin, &buf[channel*2*nSamples], 2*nSamples);
        }
        else
        {
            if (m_fcPos == 0) // Infra
            {
                switch (m_log2Interp)
                {
                case 1:
                    m_interpolators[channel].interpolate2_inf(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 2:
                    m_interpolators[channel].interpolate4_inf(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 3:
                    m_interpolators[channel].interpolate8_inf(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 4:
                    m_interpolators[channel].interpolate16_inf(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 5:
                    m_interpolators[channel].interpolate32_inf(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 6:
                    m_interpolators[channel].interpolate64_inf(&begin, &buf[channel*2*nSamples], 2*nSamples);
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
                    m_interpolators[channel].interpolate2_sup(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 2:
                    m_interpolators[channel].interpolate4_sup(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 3:
                    m_interpolators[channel].interpolate8_sup(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 4:
                    m_interpolators[channel].interpolate16_sup(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 5:
                    m_interpolators[channel].interpolate32_sup(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 6:
                    m_interpolators[channel].interpolate64_sup(&begin, &buf[channel*2*nSamples], 2*nSamples);
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
                    m_interpolators[channel].interpolate2_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 2:
                    m_interpolators[channel].interpolate4_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 3:
                    m_interpolators[channel].interpolate8_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 4:
                    m_interpolators[channel].interpolate16_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 5:
                    m_interpolators[channel].interpolate32_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 6:
                    m_interpolators[channel].interpolate64_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                default:
                    break;
                }
            }
        }

        if (channel == m_feedSpectrumIndex) {
            feedSpectrum(&buf[channel*2*nSamples], nSamples*2);
        }
    }
}

void TestMOSyncWorker::tick()
{
	if (m_running)
	{
        qint64 throttlems = m_elapsedTimer.restart();

        if (throttlems != m_throttlems)
        {
            m_throttlems = throttlems;
            m_samplesChunkSize = (m_samplerate * (m_throttlems+(m_throttleToggle ? 1 : 0))) / 1000;
            m_throttleToggle = !m_throttleToggle;
        }

        unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
        std::vector<SampleVector>& data = m_sampleFifo->getData();
        m_sampleFifo->readSync(m_samplesChunkSize, iPart1Begin, iPart1End, iPart2Begin, iPart2End);

        if (iPart1Begin != iPart1End) {
            callbackPart(data, iPart1Begin, iPart1End);
        }

        if (iPart2Begin != iPart2End) {
            callbackPart(data, iPart2Begin, iPart2End);
        }
	}
}

void TestMOSyncWorker::callbackPart(std::vector<SampleVector>& data, unsigned int iBegin, unsigned int iEnd)
{
    unsigned int chunkSize = iEnd - iBegin;

    for (unsigned int channel = 0; channel < 2; channel++)
    {
        SampleVector::iterator beginRead = data[channel].begin()  + iBegin;

        if (m_log2Interp == 0)
        {
            m_interpolators[channel].interpolate1(&beginRead, m_buf, 2*chunkSize);

            if (channel == m_feedSpectrumIndex) {
                feedSpectrum(m_buf, 2*chunkSize);
            }
        }
        else
        {
            switch (m_log2Interp)
            {
            case 1:
                m_interpolators[channel].interpolate2_cen(&beginRead, m_buf, chunkSize*(1<<m_log2Interp)*2);
                break;
            case 2:
                m_interpolators[channel].interpolate4_cen(&beginRead, m_buf, chunkSize*(1<<m_log2Interp)*2);
                break;
            case 3:
                m_interpolators[channel].interpolate8_cen(&beginRead, m_buf, chunkSize*(1<<m_log2Interp)*2);
                break;
            case 4:
                m_interpolators[channel].interpolate16_cen(&beginRead, m_buf, chunkSize*(1<<m_log2Interp)*2);
                break;
            case 5:
                m_interpolators[channel].interpolate32_cen(&beginRead, m_buf, chunkSize*(1<<m_log2Interp)*2);
                break;
            case 6:
                m_interpolators[channel].interpolate64_cen(&beginRead, m_buf, chunkSize*(1<<m_log2Interp)*2);
                break;
            default:
                break;
            }

            if (channel == m_feedSpectrumIndex) {
                feedSpectrum(m_buf, 2*chunkSize*(1<<m_log2Interp));
            }
        }
    }
}

void TestMOSyncWorker::feedSpectrum(int16_t *buf, unsigned int bufSize)
{
    if (!m_spectrumSink) {
        return;
    }

    m_samplesVector.allocate(bufSize/2);
    Sample16 *s16Buf = (Sample16*) buf;

    std::transform(
        s16Buf,
        s16Buf + (bufSize/2),
        m_samplesVector.m_vector.begin(),
        [](Sample16 s) -> Sample {
            return Sample{s.m_real, s.m_imag};
        }
    );

    m_spectrumSink->feed(m_samplesVector.m_vector.begin(), m_samplesVector.m_vector.begin() + (bufSize/2), false);
}
