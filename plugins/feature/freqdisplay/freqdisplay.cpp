///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by Copilot                                                          //
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

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFreqDisplaySettings.h"
#include "SWGFreqDisplayReport.h"

#include "channel/channelwebapiutils.h"
#include "maincore.h"
#include "settings/serializable.h"

#include "freqdisplay.h"

MESSAGE_CLASS_DEFINITION(FreqDisplay::MsgConfigureFreqDisplay, Message)

const char* const FreqDisplay::m_featureIdURI = "sdrangel.feature.freqdisplay";
const char* const FreqDisplay::m_featureId = "FreqDisplay";

FreqDisplay::FreqDisplay(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface)
{
    qDebug("FreqDisplay::FreqDisplay: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    setState(StIdle);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &FreqDisplay::networkManagerFinished
    );
}

FreqDisplay::~FreqDisplay()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &FreqDisplay::networkManagerFinished
    );
    delete m_networkManager;
}

bool FreqDisplay::handleMessage(const Message& cmd)
{
    if (MsgConfigureFreqDisplay::match(cmd))
    {
        MsgConfigureFreqDisplay& cfg = (MsgConfigureFreqDisplay&) cmd;
        qDebug() << "FreqDisplay::handleMessage: MsgConfigureFreqDisplay";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());
        return true;
    }

    return false;
}

QByteArray FreqDisplay::serialize() const
{
    return m_settings.serialize();
}

bool FreqDisplay::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        auto *msg = MsgConfigureFreqDisplay::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        auto *msg = MsgConfigureFreqDisplay::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void FreqDisplay::applySettings(const FreqDisplaySettings& settings, const QStringList& settingsKeys, bool force)
{
    qDebug() << "FreqDisplay::applySettings:" << " force: " << force;

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
                settingsKeys.contains("reverseAPIAddress") ||
                settingsKeys.contains("reverseAPIPort") ||
                settingsKeys.contains("reverseAPIFeatureSetIndex") ||
                settingsKeys.contains("reverseAPIFeatureIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

int FreqDisplay::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setFreqDisplaySettings(new SWGSDRangel::SWGFreqDisplaySettings());
    response.getFreqDisplaySettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int FreqDisplay::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    FreqDisplaySettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureFreqDisplay *msg = MsgConfigureFreqDisplay::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    qDebug("FreqDisplay::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue)
    {
        MsgConfigureFreqDisplay *msgToGUI = MsgConfigureFreqDisplay::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);
    return 200;
}

int FreqDisplay::webapiReportGet(
    SWGSDRangel::SWGFeatureReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setFreqDisplayReport(new SWGSDRangel::SWGFreqDisplayReport());
    response.getFreqDisplayReport()->init();
    webapiFormatFeatureReport(response);
    return 200;
}

void FreqDisplay::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const FreqDisplaySettings& settings)
{
    if (response.getFreqDisplaySettings()->getTitle()) {
        *response.getFreqDisplaySettings()->getTitle() = settings.m_title;
    } else {
        response.getFreqDisplaySettings()->setTitle(new QString(settings.m_title));
    }

    response.getFreqDisplaySettings()->setRgbColor(settings.m_rgbColor);

    if (response.getFreqDisplaySettings()->getSelectedChannel()) {
        *response.getFreqDisplaySettings()->getSelectedChannel() = settings.m_selectedChannel;
    } else {
        response.getFreqDisplaySettings()->setSelectedChannel(new QString(settings.m_selectedChannel));
    }

    if (response.getFreqDisplaySettings()->getFontName()) {
        *response.getFreqDisplaySettings()->getFontName() = settings.m_fontName;
    } else {
        response.getFreqDisplaySettings()->setFontName(new QString(settings.m_fontName));
    }

    response.getFreqDisplaySettings()->setTransparentBackground(settings.m_transparentBackground ? 1 : 0);
    response.getFreqDisplaySettings()->setDisplayMode(static_cast<int>(settings.m_displayMode));
    response.getFreqDisplaySettings()->setSpeechEnabled(settings.m_speechEnabled ? 1 : 0);
    response.getFreqDisplaySettings()->setFrequencyUnits(static_cast<int>(settings.m_frequencyUnits));
    response.getFreqDisplaySettings()->setShowUnits(settings.m_showUnits ? 1 : 0);
    response.getFreqDisplaySettings()->setFreqDecimalPlaces(settings.m_freqDecimalPlaces);
    response.getFreqDisplaySettings()->setPowerDecimalPlaces(settings.m_powerDecimalPlaces);
    response.getFreqDisplaySettings()->setTextcolor(settings.m_textColor.rgba());
    response.getFreqDisplaySettings()->setDropShadowEnabled(settings.m_dropShadowEnabled ? 1 : 0);
    response.getFreqDisplaySettings()->setDropShadowColor(settings.m_dropShadowColor.rgba());
    response.getFreqDisplaySettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getFreqDisplaySettings()->getReverseApiAddress()) {
        *response.getFreqDisplaySettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getFreqDisplaySettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getFreqDisplaySettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getFreqDisplaySettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getFreqDisplaySettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getFreqDisplaySettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getFreqDisplaySettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getFreqDisplaySettings()->setRollupState(swgRollupState);
        }
    }
}

