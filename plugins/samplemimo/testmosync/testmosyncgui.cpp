///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include <QTime>
#include <QString>
#include <QMessageBox>

#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/spectrumvis.h"
#include "mainwindow.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"

#include "testmosync.h"
#include "ui_testmosyncgui.h"
#include "testmosyncgui.h"

TestMOSyncGui::TestMOSyncGui(DeviceUISet *deviceUISet, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::TestMOSyncGui),
	m_deviceUISet(deviceUISet),
	m_doApplySettings(true),
	m_forceSettings(true),
	m_settings(),
    m_sampleMIMO(nullptr),
    m_sampleRate(0),
    m_generation(false),
	m_samplesCount(0),
	m_tickCount(0),
	m_lastEngineState(DeviceAPI::StNotStarted)
{
	ui->setupUi(this);
    m_sampleMIMO = (TestMOSync*) m_deviceUISet->m_deviceAPI->getSampleMIMO();

	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->centerFrequency->setValueRange(7, 0, pow(10,7));

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(7, 32000U, 9000000U);

    m_spectrumVis = new SpectrumVis(SDR_TX_SCALEF, ui->glSpectrum);
    m_sampleMIMO->setSpectrumSink(m_spectrumVis);
    ui->glSpectrum->setCenterFrequency(m_settings.m_centerFrequency);
    ui->glSpectrum->setSampleRate(m_settings.m_sampleRate*(1<<m_settings.m_log2Interp));
    ui->glSpectrum->connectTimer(MainWindow::getInstance()->getMasterTimer());
    ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

	connect(&(m_deviceUISet->m_deviceAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	displaySettings();

    m_sampleMIMO->setMessageQueueToGUI(&m_inputMessageQueue);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);

    m_deviceUISet->m_spectrum->setDisplayedStream(false, 0);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(false, 0);
    m_deviceUISet->setSpectrumScalingFactor(SDR_TX_SCALEF);
}

TestMOSyncGui::~TestMOSyncGui()
{
    delete m_spectrumVis;
	delete ui;
}

void TestMOSyncGui::destroy()
{
	delete this;
}

void TestMOSyncGui::setName(const QString& name)
{
	setObjectName(name);
}

QString TestMOSyncGui::getName() const
{
	return objectName();
}

void TestMOSyncGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 TestMOSyncGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void TestMOSyncGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

QByteArray TestMOSyncGui::serialize() const
{
	return m_settings.serialize();
}

bool TestMOSyncGui::deserialize(const QByteArray& data)
{
	if (m_settings.deserialize(data))
    {
		displaySettings();
		m_forceSettings = true;
		sendSettings();
		return true;
	}
    else
    {
		resetToDefaults();
		return false;
	}
}

bool TestMOSyncGui::handleMessage(const Message& message)
{
    if (DSPMIMOSignalNotification::match(message))
    {
        const DSPMIMOSignalNotification& cfg = (DSPMIMOSignalNotification&) message;
        int istream = cfg.getIndex();
        bool sourceOrSink = cfg.getSourceOrSink();
        qDebug("TestMOSyncGui::handleMessage: DSPMIMOSignalNotification: %s:%d SampleRate:%d, CenterFrequency:%llu",
            sourceOrSink ? "Rx" : "Tx",
            istream,
            cfg.getSampleRate(),
            cfg.getCenterFrequency()
        );

        if (!sourceOrSink)
        {
            m_sampleRate = cfg.getSampleRate();
            m_deviceCenterFrequency = cfg.getCenterFrequency();
            updateSampleRateAndFrequency();
        }

        return true;
    }
    else if (TestMOSync::MsgConfigureTestMOSync::match(message))
    {
        qDebug("TestMOSyncGui::handleMessage: message: MsgConfigureTestMOSync");
        const TestMOSync::MsgConfigureTestMOSync& cfg = (TestMOSync::MsgConfigureTestMOSync&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
	else if (TestMOSync::MsgStartStop::match(message))
	{
	    TestMOSync::MsgStartStop& notif = (TestMOSync::MsgStartStop&) message;
        qDebug("TestMOSyncGui::handleMessage: message: MsgStartStop: %s", notif.getStartStop() ? "start" : "stop");
	    blockApplySettings(true);
	    ui->startStop->setChecked(notif.getStartStop());
	    blockApplySettings(false);
	    return true;
	}
	else
	{
		return false;
	}
}

void TestMOSyncGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void TestMOSyncGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)(m_sampleRate*(1<<m_settings.m_log2Interp)) / 1000));
}

void TestMOSyncGui::displaySettings()
{
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->sampleRate->setValue(m_settings.m_sampleRate);
}

void TestMOSyncGui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void TestMOSyncGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "TestMOSyncGui::updateHardware";
        TestMOSync::MsgConfigureTestMOSync* message = TestMOSync::MsgConfigureTestMOSync::create(m_settings, m_forceSettings);
        m_sampleMIMO->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void TestMOSyncGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceAPI->state(1);

    if (m_lastEngineState != state)
    {
        switch (state)
        {
            case DeviceAPI::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DeviceAPI::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DeviceAPI::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DeviceAPI::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

void TestMOSyncGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    ui->glSpectrum->setCenterFrequency(m_settings.m_centerFrequency);
    sendSettings();
}

void TestMOSyncGui::on_sampleRate_changed(quint64 value)
{
    m_settings.m_sampleRate = value;
    ui->glSpectrum->setSampleRate(m_settings.m_sampleRate*(1<<m_settings.m_log2Interp));
    sendSettings();
}

void TestMOSyncGui::on_interp_currentIndexChanged(int index)
{
    if (index < 0) {
        return;
    }

    m_settings.m_log2Interp = index;
    ui->glSpectrum->setSampleRate(m_settings.m_sampleRate*(1<<m_settings.m_log2Interp));
    sendSettings();
}

void TestMOSyncGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        TestMOSync::MsgStartStop *message = TestMOSync::MsgStartStop::create(checked, false);
        m_sampleMIMO->getInputMessageQueue()->push(message);
    }
}

void TestMOSyncGui::on_spectrumIndex_currentIndexChanged(int index)
{
    m_deviceUISet->m_spectrum->setDisplayedStream(false, index);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(false, index);
    m_sampleMIMO->setFeedSpectrumIndex(index);
}

void TestMOSyncGui::tick()
{
}
