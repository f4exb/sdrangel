///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef _TESTSOURCE_TESTSOURCETHREAD_H_
#define _TESTSOURCE_TESTSOURCETHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>

#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"
#include "dsp/ncof.h"

#define TESTSOURCE_THROTTLE_MS 50

class TestSourceThread : public QThread {
	Q_OBJECT

public:
	TestSourceThread(SampleSinkFifo* sampleFifo, QObject* parent = 0);
	~TestSourceThread();

	void startWork();
	void stopWork();
	void setSamplerate(int samplerate);
    void setLog2Decimation(unsigned int log2_decim);
    void setFcPos(int fcPos);
	void setBitSize(uint32_t bitSizeIndex);
    void setAmplitudeBits(int32_t amplitudeBits);
    void setDCFactor(float iFactor);
    void setIFactor(float iFactor);
    void setQFactor(float qFactor);
    void setFrequencyShift(int shift);

    void connectTimer(const QTimer& timer);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	qint16  *m_buf;
    quint32 m_bufsize;
    quint32 m_chunksize;
	SampleVector m_convertBuffer;
	SampleSinkFifo* m_sampleFifo;
	NCOF m_nco;
	int m_frequencyShift;

	int m_samplerate;
    unsigned int m_log2Decim;
    int m_fcPos;
	uint32_t m_bitSizeIndex;
	uint32_t m_bitShift;
    int32_t m_amplitudeBits;
    float m_dcBias;
    float m_iBias;
    float m_qBias;
    int32_t m_amplitudeBitsDC;
    int32_t m_amplitudeBitsI;
    int32_t m_amplitudeBitsQ;

    uint64_t m_frequency;
    int m_fcPosShift;

    int m_throttlems;
    QElapsedTimer m_elapsedTimer;
    bool m_throttleToggle;
    QMutex m_mutex;

#ifdef SDR_RX_SAMPLE_24BIT
    Decimators<qint64, qint16, SDR_RX_SAMP_SZ, 8> m_decimators_8;
    Decimators<qint64, qint16, SDR_RX_SAMP_SZ, 12> m_decimators_12;
    Decimators<qint64, qint16, SDR_RX_SAMP_SZ, 16> m_decimators_16;
#else
	Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 8> m_decimators_8;
    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12> m_decimators_12;
    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 16> m_decimators_16;
#endif

	void run();
	void callback(const qint16* buf, qint32 len);
	void setBuffers(quint32 chunksize);
    void generate(quint32 chunksize);

	//  Decimate according to specified log2 (ex: log2=4 => decim=16)
	inline void convert_8(SampleVector::iterator* it, const qint16* buf, qint32 len)
	{
	    if (m_log2Decim == 0) {
	        m_decimators_8.decimate1(it, buf, len);
	    } else {
	        if (m_fcPos == 0) { // Infradyne
	            switch (m_log2Decim) {
	            case 1:
	                m_decimators_8.decimate2_inf(it, buf, len);
	                break;
	            case 2:
	                m_decimators_8.decimate4_inf(it, buf, len);
	                break;
	            case 3:
	                m_decimators_8.decimate8_inf(it, buf, len);
	                break;
	            case 4:
	                m_decimators_8.decimate16_inf(it, buf, len);
	                break;
	            case 5:
	                m_decimators_8.decimate32_inf(it, buf, len);
	                break;
                case 6:
                    m_decimators_8.decimate64_inf(it, buf, len);
                    break;
	            default:
	                break;
	            }
	        } else if (m_fcPos == 1)  {// Supradyne
	            switch (m_log2Decim) {
	            case 1:
	                m_decimators_8.decimate2_sup(it, buf, len);
	                break;
	            case 2:
	                m_decimators_8.decimate4_sup(it, buf, len);
	                break;
	            case 3:
	                m_decimators_8.decimate8_sup(it, buf, len);
	                break;
	            case 4:
	                m_decimators_8.decimate16_sup(it, buf, len);
	                break;
	            case 5:
	                m_decimators_8.decimate32_sup(it, buf, len);
	                break;
                case 6:
                    m_decimators_8.decimate64_sup(it, buf, len);
                    break;
	            default:
	                break;
	            }
	        } else { // Centered
	            switch (m_log2Decim) {
	            case 1:
	                m_decimators_8.decimate2_cen(it, buf, len);
	                break;
	            case 2:
	                m_decimators_8.decimate4_cen(it, buf, len);
	                break;
	            case 3:
	                m_decimators_8.decimate8_cen(it, buf, len);
	                break;
	            case 4:
	                m_decimators_8.decimate16_cen(it, buf, len);
	                break;
	            case 5:
	                m_decimators_8.decimate32_cen(it, buf, len);
	                break;
                case 6:
                    m_decimators_8.decimate64_cen(it, buf, len);
                    break;
	            default:
	                break;
	            }
	        }
	    }
	}

