///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include <QDateTime>
#include <QDebug>
#include <QStringList>

#include "meshcoremodencoder.h"
#include "meshcoremodencoderlora.h"
#include "meshcorepacket.h"
#include "meshcore_identity.h"

namespace
{

// Translate `MeshcoreModSettings::MessageType` + identity into a single
// `MESHCORE: type=...; ...` command string that `Packet::buildFrameFromCommand`
// already knows how to render into wire bytes. All optional knobs (lat/lon,
// txt_type, route, ts) come from settings; the seed is loaded from the
// shared identity store on demand.
QString buildMeshcoreCommandFromSettings(const MeshcoreModSettings& s)
{
    auto loadSeedHex = [&]() -> QString {
        const QString path = s.m_meshIdentityPath.isEmpty()
            ? modemmeshcore::identity::defaultIdentityPath()
            : s.m_meshIdentityPath;
        modemmeshcore::identity::Identity id =
            modemmeshcore::identity::loadOrCreateIdentity(path);
        return id.isValid() ? id.seedHex() : QString();
    };

    auto resolvedNodeName = [&]() -> QString {
        if (!s.m_meshNodeName.isEmpty()) return s.m_meshNodeName;
        const QString path = s.m_meshIdentityPath.isEmpty()
            ? modemmeshcore::identity::defaultIdentityPath()
            : s.m_meshIdentityPath;
        modemmeshcore::identity::Identity id =
            modemmeshcore::identity::loadOrCreateIdentity(path);
        return modemmeshcore::identity::defaultNodeNameFor(id);
    };

    const quint64 now = static_cast<quint64>(QDateTime::currentSecsSinceEpoch());
    QStringList toks;

    switch (s.m_messageType)
    {
    case MeshcoreModSettings::MessageText:
        // Caller-managed: m_textMessage already carries either a literal or
        // a MESHCORE: command. Encoder's existing path handles both.
        return QString();

    case MeshcoreModSettings::MessageAdvert: {
        const QString seed = loadSeedHex();
        if (seed.isEmpty()) return QString();
        toks << "type=advert"
             << "seed=" + seed
             << "name=" + resolvedNodeName()
             << "ts=" + QString::number(now)
             << "route=flood";
        if (s.m_meshAdvertLocationEnabled) {
            toks << QString("lat=%1").arg(s.m_meshAdvertLat, 0, 'f', 6);
            toks << QString("lon=%1").arg(s.m_meshAdvertLon, 0, 'f', 6);
        }
        break;
    }

    case MeshcoreModSettings::MessageTxtMsg: {
        const QString seed = loadSeedHex();
        if (seed.isEmpty() || s.m_meshDestPubKeyHex.isEmpty()) return QString();
        toks << "type=txt_msg"
             << "seed=" + seed
             << "dest=" + s.m_meshDestPubKeyHex
             << "ts=" + QString::number(now)
             << "text=" + s.m_textMessage;
        break;
    }

    case MeshcoreModSettings::MessageGrpTxt: {
        toks << "type=grp_txt";
        if (!s.m_meshGroupChannelPskHex.isEmpty()) {
            toks << "channel_psk=" + s.m_meshGroupChannelPskHex;
        } else {
            toks << "channel=" + (s.m_meshGroupChannelName.isEmpty()
                                   ? QStringLiteral("public")
                                   : s.m_meshGroupChannelName);
        }
        toks << "ts=" + QString::number(now)
             << "text=" + s.m_textMessage;
        break;
    }

    case MeshcoreModSettings::MessageAnonReq: {
        const QString seed = loadSeedHex();
        if (seed.isEmpty() || s.m_meshDestPubKeyHex.isEmpty()) return QString();
        toks << "type=anon_req"
             << "seed=" + seed
             << "dest=" + s.m_meshDestPubKeyHex
             << "ts=" + QString::number(now)
             << "data=text:" + s.m_textMessage;
        break;
    }

    case MeshcoreModSettings::MessageAck: {
        if (s.m_meshDestPubKeyHex.isEmpty() || s.m_meshAckMsgHashHex.isEmpty()) {
            return QString();
        }
        toks << "type=ack"
             << "dest=" + s.m_meshDestPubKeyHex
             << "msg_hash=" + s.m_meshAckMsgHashHex;
        break;
    }
    }

    if (toks.isEmpty()) return QString();
    return QStringLiteral("MESHCORE: ") + toks.join(QStringLiteral("; "));
}

} // anonymous

