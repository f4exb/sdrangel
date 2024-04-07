///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include "chirpchatmodencoderft.h"

#ifndef HAS_FT8
void ChirpChatModEncoderFT::encodeMsg(
    const QString& myCall,
    const QString& urCall,
    const QString& myLocator,
    const QString& myReport,
    const QString& textMessage,
    ChirpChatModSettings::MessageType messageType,
    unsigned int nbSymbolBits,
    std::vector<unsigned short>& symbols
)
{
    qDebug("ChirpChatModEncoderFT::encodeMsg: not implemented");
}
#else

#include "ft8.h"
#include "packing.h"


void ChirpChatModEncoderFT::encodeMsg(
    const QString& myCall,
    const QString& urCall,
    const QString& myLocator,
    const QString& myReport,
    const QString& textMessage,
    ChirpChatModSettings::MessageType messageType,
    unsigned int nbSymbolBits,
    std::vector<unsigned short>& symbols
)
{
    int a174[174]; // FT payload is 174 bits

    if (messageType == ChirpChatModSettings::MessageNone) {
        return; // do nothing
    } else if (messageType == ChirpChatModSettings::MessageBeacon) {
        encodeMsgBeaconOrCQ(myCall, myLocator, "DE", a174);
    } else if (messageType == ChirpChatModSettings::MessageCQ) {
        encodeMsgBeaconOrCQ(myCall, myLocator, "CQ", a174);
    } else if (messageType == ChirpChatModSettings::MessageReply) {
        encodeMsgReply(myCall, urCall, myLocator, a174);
    } else if (messageType == ChirpChatModSettings::MessageReport) {
        encodeMsgReport(myCall, urCall, myReport, 0, a174);
    } else if (messageType == ChirpChatModSettings::MessageReplyReport) {
        encodeMsgReport(myCall, urCall, myReport, 1, a174);
    } else if (messageType == ChirpChatModSettings::MessageRRR) {
        encodeMsgReport(myCall, urCall, "RRR", 1, a174);
    } else if (messageType == ChirpChatModSettings::Message73) {
        encodeMsgReport(myCall, urCall, "73", 1, a174);
    } else {
        encodeTextMsg(textMessage, a174);
    }

    int allBits = ((174 / nbSymbolBits) + (174 % nbSymbolBits == 0 ? 0 : 1))*nbSymbolBits; // ensures zero bits padding
    int iBit;
    int symbol = 0;

    interleave174(a174);

    for (int i = 0; i < allBits; i++)
    {
        iBit = nbSymbolBits - (i % nbSymbolBits) - 1; // MSB first

        if (i < 174) {
            symbol += a174[i] * (1<<iBit);
        }

        if ((i % nbSymbolBits) == (nbSymbolBits - 1))
        {
            symbol = symbol ^ (symbol >> 1); // Gray code
            symbols.push_back(symbol);
            symbol = 0;
        }
    }
}

void ChirpChatModEncoderFT::encodeTextMsg(const QString& text, int a174[])
{
    int a77[77];
    std::fill(a77, a77 + 77, 0);
    QString sentMsg = text.rightJustified(13, ' ', true);

    if (!FT8::Packing::packfree(a77, sentMsg.toUpper().toStdString()))
    {
        qDebug("ChirpChatModEncoderFT::encodeTextMsg: failed to encode free text message (%s)", qPrintable(sentMsg));
        return;
    }

    FT8::FT8::encode(a174, a77);
}

void ChirpChatModEncoderFT::encodeMsgBeaconOrCQ(const QString& myCall, const QString& myLocator, const QString& shorthand, int a174[])
{
    int c28_1, c28_2, g15;

    if (!FT8::Packing::packcall_std(c28_1, shorthand.toUpper().toStdString())) //
    {
        qDebug("ChirpChatModEncoderFT::encodeMsgBeaconOrCQ: failed to encode call1 (%s)", qPrintable(shorthand));
        return;
    }

    if (!FT8::Packing::packcall_std(c28_2, myCall.toUpper().toStdString()))
    {
        qDebug("ChirpChatModEncoderFT::encodeMsgBeaconOrCQ: failed to encode call2 (%s)", qPrintable(myCall));
        return;
    }

    if (myLocator.size() < 4)
    {
        qDebug("ChirpChatModEncoderFT::encodeMsgBeaconOrCQ: locator invalid (%s)", qPrintable(myLocator));
        return;
    }

    if (!FT8::Packing::packgrid(g15, myLocator.left(4).toUpper().toStdString()))
    {
        qDebug("ChirpChatModEncoderFT::encodeMsgBeaconOrCQ: failed to encode locator (%s)", qPrintable(myLocator));
        return;
    }

    int a77[77];
    std::fill(a77, a77 + 77, 0);
    FT8::Packing::pack1(a77, c28_1, c28_2, g15, 0);
    FT8::FT8::encode(a174, a77);
}

