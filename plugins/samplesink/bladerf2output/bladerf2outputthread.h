///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESINK_BLADERF2OUTPUT_BLADERF2OUTPUTTHREAD_H_
#define PLUGINS_SAMPLESINK_BLADERF2OUTPUT_BLADERF2OUTPUTTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <libbladeRF.h>

#include "bladerf2/devicebladerf2shared.h"
#include "dsp/interpolators.h"

class SampleSourceFifo;

class BladeRF2OutputThread : public QThread {
    Q_OBJECT

public:
    BladeRF2OutputThread(struct bladerf* dev, unsigned int nbTxChannels, QObject* parent = 0);
    ~BladeRF2OutputThread();

    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    unsigned int getNbChannels() const { return m_nbChannels; }
    void setLog2Interpolation(unsigned int channel, unsigned int log2_interp);
    unsigned int getLog2Interpolation(unsigned int channel) const;
    void setFifo(unsigned int channel, SampleSourceFifo *sampleFifo);
    SampleSourceFifo *getFifo(unsigned int channel);

private:
    struct Channel
    {
        SampleSourceFifo* m_sampleFifo;
        unsigned int m_log2Interp;
        Interpolators<qint16, SDR_TX_SAMP_SZ, 12>  m_interpolators;

        Channel() :
            m_sampleFifo(0),
            m_log2Interp(0)
        {}

        ~Channel()
        {}
    };

    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;
    struct bladerf* m_dev;

    Channel *m_channels; //!< Array of channels dynamically allocated for the given number of Tx channels
    qint16 *m_buf; //!< Full buffer for SISO or MIMO operation
    unsigned int m_nbChannels;

    void run();
    unsigned int getNbFifos();
    void callbackSO(qint16* buf, qint32 len, unsigned int channel = 0);
    void callbackMO(qint16* buf, qint32 samplesPerChannel);
    void callbackPart(qint16* buf, SampleVector& data, unsigned int iBegin, unsigned int iEnd, unsigned int channel);
};



#endif /* PLUGINS_SAMPLESINK_BLADERF2OUTPUT_BLADERF2OUTPUTTHREAD_H_ */
