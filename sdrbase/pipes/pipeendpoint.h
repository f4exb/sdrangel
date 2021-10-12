///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
//                                                                               //
// Parent for ChannelAPI and Features, where either can be used.                 //
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

#ifndef SDRBASE_PIPES_PIPEENDPOINT_H_
#define SDRBASE_PIPES_PIPEENDPOINT_H_

#include <QList>
#include <QString>
#include <QStringList>

#include "util/message.h"
#include "export.h"

class Feature;

class SDRBASE_API PipeEndPoint {
public:

    // Used by pipe sinks (channels or features) to record details about available pipe sources (channels or features)
    struct AvailablePipeSource
    {
        enum {RX, TX, Feature} m_type;
        int m_setIndex;
        int m_index;
        PipeEndPoint *m_source;
        QString m_id;

        AvailablePipeSource() = default;
        AvailablePipeSource(const AvailablePipeSource&) = default;
        AvailablePipeSource& operator=(const AvailablePipeSource&) = default;
        friend bool operator==(const AvailablePipeSource &lhs, const AvailablePipeSource &rhs)
        {
            return (lhs.m_type == rhs.m_type)
                && (lhs.m_setIndex == rhs.m_setIndex)
                && (lhs.m_source == rhs.m_source)
                && (lhs.m_id == rhs.m_id);
        }

        QString getTypeName() const
        {
            QStringList typeNames = {"R", "T", "F"};
            return typeNames[m_type];
        }

        // Name for use in GUI combo boxes and WebAPI
        QString getName() const
        {
            QString type;

            return QString("%1%2:%3 %4").arg(getTypeName())
                                        .arg(m_setIndex)
                                        .arg(m_index)
                                        .arg(m_id);
        }
    };

    class SDRBASE_API MsgReportPipes : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QList<AvailablePipeSource>& getAvailablePipes() { return m_availablePipes; }

        static MsgReportPipes* create() {
            return new MsgReportPipes();
        }

    private:
        QList<AvailablePipeSource> m_availablePipes;

        MsgReportPipes() :
            Message()
        {}
    };


protected:

    // Utility functions for pipe sinks to manage list of sources
    QList<AvailablePipeSource> updateAvailablePipeSources(QString pipeName, QStringList pipeTypes, QStringList pipeURIs, PipeEndPoint *destination);
    PipeEndPoint *getPipeEndPoint(const QString name, const QList<AvailablePipeSource> &availablePipeSources);

};

#endif // SDRBASE_PIPES_PIPEENDPOINT_H_
