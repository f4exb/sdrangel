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

#ifndef SDRBASE_PIPES_DATAPIPES_H_
#define SDRBASE_PIPES_DATAPIPES_H_

#include <QObject>
#include <QHash>
#include <QMap>
#include <QMutex>
#include <QThread>

#include "export.h"

#include "datapipescommon.h"
#include "elementpipesregistrations.h"

class ChannelAPI;
class Feature;
class DataPipesGCWorker;
class DataFifo;

class SDRBASE_API DataPipes : public QObject
{
    Q_OBJECT
public:
    DataPipes();
    DataPipes(const DataPipes&) = delete;
    DataPipes& operator=(const DataPipes&) = delete;
    ~DataPipes();

    DataFifo *registerChannelToFeature(const ChannelAPI *source, Feature *feature, const QString& type);
    void unregisterChannelToFeature(const ChannelAPI *source, Feature *feature, const QString& type);
    QList<DataFifo*>* getFifos(const ChannelAPI *source, const QString& type);

private:
    ElementPipesRegistrations<ChannelAPI, Feature, DataFifo> m_registrations;
    QThread m_gcThread; //!< Garbage collector thread
    DataPipesGCWorker *m_gcWorker; //!< Garbage collector

	void startGC(); //!< Start garbage collector
	void stopGC();  //!< Stop garbage collector
};

#endif // SDRBASE_PIPES_DATAPIPES_H_
