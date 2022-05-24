///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_AMBE_H_
#define INCLUDE_FEATURE_AMBE_H_

#include <QNetworkRequest>
#include <QSet>

#include "feature/feature.h"
#include "util/message.h"

#include "ambeengine.h"
#include "ambesettings.h"

class WebAPIAdapterInterface;
class QNetworkAccessManager;
class QNetworkReply;

class AMBE : public Feature
{
	Q_OBJECT
public:
    class MsgConfigureAMBE : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AMBESettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureAMBE* create(const AMBESettings& settings, bool force) {
            return new MsgConfigureAMBE(settings, force);
        }

    private:
        AMBESettings m_settings;
        bool m_force;

        MsgConfigureAMBE(const AMBESettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    AMBE(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~AMBE();
    virtual void destroy() { delete this; }
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) const { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) const { title = m_settings.m_title; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    AMBEEngine *getAMBEEngine() { return &m_ambeEngine; }

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    AMBESettings m_settings;
    AMBEEngine m_ambeEngine;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const AMBESettings& settings, bool force = false);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif
