///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_CHANALYZER_H
#define INCLUDE_CHANALYZER_H

#include <QMutex>
#include <QThread>
#include <QNetworkRequest>

#include <vector>

#include "dsp/basebandsamplesink.h"
#include "dsp/spectrumvis.h"
#include "dsp/scopevis.h"
#include "channel/channelapi.h"
#include "util/message.h"
#include "util/movingaverage.h"

#include "chanalyzerbaseband.h"

class DownChannelizer;
class QNetworkReply;
class QNetworkAccessManager;
class ObjectPipe;
class ChannelAnalyzer : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureChannelAnalyzer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ChannelAnalyzerSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureChannelAnalyzer* create(const ChannelAnalyzerSettings& settings, bool force) {
            return new MsgConfigureChannelAnalyzer(settings, force);
        }

    private:
        ChannelAnalyzerSettings m_settings;
        bool m_force;

        MsgConfigureChannelAnalyzer(const ChannelAnalyzerSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    ChannelAnalyzer(DeviceAPI *deviceAPI);
	virtual ~ChannelAnalyzer();
	virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }
    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    ScopeVis *getScopeVis() { return &m_scopeVis; }
    void setScopeVis(ScopeVis *scopeVis) { m_basebandSink->setScopeVis(scopeVis); }
    int getChannelSampleRate();
    int getDecimation() const { return 1<<m_settings.m_log2Decim; }
	double getMagSq() const { return m_basebandSink->getMagSq(); }
	double getMagSqAvg() const { return m_basebandSink->getMagSqAvg(); }
	bool isPllLocked() const { return m_basebandSink->isPllLocked(); }
    Real getPllFrequency() const { return m_basebandSink->getPllFrequency(); }
	Real getPllDeltaPhase() const { return m_basebandSink->getPllDeltaPhase(); }
    Real getPllPhase() const { return m_basebandSink->getPllPhase(); }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) { title = objectName(); }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const { return QByteArray(); }
    virtual bool deserialize(const QByteArray& data) { (void) data; return false; }

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }
    uint32_t getNumberOfDeviceStreams() const;

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiWorkspaceGet(
            SWGSDRangel::SWGWorkspaceInfo& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const ChannelAnalyzerSettings& settings);

    static void webapiUpdateChannelSettings(
            ChannelAnalyzerSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
	DeviceAPI *m_deviceAPI;
    QThread m_thread;
    ChannelAnalyzerBaseband *m_basebandSink;
    ChannelAnalyzerSettings m_settings;
    SpectrumVis m_spectrumVis;
    ScopeVis m_scopeVis;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency; //!< stored from device message used when starting baseband sink

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

	virtual bool handleMessage(const Message& cmd);
	void applySettings(const ChannelAnalyzerSettings& settings, bool force = false);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const ChannelAnalyzerSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const ChannelAnalyzerSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const ChannelAnalyzerSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_CHANALYZER_H