const MeshcoreModSettings::CodingScheme MeshcoreModEncoder::m_codingScheme = MeshcoreModSettings::CodingLoRa;
const bool MeshcoreModEncoder::m_hasCRC = true;
const bool MeshcoreModEncoder::m_hasHeader = true;

MeshcoreModEncoder::MeshcoreModEncoder() :
    m_spreadFactor(0),
    m_deBits(0),
    m_nbSymbolBits(5),
    m_nbParityBits(1)
{}

MeshcoreModEncoder::~MeshcoreModEncoder()
{}

void MeshcoreModEncoder::setNbSymbolBits(unsigned int spreadFactor, unsigned int deBits)
{
    m_spreadFactor = spreadFactor;

    if (deBits >= spreadFactor) {
        m_deBits = m_spreadFactor - 1;
    } else {
        m_deBits = deBits;
    }

    m_nbSymbolBits = m_spreadFactor - m_deBits;
}

void MeshcoreModEncoder::encode(MeshcoreModSettings settings, std::vector<unsigned short>& symbols)
{
    if (m_nbSymbolBits < 5) {
        return;
    }

    QByteArray bytes;
    QString summary;
    QString error;

    // Synthesise a MESHCORE: command from m_messageType + identity store +
    // settings fields. For MessageText we fall through to the legacy path
    // which honours either a literal payload or a hand-typed MESHCORE: line
    // in m_textMessage.
    QString command;
    if (settings.m_messageType != MeshcoreModSettings::MessageText) {
        command = buildMeshcoreCommandFromSettings(settings);
    } else if (modemmeshcore::Packet::isCommand(settings.m_textMessage)) {
        command = settings.m_textMessage;
    }

    if (!command.isEmpty())
    {
        if (!modemmeshcore::Packet::buildFrameFromCommand(command, bytes, summary, error))
        {
            qWarning() << "MeshcoreModEncoder::encode: Meshcore command error:"
                       << error << " command=" << command;
            return;
        }
        qInfo() << "MeshcoreModEncoder::encode:" << summary;
    }
    else
    {
        bytes = settings.m_textMessage.toUtf8();
    }

    encodeBytesLoRa(bytes, symbols);
}

void MeshcoreModEncoder::encodeBytes(const QByteArray& bytes, std::vector<unsigned short>& symbols)
{
    switch (m_codingScheme)
    {
    case MeshcoreModSettings::CodingLoRa:
    {
        QByteArray payload(bytes);

        if (modemmeshcore::Packet::isCommand(QString::fromUtf8(bytes)))
        {
            QString summary;
            QString error;

            if (!modemmeshcore::Packet::buildFrameFromCommand(QString::fromUtf8(bytes), payload, summary, error))
            {
                qWarning() << "MeshcoreModEncoder::encodeBytes: Meshcore command error:" << error;
                return;
            }

            qInfo() << "MeshcoreModEncoder::encodeBytes:" << summary;
        }

        encodeBytesLoRa(payload, symbols);
        break;
    }
    default:
        break;
    };
}

void MeshcoreModEncoder::encodeBytesLoRa(const QByteArray& bytes, std::vector<unsigned short>& symbols)
{
    QByteArray payload(bytes);

    if ((payload.size() + (m_hasCRC ? 2 : 0)) > 255)
    {
        qWarning() << "MeshcoreModEncoder::encodeBytesLoRa: payload too large:" << payload.size();
        return;
    }

    if (m_hasCRC) {
        MeshcoreModEncoderLoRa::addChecksum(payload);
    }

    const unsigned int headerNbSymbolBits = (m_hasHeader && (m_spreadFactor > 2U))
        ? (m_spreadFactor - 2U)
        : m_nbSymbolBits;

    MeshcoreModEncoderLoRa::encodeBytes(
        payload,
        symbols,
        m_nbSymbolBits,
        headerNbSymbolBits,
        m_hasHeader,
        m_hasCRC,
        m_nbParityBits
    );
}
