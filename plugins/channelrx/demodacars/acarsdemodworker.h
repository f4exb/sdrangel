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

#ifndef INCLUDE_ACARSDEMODWORKER_H
#define INCLUDE_ACARSDEMODWORKER_H

#include <QObject>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QUdpSocket>
#include <QTcpSocket>
#include <QHostAddress>
#include <QGeoCoordinate>
#include <QMetaType>

#include "util/message.h"
#include "util/messagequeue.h"
#include "util/aircraftreport.h"
#include "util/osndb.h"
// For m_airportInfo, which names the airports a message mentions
#include "util/ourairportsdb.h"

#include <libacars/reassembly.h>

#include "acarsdemodsettings.h"
#include "util/acarsmessage.h"
#include "util/acarsmultipartassembler.h"
#include "acarsvdl2.h"
#include "acarshfdl.h"
#include "acarsaero.h"
#include "vdl2atn.h"

class AcarsDemod;

// One decoded frame, ready for display: the worker computes every table column
// and the decode view content, so the GUI only builds rows from strings
struct AcarsRowEvent {
    int m_frameType = 0;            // 0 = ACARS message, 1 = VDL-2 link frame, 2 = HFDL frame, 3 = Aero signal unit
    // Frames carrying no usable information - VDL-2 supervisory and link management
    // responses, Aero satellite housekeeping - which the GUI hides by default
    bool m_noInfo = false;
    QDateTime m_received;
    bool m_uplink = false;

    // Which link the frame arrived on, and how fast it was going. Both kept out of
    // Label Decode, which is for what the Label means and nothing else.
    QString m_protocol;
    int m_bitRate = 0;              // bits per second, 0 when not known

    // Table column texts. The mode is the raw character - the GUI derives the
    // country-specific decode so it can relabel when the country changes.
    // The ground station the frame went to or came from, and its name. On plain ACARS
    // the mode is the ARINC 618 Mode character and the decode is derived from it in the
    // model; every other protocol names its ground station here, where the tables are.
    QString m_mode;
    QString m_modeDecode;
    // Where that ground station is, for the Find on map action on the GS columns. Only
    // the protocols whose stations are at known places have one - HFDL from its system
    // table, VDL-2 from the community list - so ACARS and Aero leave this unset.
    bool m_gsHasPosition = false;
    float m_gsLatitude = 0.0f;
    float m_gsLongitude = 0.0f;
    QString m_address;
    QString m_ack;
    QString m_label;
    QString m_labelDecode;
    QString m_blockId;
    QString m_originator;
    QString m_originatorDecode;
    QString m_messageNumber;
    QString m_blockSequence;
    QString m_flight;
    QString m_text;
    QString m_textDecode;           // Flattened single-line decode for the column
    QString m_atc;
    QString m_hex;
    bool m_hasPosition = false;
    bool m_hasAltitude = false;
    double m_latitude = 0.0;
    double m_longitude = 0.0;
    double m_altitudeFt = 0.0;

    // Decode view / Map checkbox payloads
    QString m_fullDecode;           // Multi-line plain decode (the VDL2_DECODE_ROLE content)
    bool m_useLibacars = false;
    QString m_viewDecodeHtml;       // Precomputed decode-view HTML for non-multipart rows

    // Multipart: rows of one assembly share an id; the assembly's evolving
    // state arrives via AcarsAssemblyEvent
    quint64 m_assemblyId = 0;
    int m_partNumber = 0;

    // For the GUI's aircraft-count chart
    QString m_chartAircraftId;

    // For the GUI's HFDL ground-stations-heard table: a squitter announces the
    // frequency sets of up to three stations
    QList<int> m_gsHeardIds;
    QList<QList<int>> m_gsHeardFreqs;   // kHz, parallel to m_gsHeardIds
};

// The state of a multipart assembly, for the decode view of its rows
struct AcarsAssemblyEvent {
    quint64 m_assemblyId = 0;
    bool m_isMultipart = false;
    bool m_uplink = false;
    QString m_statusText;
    int m_uniquePartCount = 0;
    int m_duplicateCount = 0;
    QStringList m_missingSequences;
    QString m_combinedHtmlBody;     // The "Combined message" section, ready for the view
};

Q_DECLARE_METATYPE(AcarsRowEvent)
Q_DECLARE_METATYPE(AcarsAssemblyEvent)

