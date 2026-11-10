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

#ifndef INCLUDE_DSEMODTXSESSION_H
#define INCLUDE_DSEMODTXSESSION_H

#include <deque>
#include <memory>

#include <QByteArray>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

#include "dsp/dv/dvframe.h"

#include "dsemodsettings.h"

class DSEModPacketBuilder;

// Transport-independent admission and bounded storage. Used only on the processor
// thread: a successful submission reserves its entire payload before returning.
class DSEModTxSession
{
public:
    using Frame = DVFrame;

    static constexpr int MaxFrames = 250;
    static constexpr int MaxChunkBytes = 96000;
    static constexpr int IdleTimeoutMs = 5000;
    static constexpr int PCMMaxDurationMs = 180000;
    static constexpr int DefaultFrameMaxDurationMs = 180000;
    static constexpr int MinFrameMaxDurationMs = 180000;
    static constexpr int MaxFrameMaxDurationMs = 86400000;
    //! mode AnyAngel61: an Angel61 frame record of any profile (the "angel61" input)
    static constexpr int AnyAngel61 = -1;

    int submit(const QJsonObject& request, const DSEModSettings& settings, QString& error);
    QJsonObject report() const;
    static QJsonObject capabilities(const DSEModSettings& settings);
    static bool decodeBase64(const QJsonValue& value, QByteArray& data);
    static bool keys(const QJsonObject& object, const QStringList& allowed);
    static bool integer(const QJsonValue& value, int low, int high);
    //! An Angel61 frame record's check (DVAngel61::unpackRecord: type, length, CRC-12) and the DSE
    //! mode of its profile, or 0
    static int angel61Mode(const Frame& frame, QString& error);
    static bool parseFrame(const QJsonObject& object, int mode, Frame& frame, QString& error);

    void complete(const QString& state, const QString& error = QString());
    void setState(const QString& state) { m_state = state; }
    bool active() const { return m_active; }
    //! The request is the one that opened the current or last session, sent again
    bool isOpenRetry(const QJsonObject& request) const;
    bool closed() const { return m_closed; }
    bool aborted() const { return m_abort; }
    bool timedOut() const;
    bool expired() const { return m_lifetime.isValid() && (m_lifetime.elapsed() >= m_maxDurationMs); }

    const QString& input() const { return m_input; }
    const QString& id() const { return m_id; }
    int rate() const { return m_rate; }
    int mode() const { return m_mode; }
    //! "angel61" input: the profile of the call's frames as its DSE mode (9 NB, 10 WB, 11 HF-NB,
    //! 12 HF-WB), 0 before the first
    int sourceProtocol() const { return m_sourceProtocol; }
    const QString& source() const { return m_source; }
    const QString& destination() const { return m_destination; }
    //! "angel61" input: the path of the call as this modulator sends it (a gateway's callsign), or empty
    const QString& via() const { return m_via; }
    int maxDurationMs() const { return m_maxDurationMs; }

    int pcmBytes() const { return m_pcm.size(); }
    int frameCount() const { return (int) m_frames.size(); }
    //! The queued frames, oldest first (a revision 3 relay looks for its call's rate before key-up)
    const std::deque<Frame>& frames() const { return m_frames; }
    QByteArray takePCM(int bytes);
    Frame takeFrame();
    void consumed(int count) { m_consumed += count; }
    void reset();

private:
    QString m_id;
    QString m_input;
    QString m_state = "idle";
    QString m_error;
    int m_mode = 0;
    int m_sourceProtocol = 0;
    int m_rate = 0;
    int m_next = 0;
    int m_maxDurationMs = PCMMaxDurationMs;
    QString m_source;
    QString m_destination;
    QString m_via;
    qint64 m_accepted = 0;
    qint64 m_consumed = 0;
    bool m_active = false;
    bool m_closed = false;
    bool m_abort = false;
    QByteArray m_pcm;
    std::deque<Frame> m_frames;
    QMap<int, QByteArray> m_receipts;
    QByteArray m_openReceipt;
    QByteArray m_closeReceipt;
    QElapsedTimer m_arrival;
    QElapsedTimer m_lifetime;
    QJsonArray m_history;
    std::shared_ptr<DSEModPacketBuilder> m_validator;
};

#endif
