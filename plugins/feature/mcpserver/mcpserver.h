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

#ifndef INCLUDE_FEATURE_MCPSERVER_H_
#define INCLUDE_FEATURE_MCPSERVER_H_

#include <QNetworkRequest>

#include "feature/feature.h"
#include "util/message.h"

#include "mcpserversettings.h"
#include "mcpstreams.h"

class WebAPIAdapterInterface;
class QNetworkAccessManager;
class QNetworkReply;
class MCPProtocol;
class MCPRequestHandler;
class MCPNotifier;

namespace qtwebapp {
    class HttpListener;
}

namespace SWGSDRangel {
    class SWGDeviceState;
}

// Feature exposing SDRangel to AI agents through the Model Context Protocol (MCP)
// using the Streamable HTTP transport.
class MCPServer : public Feature
{
	Q_OBJECT
public:
    class MsgConfigureMCPServer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const MCPServerSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureMCPServer* create(const MCPServerSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureMCPServer(settings, settingsKeys, force);
        }

    private:
        MCPServerSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureMCPServer(const MCPServerSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    MCPServer(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~MCPServer();
    virtual void destroy() { delete this; }
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) const { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) const { title = m_settings.m_title; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int webapiRun(bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGFeatureReport& response,
            QString& errorMessage);

    virtual int webapiActionsPost(
            const QStringList& featureActionsKeys,
            SWGSDRangel::SWGFeatureActions& query,
            QString& errorMessage);

    static void webapiFormatFeatureSettings(
        SWGSDRangel::SWGFeatureSettings& response,
        const MCPServerSettings& settings);

    static void webapiUpdateFeatureSettings(
            MCPServerSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    // Status for the GUI
    int getRequestCount() const;
    int getStreamCount() const;
    QString getLastRequest() const;
    QString getServerURL() const;

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    MCPServerSettings m_settings;
    MCPStreams m_streams;
    MCPProtocol *m_protocol;
    MCPRequestHandler *m_requestHandler;
    MCPNotifier *m_notifier;
    qtwebapp::HttpListener *m_listener;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const MCPServerSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const MCPServerSettings& settings, bool force);
    void webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_FEATURE_MCPSERVER_H_