void FreqDisplay::webapiUpdateFeatureSettings(
    FreqDisplaySettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getFreqDisplaySettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getFreqDisplaySettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("selectedChannel")) {
        settings.m_selectedChannel = *response.getFreqDisplaySettings()->getSelectedChannel();
    }
    if (featureSettingsKeys.contains("fontName")) {
        settings.m_fontName = *response.getFreqDisplaySettings()->getFontName();
    }
    if (featureSettingsKeys.contains("transparentBackground")) {
        settings.m_transparentBackground = response.getFreqDisplaySettings()->getTransparentBackground() != 0;
    }
    if (featureSettingsKeys.contains("displayMode")) {
        settings.m_displayMode = static_cast<FreqDisplaySettings::DisplayMode>(response.getFreqDisplaySettings()->getDisplayMode());
    }
    if (featureSettingsKeys.contains("speechEnabled")) {
        settings.m_speechEnabled = response.getFreqDisplaySettings()->getSpeechEnabled() != 0;
    }
    if (featureSettingsKeys.contains("frequencyUnits")) {
        settings.m_frequencyUnits = static_cast<FreqDisplaySettings::FrequencyUnits>(response.getFreqDisplaySettings()->getFrequencyUnits());
    }
    if (featureSettingsKeys.contains("showUnits")) {
        settings.m_showUnits = response.getFreqDisplaySettings()->getShowUnits() != 0;
    }
    if (featureSettingsKeys.contains("freqDecimalPlaces")) {
        settings.m_freqDecimalPlaces = response.getFreqDisplaySettings()->getFreqDecimalPlaces();
    }
    if (featureSettingsKeys.contains("powerDecimalPlaces")) {
        settings.m_powerDecimalPlaces = response.getFreqDisplaySettings()->getPowerDecimalPlaces();
    }
    if (featureSettingsKeys.contains("textColor")) {
        settings.m_textColor = QColor::fromRgba(response.getFreqDisplaySettings()->getTextcolor());
    }
    if (featureSettingsKeys.contains("dropShadowEnabled")) {
        settings.m_dropShadowEnabled = response.getFreqDisplaySettings()->getDropShadowEnabled() != 0;
    }
    if (featureSettingsKeys.contains("dropShadowColor")) {
        settings.m_dropShadowColor = QColor::fromRgba(response.getFreqDisplaySettings()->getDropShadowColor());
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getFreqDisplaySettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getFreqDisplaySettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getFreqDisplaySettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getFreqDisplaySettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getFreqDisplaySettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getFreqDisplaySettings()->getRollupState());
    }
}

