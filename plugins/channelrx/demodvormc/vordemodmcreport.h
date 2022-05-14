///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_VORDEMODREPORT_H
#define INCLUDE_VORDEMODREPORT_H

#include <QObject>

#include "util/message.h"

class VORDemodMCReport : public QObject
{
    Q_OBJECT
public:
    class MsgReportRadial : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSubChannelId() const { return m_subChannelId; }
        float getRadial() const { return m_radial; }
        float getRefMag() const { return m_refMag; }
        float getVarMag() const { return m_varMag; }

        static MsgReportRadial* create(int subChannelId, float radial, float refMag, float varMag)
        {
            return new MsgReportRadial(subChannelId, radial, refMag, varMag);
        }

    private:
        int m_subChannelId;
        float m_radial;
        float m_refMag;
        float m_varMag;

        MsgReportRadial(int subChannelId, float radial, float refMag, float varMag) :
            Message(),
            m_subChannelId(subChannelId),
            m_radial(radial),
            m_refMag(refMag),
            m_varMag(varMag)
        {
        }
    };

    class MsgReportFreqOffset : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSubChannelId() const { return m_subChannelId; }
        int getFreqOffset() const { return m_freqOffset; }
        bool getOutOfBand() const { return m_outOfBand; }

        static MsgReportFreqOffset* create(int subChannelId, int freqOffset, bool outOfBand)
        {
            return new MsgReportFreqOffset(subChannelId, freqOffset, outOfBand);
        }

    private:
        int m_subChannelId;
        int m_freqOffset;
        bool m_outOfBand;

        MsgReportFreqOffset(int subChannelId, int freqOffset, bool outOfBand) :
            Message(),
            m_subChannelId(subChannelId),
            m_freqOffset(freqOffset),
            m_outOfBand(outOfBand)
        {
        }
    };

    class MsgReportIdent : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSubChannelId() const { return m_subChannelId; }
        QString getIdent() const { return m_ident; }

        static MsgReportIdent* create(int subChannelId, QString ident)
        {
            return new MsgReportIdent(subChannelId, ident);
        }

    private:
        int m_subChannelId;
        QString m_ident;

        MsgReportIdent(int subChannelId, QString ident) :
            Message(),
            m_subChannelId(subChannelId),
            m_ident(ident)
        {
        }
    };

public:
    VORDemodMCReport() {}
    ~VORDemodMCReport() {}
};

#endif // INCLUDE_VORDEMODREPORT_H
