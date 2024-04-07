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

#ifndef PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODENCODEFT_H_
#define PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODENCODEFT_H_

#include <vector>
#include <QString>

#include "chirpchatmodsettings.h"

class ChirpChatModEncoderFT
{
public:
    static void encodeMsg(
        const QString& myCall,
        const QString& urCall,
        const QString& myLocator,
        const QString& myReport,
        const QString& textMessage,
        ChirpChatModSettings::MessageType messageType,
        unsigned int nbSymbolBits,
        std::vector<unsigned short>& symbols
    );

private:
    static void encodeTextMsg(const QString& text, int a174[]);
    static void encodeMsgBeaconOrCQ(const QString& myCall, const QString& myLocator, const QString& shorthand, int a174[]);
    static void encodeMsgReply(const QString& myCall, const QString& urCall, const QString& myLocator, int a174[]);
    static void encodeMsgReport(const QString& myCall, const QString& urCall, const QString& myReport, int reply, int a174[]);
    static void encodeMsgFinish(const QString& myCall, const QString& urCall, const QString& shorthand, int a174[]);
    static void interleave174(int a174[]);
};

#endif // PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODENCODEFT_H_
