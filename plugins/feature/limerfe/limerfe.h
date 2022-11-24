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

#ifndef INCLUDE_FEATURE_LIMERFE_H_
#define INCLUDE_FEATURE_LIMERFE_H_

#include <QNetworkRequest>

#include <string>
#include <map>
#include "lime/limeRFE.h"

#include "feature/feature.h"
#include "util/message.h"

#include "limerfesettings.h"

class QNetworkReply;
class QNetworkAccessManager;

class LimeRFE : public Feature
{
	Q_OBJECT
public:
    class MsgConfigureLimeRFE : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const LimeRFESettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureLimeRFE* create(const LimeRFESettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureLimeRFE(settings, settingsKeys, force);
        }

    private:
        LimeRFESettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureLimeRFE(const LimeRFESettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgReportSetRx : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool isOn() const { return m_on; }

        static MsgReportSetRx* create(bool on) {
            return new MsgReportSetRx(on);
        }

    private:
        bool m_on;

        MsgReportSetRx(bool on) :
            Message(),
            m_on(on)
        { }
    };

    class MsgReportSetTx : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool isOn() const { return m_on; }

        static MsgReportSetTx* create(bool on) {
            return new MsgReportSetTx(on);
        }

    private:
        bool m_on;

        MsgReportSetTx(bool on) :
            Message(),
            m_on(on)
        { }
    };

    LimeRFE(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~LimeRFE();
    virtual void destroy() { delete this; }
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) const { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) const { title = m_settings.m_title; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

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
        const LimeRFESettings& settings);

    static void webapiUpdateFeatureSettings(
            LimeRFESettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    int openDevice(const std::string& serialDeviceName);
    void closeDevice();

    const QStringList& getComPorts() { return m_comPorts; }
    int configure();
    int getState();
    static std::string getError(int errorCode);
    int setRx(bool rxOn);
    int setTx(bool txOn);
    bool getRx() const { return m_rxOn; };
    bool getTx() const { return m_txOn; };
    bool turnDevice(int deviceSetIndex, bool on);
    int getFwdPower(int& powerDB);
    int getRefPower(int& powerDB);

    void settingsToState(const LimeRFESettings& settings);
    void stateToSettings(LimeRFESettings& settings, QList<QString>& settingsKeys);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    LimeRFESettings m_settings;
    bool m_rxOn;
    bool m_txOn;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    WebAPIAdapterInterface *m_webAPIAdapterInterface;

    rfe_dev_t *m_rfeDevice;
    rfe_boardState m_rfeBoardState;
    static const std::map<int, std::string> m_errorCodesMap;
    QStringList m_comPorts;

    void start();
    void stop();
    void listComPorts();
    void applySettings(const LimeRFESettings& settings, const QList<QString>& settingsKeys, bool force = false);
    int webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response, QString& errorMessage);

private slots:
    void networkManagerFinished(QNetworkReply *reply);

};

#endif
