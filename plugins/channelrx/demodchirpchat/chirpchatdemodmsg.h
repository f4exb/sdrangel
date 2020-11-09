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

#ifndef INCLUDE_CHIRPCHATDEMODMSG_H
#define INCLUDE_CHIRPCHATDEMODMSG_H

#include <QObject>
#include "util/message.h"

namespace ChirpChatDemodMsg
{
    class MsgDecodeSymbols : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const std::vector<unsigned short>& getSymbols() const { return m_symbols; }
        unsigned int getSyncWord() const { return m_syncWord; }
        float getSingalDb() const { return m_signalDb; }
        float getNoiseDb() const { return m_noiseDb; }

        void pushBackSymbol(unsigned short symbol) {
            m_symbols.push_back(symbol);
        }
        void popSymbol() {
            m_symbols.pop_back();
        }
        void setSyncWord(unsigned char syncWord) {
            m_syncWord = syncWord;
        }
        void setSignalDb(float db) {
            m_signalDb = db;
        }
        void setNoiseDb(float db) {
            m_noiseDb = db;
        }

        static MsgDecodeSymbols* create() {
            return new MsgDecodeSymbols();
        }
        static MsgDecodeSymbols* create(const std::vector<unsigned short> symbols) {
            return new MsgDecodeSymbols(symbols);
        }

    private:
        std::vector<unsigned short> m_symbols;
        unsigned int m_syncWord;
        float m_signalDb;
        float m_noiseDb;

        MsgDecodeSymbols() : //!< create an empty message
            Message(),
            m_syncWord(0),
            m_signalDb(0.0),
            m_noiseDb(0.0)
        {}
        MsgDecodeSymbols(const std::vector<unsigned short> symbols) : //!< create a message with symbols copy
            Message(),
            m_syncWord(0),
            m_signalDb(0.0),
            m_noiseDb(0.0)
        { m_symbols = symbols; }
    };
}

#endif // INCLUDE_CHIRPCHATDEMODMSG_H
