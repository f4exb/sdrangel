///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_DSP_DV_DVFRAME_H
#define INCLUDE_DSP_DV_DVFRAME_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include "export.h"
#include "util/message.h"

// A complete, packed protocol unit for external transmission. Payload packing
// is specified by format (dstar-header/dstar-voice/dmr-burst/ysf-frame). It owns
// its bytes so it can also cross an in-process message queue safely. Transport
// wrappers such as DSRP/DMRD/YSFD are removed before this unit is scheduled.
struct DVFrame
{
    QString format;
    QString type;
    QByteArray data;
    int slot = 0;
    int sequence = 0;
};

/**
 * One ordered event from a digital-voice receiver. The event owns all of its
 * data and is safe to pass through a MessageQueue. Sequence numbers order the
 * envelope; DVFrame::sequence remains the protocol's sequence number.
 */
struct DVFrameEvent
{
    static constexpr int Version = 1;
    enum Kind { Start, Frame, End, Lost };

    int version = Version;
    QString protocol;
    quint64 callId = 0;
    Kind kind = Frame;
    quint64 sequence = 0;
    qint64 sampleIndex = -1;
    DVFrame frame;
    QString source;
    QString destination;
    QString via;            //!< the first entry of the call's path (Angel61: a repeater or gateway)
    bool group = true;
    QString reason;
};

class SDRBASE_API MsgDVFrame : public Message
{
    MESSAGE_CLASS_DECLARATION

public:
    const DVFrameEvent& getEvent() const { return m_event; }

    static MsgDVFrame *create(const DVFrameEvent& event)
    {
        return new MsgDVFrame(event);
    }

private:
    DVFrameEvent m_event;

    explicit MsgDVFrame(const DVFrameEvent& event) :
        Message(),
        m_event(event)
    {}
};

#endif