void ChirpChatModEncoderFT::encodeMsgReply(const QString& myCall, const QString& urCall, const QString& myLocator, int a174[])
{
    int c28_1, c28_2, g15;

    if (!FT8::Packing::packcall_std(c28_1, urCall.toUpper().toStdString())) //
    {
        qDebug("ChirpChatModEncoderFT::encodeMsgReply: failed to encode call1 (%s)", qPrintable(urCall));
        return;
    }

    if (!FT8::Packing::packcall_std(c28_2, myCall.toUpper().toStdString()))
    {
        qDebug("ChirpChatModEncoderFT::encodeMsgReply: failed to encode call2 (%s)", qPrintable(myCall));
        return;
    }

    if (myLocator.size() < 4)
    {
        qDebug("ChirpChatModEncoderFT::encodeMsgReply: locator invalid (%s)", qPrintable(myLocator));
        return;
    }

    if (!FT8::Packing::packgrid(g15, myLocator.left(4).toUpper().toStdString()))
    {
        qDebug("ChirpChatModEncoderFT::encodeMsgReply: failed to encode locator (%s)", qPrintable(myLocator));
        return;
    }

    int a77[77];
    std::fill(a77, a77 + 77, 0);
    FT8::Packing::pack1(a77, c28_1, c28_2, g15, 0);
    FT8::FT8::encode(a174, a77);
}

void ChirpChatModEncoderFT::encodeMsgReport(const QString& myCall, const QString& urCall, const QString& myReport, int reply, int a174[])
{
    int c28_1, c28_2, g15;

    if (!FT8::Packing::packcall_std(c28_1, urCall.toUpper().toStdString())) //
    {
        qDebug("ChirpChatModEncoderFT::encodeMsgReport: failed to encode call1 (%s)", qPrintable(urCall));
        return;
    }

    if (!FT8::Packing::packcall_std(c28_2, myCall.toUpper().toStdString()))
    {
        qDebug("ChirpChatModEncoderFT::encodeMsgReport: failed to encode call2 (%s)", qPrintable(myCall));
        return;
    }

    if (!FT8::Packing::packgrid(g15, myReport.toUpper().toStdString()))
    {
        qDebug("ChirpChatModEncoderFT::encodeMsgReport: failed to encode report (%s)", qPrintable(myReport));
        return;
    }

    int a77[77];
    std::fill(a77, a77 + 77, 0);
    FT8::Packing::pack1(a77, c28_1, c28_2, g15, reply);
    FT8::FT8::encode(a174, a77);
}

void ChirpChatModEncoderFT::encodeMsgFinish(const QString& myCall, const QString& urCall, const QString& shorthand, int a174[])
{
    int c28_1, c28_2, g15;

    if (!FT8::Packing::packcall_std(c28_1, urCall.toUpper().toStdString())) //
    {
        qDebug("ChirpChatModEncoderFT::encodeMsgFinish: failed to encode call1 (%s)", qPrintable(urCall));
        return;
    }

    if (!FT8::Packing::packcall_std(c28_2, myCall.toUpper().toStdString()))
    {
        qDebug("ChirpChatModEncoderFT::encodeMsgFinish: failed to encode call2 (%s)", qPrintable(myCall));
        return;
    }

    if (!FT8::Packing::packgrid(g15, shorthand.toUpper().toStdString()))
    {
        qDebug("ChirpChatModEncoderFT::encodeMsgFinish: failed to encode shorthand (%s)", qPrintable(shorthand));
        return;
    }

    int a77[77];
    std::fill(a77, a77 + 77, 0);
    FT8::Packing::pack1(a77, c28_1, c28_2, g15, 0);
    FT8::FT8::encode(a174, a77);
}

void ChirpChatModEncoderFT::interleave174(int a174[])
{
    // 174 = 2*3*29
    int t174[174];
    std::copy(a174, a174+174, t174);

    for (int i = 0; i < 174; i++) {
        a174[i] = t174[(i%6)*29 + (i%29)];
    }
}

#endif // HAS_FT8
