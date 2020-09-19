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

#include <QDebug>

#include "dsp/dspengine.h"

#include "simplepttworker.h"
#include "simpleptt.h"

MESSAGE_CLASS_DEFINITION(SimplePTT::MsgConfigureSimplePTT, Message)
MESSAGE_CLASS_DEFINITION(SimplePTT::MsgPTT, Message)
MESSAGE_CLASS_DEFINITION(SimplePTT::MsgStartStop, Message)

const QString SimplePTT::m_featureIdURI = "sdrangel.feature.simpleptt";
const QString SimplePTT::m_featureId = "SimplePTT";

SimplePTT::SimplePTT(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface)
{
    setObjectName(m_featureId);
    m_worker = new SimplePTTWorker(webAPIAdapterInterface);
    m_state = StIdle;
    m_errorMessage = "SimplePTT error";
}

SimplePTT::~SimplePTT()
{
    if (m_worker->isRunning()) {
        stop();
    }

    delete m_worker;    
}

void SimplePTT::start()
{
	qDebug("SimplePTT::start");

    m_worker->reset();
    m_worker->setMessageQueueToGUI(getMessageQueueToGUI());
    bool ok = m_worker->startWork();
    m_state = ok ? StRunning : StError;
    m_thread.start();

    SimplePTTWorker::MsgConfigureSimplePTTWorker *msg = SimplePTTWorker::MsgConfigureSimplePTTWorker::create(m_settings, true);
    m_worker->getInputMessageQueue()->push(msg);
}

void SimplePTT::stop()
{
    qDebug("SimplePTT::stop");
	m_worker->stopWork();
    m_state = StIdle;
	m_thread.quit();
	m_thread.wait();
}

bool SimplePTT::handleMessage(const Message& cmd)
{
	if (MsgConfigureSimplePTT::match(cmd))
	{
        MsgConfigureSimplePTT& cfg = (MsgConfigureSimplePTT&) cmd;
        qDebug() << "SimplePTT::handleMessage: MsgConfigureSimplePTT";
        applySettings(cfg.getSettings(), cfg.getForce());

		return true;
	}
    else if (MsgPTT::match(cmd))
    {
        MsgPTT& cfg = (MsgPTT&) cmd;
        qDebug() << "SimplePTT::handleMessage: MsgPTT: tx:" << cfg.getTx();

        SimplePTTWorker::MsgPTT *msg = SimplePTTWorker::MsgPTT::create(cfg.getTx()); 
        m_worker->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "SimplePTT::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
	else
	{
		return false;
	}
}

QByteArray SimplePTT::serialize() const
{
    return m_settings.serialize();
}

bool SimplePTT::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureSimplePTT *msg = MsgConfigureSimplePTT::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureSimplePTT *msg = MsgConfigureSimplePTT::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void SimplePTT::applySettings(const SimplePTTSettings& settings, bool force)
{
    qDebug() << "SimplePTT::applySettings:"
            << " m_title: " << settings.m_title
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_rxDeviceSetIndex: " << settings.m_rxDeviceSetIndex
            << " m_txDeviceSetIndex: " << settings.m_txDeviceSetIndex
            << " m_rx2TxDelayMs: " << settings.m_rx2TxDelayMs
            << " m_tx2RxDelayMs: " << settings.m_tx2RxDelayMs
            << " force: " << force;

    SimplePTTWorker::MsgConfigureSimplePTTWorker *msg = SimplePTTWorker::MsgConfigureSimplePTTWorker::create(
        settings, force
    ); 
    m_worker->getInputMessageQueue()->push(msg);

    m_settings = settings;
}
