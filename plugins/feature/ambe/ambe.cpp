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

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"

#include "settings/serializable.h"
#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"

#include "ambe.h"

MESSAGE_CLASS_DEFINITION(AMBE::MsgConfigureAMBE, Message)

const char* const AMBE::m_featureIdURI = "sdrangel.feature.ambe";
const char* const AMBE::m_featureId = "AMBE";

AMBE::AMBE(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface)
{
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "AMBE error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AMBE::networkManagerFinished
    );
}

AMBE::~AMBE()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AMBE::networkManagerFinished
    );
    delete m_networkManager;
}

void AMBE::start()
{
	qDebug("AMBE::start");
    m_state = StRunning;
}

void AMBE::stop()
{
    qDebug("AMBE::stop");
    m_state = StIdle;
}

void AMBE::applySettings(const AMBESettings& settings, bool force)
{
    (void) force;
    m_settings = settings;
}

bool AMBE::handleMessage(const Message& cmd)
{
	if (MsgConfigureAMBE::match(cmd))
	{
        MsgConfigureAMBE& cfg = (MsgConfigureAMBE&) cmd;
        qDebug() << "AMBE::handleMessage: MsgConfigureAMBE";
        applySettings(cfg.getSettings(), cfg.getForce());
		return true;
	}
    else if (DSPPushMbeFrame::match(cmd))
    {
        DSPPushMbeFrame& cfg = (DSPPushMbeFrame&) cmd;
        m_ambeEngine.pushMbeFrame(
            cfg.getMbeFrame(),
            cfg.getMbeRateIndex(),
            cfg.getMbeVolumeIndex(),
            cfg.getChannels(),
            cfg.getUseHP(),
            cfg.getUpsampling(),
            cfg.getAudioFifo()
        );
        return true;
    }

    return false;
}

QByteArray AMBE::serialize() const
{
    SimpleSerializer s(1);
    s.writeBlob(1, m_settings.serialize());
    return s.final();
}

bool AMBE::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        m_settings.resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        d.readBlob(1, &bytetmp);

        if (m_settings.deserialize(bytetmp))
        {
            MsgConfigureAMBE *msg = MsgConfigureAMBE::create(m_settings, true);
            m_inputMessageQueue.push(msg);
            return true;
        }
        else
        {
            m_settings.resetToDefaults();
            MsgConfigureAMBE *msg = MsgConfigureAMBE::create(m_settings, true);
            m_inputMessageQueue.push(msg);
            return false;
        }
    }
    else
    {
        return false;
    }
}

void AMBE::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AMBE::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AMBE::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
