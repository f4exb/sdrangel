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

#ifndef INCLUDE_VORLOCALIZERREPORT_H
#define INCLUDE_VORLOCALIZERREPORT_H

#include <QObject>
#include <QHash>

#include "util/message.h"

class VORLocalizerReport : public QObject
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
        bool getValidRadial() const { return m_validRadial; }
        bool getValidRefMag() const { return m_validRefMag; }
        bool getValidVarMag() const { return m_validVarMag; }

        static MsgReportRadial* create(
            int subChannelId,
            float radial,
            float refMag,
            float varMag,
            bool validRadial,
            bool validRefMag,
            bool validVarMag
        )
        {
            return new MsgReportRadial(
                subChannelId,
                radial,
                refMag,
                varMag,
                validRadial,
                validRefMag,
                validVarMag
            );
        }

    private:
        int m_subChannelId;
        float m_radial;
        float m_refMag;
        float m_varMag;
        bool m_validRadial;
        bool m_validRefMag;
        bool m_validVarMag;

        MsgReportRadial(
            int subChannelId,
            float radial,
            float refMag,
            float varMag,
            bool validRadial,
            bool validRefMag,
            bool validVarMag
        ) :
            Message(),
            m_subChannelId(subChannelId),
            m_radial(radial),
            m_refMag(refMag),
            m_varMag(varMag),
            m_validRadial(validRadial),
            m_validRefMag(validRefMag),
            m_validVarMag(validVarMag)
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

    class MsgReportChannels : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        struct Channel {
            int m_deviceSetIndex;
            int m_channelIndex;
        };

        std::vector<Channel>& getChannels() { return m_channels; }

        static MsgReportChannels* create() {
            return new MsgReportChannels();
        }

    private:
        std::vector<Channel> m_channels;

        MsgReportChannels() :
            Message()
        {}
    };

    class MsgReportServiceddVORs : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        std::vector<int>& getNavIds() { return m_navIds; }
        QHash<int, bool>& getSinglePlans() { return m_singlePlans; }

        static MsgReportServiceddVORs* create() {
            return new MsgReportServiceddVORs();
        }

    private:
        std::vector<int> m_navIds;
        QHash<int, bool> m_singlePlans;

        MsgReportServiceddVORs() :
            Message()
        {}
    };

public:
    VORLocalizerReport() {}
    ~VORLocalizerReport() {}
};

#endif // INCLUDE_VORLOCALIZERREPORT_H
