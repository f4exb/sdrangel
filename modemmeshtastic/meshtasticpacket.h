///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
// Copyright (C) 2026 AI (unknown)                                          //
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

#ifndef MODEMMESHTASTIC_MESHTASTICPACKET_H_
#define MODEMMESHTASTIC_MESHTASTICPACKET_H_

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <QVector>

#include <stdint.h>

#include "export.h"

namespace modemmeshtastic
{

struct MODEMMESHTASTIC_API DecodeResult
{
    struct Field
    {
        QString path;
        QString value;
    };

    bool isFrame = false;
    bool dataDecoded = false;
    bool decrypted = false;
    QString keyLabel;
    QString summary;
    QVector<Field> fields;
};

struct MODEMMESHTASTIC_API TxRadioSettings
{
    bool hasCommand = false;
    bool hasLoRaParams = false;
    int bandwidthHz = 0;
    int spreadFactor = 0;
    int parityBits = 0; // 1..4 maps to CR 4/5 .. 4/8
    int deBits = 0;
    uint8_t syncWord = 0x00; // Meshtastic_SDR/gr-lora_sdr reference flow uses [0,0]
    int preambleChirps = 17;

    bool hasCenterFrequency = false;
    qint64 centerFrequencyHz = 0;

    QString summary;
};

class MODEMMESHTASTIC_API Packet
{
public:
    static bool isCommand(const QString& text);

    static bool buildFrameFromCommand(
        const QString& command,
        QByteArray& frame,
        QString& summary,
        QString& error
    );

    static bool decodeFrame(
        const QByteArray& frame,
        DecodeResult& result
    );

    static bool decodeFrame(
        const QByteArray& frame,
        DecodeResult& result,
        const QString& keySpecList
    );

    static bool validateKeySpecList(
        const QString& keySpecList,
        QString& error,
        int* keyCount = nullptr
    );

    static bool deriveTxRadioSettings(
        const QString& command,
        TxRadioSettings& settings,
        QString& error
    );
};

} // namespace modemmeshtastic

#endif // MODEMMESHTASTIC_MESHTASTICPACKET_H_
