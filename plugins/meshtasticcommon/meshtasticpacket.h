///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026                                                           //
// SPDX-License-Identifier: GPL-3.0-or-later                                   //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHIRPCHATCOMMON_MESHTASTICPACKET_H_
#define PLUGINS_CHIRPCHATCOMMON_MESHTASTICPACKET_H_

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <QVector>

#include <stdint.h>

namespace Meshtastic
{

struct DecodeResult
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

struct TxRadioSettings
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

class Packet
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

} // namespace Meshtastic

#endif // PLUGINS_CHIRPCHATCOMMON_MESHTASTICPACKET_H_