	void convert_12(SampleVector::iterator* it, const qint16* buf, qint32 len)
    {
        if (m_log2Decim == 0) {
            m_decimators_12.decimate1(it, buf, len);
        } else {
            if (m_fcPos == 0) { // Infradyne
                switch (m_log2Decim) {
                case 1:
                    m_decimators_12.decimate2_inf(it, buf, len);
                    break;
                case 2:
                    m_decimators_12.decimate4_inf(it, buf, len);
                    break;
                case 3:
                    m_decimators_12.decimate8_inf(it, buf, len);
                    break;
                case 4:
                    m_decimators_12.decimate16_inf(it, buf, len);
                    break;
                case 5:
                    m_decimators_12.decimate32_inf(it, buf, len);
                    break;
                case 6:
                    m_decimators_12.decimate64_inf(it, buf, len);
                    break;
                default:
                    break;
                }
            } else if (m_fcPos == 1)  {// Supradyne
                switch (m_log2Decim) {
                case 1:
                    m_decimators_12.decimate2_sup(it, buf, len);
                    break;
                case 2:
                    m_decimators_12.decimate4_sup(it, buf, len);
                    break;
                case 3:
                    m_decimators_12.decimate8_sup(it, buf, len);
                    break;
                case 4:
                    m_decimators_12.decimate16_sup(it, buf, len);
                    break;
                case 5:
                    m_decimators_12.decimate32_sup(it, buf, len);
                    break;
                case 6:
                    m_decimators_12.decimate64_sup(it, buf, len);
                    break;
                default:
                    break;
                }
            } else { // Centered
                switch (m_log2Decim) {
                case 1:
                    m_decimators_12.decimate2_cen(it, buf, len);
                    break;
                case 2:
                    m_decimators_12.decimate4_cen(it, buf, len);
                    break;
                case 3:
                    m_decimators_12.decimate8_cen(it, buf, len);
                    break;
                case 4:
                    m_decimators_12.decimate16_cen(it, buf, len);
                    break;
                case 5:
                    m_decimators_12.decimate32_cen(it, buf, len);
                    break;
                case 6:
                    m_decimators_12.decimate64_cen(it, buf, len);
                    break;
                default:
                    break;
                }
            }
        }
    }

	void convert_16(SampleVector::iterator* it, const qint16* buf, qint32 len)
    {
        if (m_log2Decim == 0) {
            m_decimators_16.decimate1(it, buf, len);
        } else {
            if (m_fcPos == 0) { // Infradyne
                switch (m_log2Decim) {
                case 1:
                    m_decimators_16.decimate2_inf(it, buf, len);
                    break;
                case 2:
                    m_decimators_16.decimate4_inf(it, buf, len);
                    break;
                case 3:
                    m_decimators_16.decimate8_inf(it, buf, len);
                    break;
                case 4:
                    m_decimators_16.decimate16_inf(it, buf, len);
                    break;
                case 5:
                    m_decimators_16.decimate32_inf(it, buf, len);
                    break;
                case 6:
                    m_decimators_16.decimate64_inf(it, buf, len);
                    break;
                default:
                    break;
                }
            } else if (m_fcPos == 1)  {// Supradyne
                switch (m_log2Decim) {
                case 1:
                    m_decimators_16.decimate2_sup(it, buf, len);
                    break;
                case 2:
                    m_decimators_16.decimate4_sup(it, buf, len);
                    break;
                case 3:
                    m_decimators_16.decimate8_sup(it, buf, len);
                    break;
                case 4:
                    m_decimators_16.decimate16_sup(it, buf, len);
                    break;
                case 5:
                    m_decimators_16.decimate32_sup(it, buf, len);
                    break;
                case 6:
                    m_decimators_16.decimate64_sup(it, buf, len);
                    break;
                default:
                    break;
                }
            } else { // Centered
                switch (m_log2Decim) {
                case 1:
                    m_decimators_16.decimate2_cen(it, buf, len);
                    break;
                case 2:
                    m_decimators_16.decimate4_cen(it, buf, len);
                    break;
                case 3:
                    m_decimators_16.decimate8_cen(it, buf, len);
                    break;
                case 4:
                    m_decimators_16.decimate16_cen(it, buf, len);
                    break;
                case 5:
                    m_decimators_16.decimate32_cen(it, buf, len);
                    break;
                case 6:
                    m_decimators_16.decimate64_cen(it, buf, len);
                    break;
                default:
                    break;
                }
            }
        }
    }


private slots:
    void tick();
};

#endif // _TESTSOURCE_TESTSOURCETHREAD_H_