void FreqDisplay::webapiReverseSendSettings(const QStringList& featureSettingsKeys, const FreqDisplaySettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    swgFeatureSettings->setFeatureType(new QString("FreqDisplay"));
    swgFeatureSettings->setFreqDisplaySettings(new SWGSDRangel::SWGFreqDisplaySettings());
    SWGSDRangel::SWGFreqDisplaySettings *swgFreqDisplaySettings = swgFeatureSettings->getFreqDisplaySettings();

    if (featureSettingsKeys.contains("title") || force) {
        swgFreqDisplaySettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgFreqDisplaySettings->setRgbColor(settings.m_rgbColor);
    }
    if (featureSettingsKeys.contains("selectedChannel") || force) {
        swgFreqDisplaySettings->setSelectedChannel(new QString(settings.m_selectedChannel));
    }
    if (featureSettingsKeys.contains("fontName") || force) {
        swgFreqDisplaySettings->setFontName(new QString(settings.m_fontName));
    }
    if (featureSettingsKeys.contains("transparentBackground") || force) {
        swgFreqDisplaySettings->setTransparentBackground(settings.m_transparentBackground ? 1 : 0);
    }
    if (featureSettingsKeys.contains("displayMode") || force) {
        swgFreqDisplaySettings->setDisplayMode(static_cast<int>(settings.m_displayMode));
    }
    if (featureSettingsKeys.contains("speechEnabled") || force) {
        swgFreqDisplaySettings->setSpeechEnabled(settings.m_speechEnabled ? 1 : 0);
    }
    if (featureSettingsKeys.contains("frequencyUnits") || force) {
        swgFreqDisplaySettings->setFrequencyUnits(static_cast<int>(settings.m_frequencyUnits));
    }
    if (featureSettingsKeys.contains("showUnits") || force) {
        swgFreqDisplaySettings->setShowUnits(settings.m_showUnits ? 1 : 0);
    }
    if (featureSettingsKeys.contains("freqDecimalPlaces") || force) {
        swgFreqDisplaySettings->setFreqDecimalPlaces(settings.m_freqDecimalPlaces);
    }
    if (featureSettingsKeys.contains("powerDecimalPlaces") || force) {
        swgFreqDisplaySettings->setPowerDecimalPlaces(settings.m_powerDecimalPlaces);
    }
    if (featureSettingsKeys.contains("textColor") || force) {
        swgFreqDisplaySettings->setTextcolor(settings.m_textColor.rgba());
    }
    if (featureSettingsKeys.contains("dropShadowEnabled") || force) {
        swgFreqDisplaySettings->setDropShadowEnabled(settings.m_dropShadowEnabled ? 1 : 0);
    }
    if (featureSettingsKeys.contains("dropShadowColor") || force) {
        swgFreqDisplaySettings->setDropShadowColor(settings.m_dropShadowColor.rgba());
    }

    QString featureSettingsURL = QString("http://%1:%2/sdrangel/featureset/%3/feature/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIFeatureSetIndex)
            .arg(settings.m_reverseAPIFeatureIndex);
    m_networkRequest.setUrl(QUrl(featureSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgFeatureSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgFeatureSettings;
}

void FreqDisplay::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response)
{
    qint64 frequency = 0;
    float power = 0.0f;

    unsigned int superIndex = 0;
    unsigned int channelIndex = 0;
    if (MainCore::getDeviceAndChannelIndexFromId(m_settings.m_selectedChannel, superIndex, channelIndex))
    {
        double centerFrequencyHz = 0.0;
        int offsetHz = 0;
        if (ChannelWebAPIUtils::getCenterFrequency(superIndex, centerFrequencyHz) &&
            ChannelWebAPIUtils::getFrequencyOffset(superIndex, channelIndex, offsetHz))
        {
            frequency = qRound64(centerFrequencyHz) + static_cast<qint64>(offsetHz);
        }

        double powerDb = 0.0;
        if (ChannelWebAPIUtils::getChannelReportValue(superIndex, channelIndex, "channelPowerDB", powerDb)) {
            power = static_cast<float>(powerDb);
        }
    }

    response.getFreqDisplayReport()->setFrequency(frequency);
    response.getFreqDisplayReport()->setPower(power);
}

void FreqDisplay::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "FreqDisplay::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("FreqDisplay::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