// Processes demodulated frames off the GUI thread: receives ACARS/VDL-2/HFDL
// frames directly from the sink and performs the higher layer decode (libacars,
// multipart assembly, ATN), the channel outputs ("packets" pipes, raw UDP, CSV
// log, aggregator feeds), aircraft reports and Map items. Runs on its own
// thread inside the channel, in both GUI and server builds; the GUI, when there
// is one, builds its table from this class's row events.
class AcarsDemodWorker : public QObject
{
    Q_OBJECT

public:
    class MsgConfigureWorker : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        const AcarsDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }
        static MsgConfigureWorker* create(const AcarsDemodSettings& settings, bool force) {
            return new MsgConfigureWorker(settings, force);
        }
    private:
        AcarsDemodSettings m_settings;
        bool m_force;
        MsgConfigureWorker(const AcarsDemodSettings& settings, bool force) :
            Message(), m_settings(settings), m_force(force)
        {}
    };

    // The aircraft database has been downloaded again: pick up the new one
    class MsgReloadAircraftDatabase : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        static MsgReloadAircraftDatabase* create() {
            return new MsgReloadAircraftDatabase();
        }
    private:
        MsgReloadAircraftDatabase() :
            Message()
        {}
    };

    // An ACARS block read back from a .csv log. Decoded and shown like a received one,
    // but kept off the outputs - UDP, the "packets" pipes, the log and the aggregator
    // feed - which would otherwise send old traffic out again as if it were new, and
    // append to the very log being read.
    class MsgReplayPacket : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        const QByteArray& getPacket() const { return m_packet; }
        QDateTime getDateTime() const { return m_dateTime; }
        static MsgReplayPacket* create(const QByteArray& packet, const QDateTime& dateTime) {
            return new MsgReplayPacket(packet, dateTime);
        }
    private:
        QByteArray m_packet;
        QDateTime m_dateTime;
        MsgReplayPacket(const QByteArray& packet, const QDateTime& dateTime) :
            Message(), m_packet(packet), m_dateTime(dateTime)
        {}
    };

    explicit AcarsDemodWorker(AcarsDemod *acarsDemod);
    ~AcarsDemodWorker();

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

public slots:
    void startWork();

signals:
    void rowReady(const AcarsRowEvent& event);
    void assemblyUpdated(const AcarsAssemblyEvent& event);

private:
    AcarsDemod *m_acarsDemod;
    AcarsDemodSettings m_settings;
    MessageQueue m_inputMessageQueue;
    qint64 m_centerFrequency = 0;

    // Channel outputs
    QUdpSocket *m_udpSocket = nullptr;  // Created on the worker thread
    QFile m_logFile;
    QTextStream m_logStream;

    // ACARS decode state
    la_reasm_ctx *m_reasmCtx = nullptr;
    AcarsMultipartAssembler m_multipartAssembler;
    // Identity handed to the multipart assembler for each decoded message. A counter,
    // not a pointer, so decoded messages no longer have to be kept alive to keep their
    // addresses unique - see processAcarsMessage()
    quint64 m_nextSourceId = 0;

    // VDL-2 decode state
    Vdl2AtnDecoder m_atnDecoder;        // X.25/CLNP/COTP/ATN decode of VDL-2 I frames
    struct Vdl2GsInfo
    {
        QString m_name;
        QString m_city;
        bool m_hasPosition = false;
        float m_latitude = 0.0f;
        float m_longitude = 0.0f;
    };
    QHash<quint32, Vdl2GsInfo> m_vdl2Gs;    // The community ground station list

    // HFDL decode state
    int m_hfdlNewestReportSecs = -1;    // Newest HFNPDU report time seen, for replay-aware freshness

    // Aggregator feeds (sockets created on the worker thread)

    // Databases for message decode and flight paths
    QSharedPointer<const QHash<QString, AircraftInformation *>> m_aircraftInfo;
    QSharedPointer<const QHash<int, AircraftInformation *>> m_aircraftInfoByIcao;

    // Label decode tables (moved from the GUI so the decode works headless)
    static QHash<QString, QString> m_labels;
    static QHash<QString, QString> m_subLabels;
    static QHash<QString, QString> m_originators;

    bool handleMessage(const Message& message);
    void applySettings(const AcarsDemodSettings& settings, bool force);
    void forwardToGui(Message *message);

    //!< icao is the address the bearer knew, or 0 - see AcarsDemod::MsgAcarsPacket
    //!< replay is true for a block read back from a .csv log - see MsgReplayPacket
    void processPacket(const QByteArray& packet, QDateTime dateTime, quint32 icao = 0, bool replay = false);
    void processAcarsMessage(QByteArray messageBytes, QDateTime received, quint32 icao = 0, bool replay = false);
    void processVdl2Frame(const AcarsVdl2Receiver::Frame& frame, QDateTime received);
    void processHfdlFrame(const AcarsHfdlReceiver::Frame& frame, QDateTime received);
    void processAeroFrame(const AcarsAeroReceiver::Frame& frame, QDateTime received);
    void loadVdl2Gs();
    void resetReassembly();
    bool hasGui() const;

    // The protocol the packets currently in flight came off, stamped by the sink rather
    // than read from m_settings - see AcarsDemod::MsgProtocolChange
    int m_protocolMode = 0;   // AcarsDemodSettings::ACARS
    void feedMessage(const QByteArray& block, const QDateTime& received);
    void sendAircraftReport(AircraftReport& report);
    void emitAssemblyState(const QSharedPointer<AcarsMultipartAssembler::Assembly>& assembly);

private slots:
    void handleInputMessages();
};

#endif // INCLUDE_ACARSDEMODWORKER_H
