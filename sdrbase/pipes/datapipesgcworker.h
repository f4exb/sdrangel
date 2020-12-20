///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_PIPES_DATAPIPESGCWORKER_H_
#define SDRBASE_PIPES_DATAPIPESGCWORKER_H_

#include <QObject>
#include <QTimer>

#include "export.h"

#include "elementpipesgc.h"
#include "datapipescommon.h"

class QMutex;
class DataFifo;

class SDRBASE_API DataPipesGCWorker : public QObject
{
    Q_OBJECT
public:
    DataPipesGCWorker();
    ~DataPipesGCWorker();

    void setC2FRegistrations(
        QMutex *c2fMutex,
        QMap<DataPipesCommon::ChannelRegistrationKey, QList<DataFifo*>> *c2fFifos,
        QMap<DataPipesCommon::ChannelRegistrationKey, QList<Feature*>> *c2fFeatures
    )
    {
        m_dataPipesGC.setRegistrations(c2fMutex, c2fFifos, c2fFeatures);
    }

    void startWork();
    void stopWork();
    void addDataFifoToDelete(DataFifo *dataFifo);
    bool isRunning() const { return m_running; }

private:
    class DataPipesGC : public ElementPipesGC<ChannelAPI, Feature, DataFifo>
    {
    private:
        virtual bool existsProducer(const ChannelAPI *channelAPI);
        virtual bool existsConsumer(const Feature *feature);
        virtual void sendMessageToConsumer(const DataFifo *fifo,  DataPipesCommon::ChannelRegistrationKey key, Feature *feature);
    };

    DataPipesGC m_dataPipesGC;
    bool m_running;
    QTimer m_gcTimer;

private slots:
    void processGC(); //!< Collect garbage
};

#endif // SDRBASE_PIPES_DATAPIPESGCWORKER_H_
