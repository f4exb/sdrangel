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

#ifndef PLUGINS_SAMPLESOURCE_XTRXOUTPUT_XTRXOUTPUTTHREAD_H_
#define PLUGINS_SAMPLESOURCE_XTRXOUTPUT_XTRXOUTPUTTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "xtrx_api.h"

#include "dsp/interpolators.h"
#include "xtrx/devicextrxshared.h"

struct xtrx_dev;
class SampleSourceFifo;

class XTRXOutputThread : public QThread, public DeviceXTRXShared::ThreadInterface
{
    Q_OBJECT

public:
    XTRXOutputThread(struct xtrx_dev *dev, unsigned int nbChannels, unsigned int uniqueChannelIndex = 0, QObject* parent = 0);
    ~XTRXOutputThread();

    virtual void startWork();
    virtual void stopWork();
    virtual bool isRunning() { return m_running; }
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
        Interpolators<qint16, SDR_TX_SAMP_SZ, 12> m_interpolators;

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
    struct xtrx_dev *m_dev;

    Channel *m_channels; //!< Array of channels dynamically allocated for the given number of Rx channels
    unsigned int m_nbChannels;
    unsigned int m_uniqueChannelIndex;

    void run();
    unsigned int getNbFifos();
    void callbackSO(qint16* buf, qint32 len);
    void callbackMO(qint16* buf0, qint16* buf1, qint32 len);
    void callbackPart(qint16* buf, SampleVector& data, unsigned int iBegin, unsigned int iEnd);
};

#endif /* PLUGINS_SAMPLESOURCE_XTRXOUTPUT_XTRXOUTPUTTHREAD_H_ */
