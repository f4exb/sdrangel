///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon source channel (Tx) GUI                                             //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QTime>
#include <QDebug>

#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "gui/basicchannelsettingsdialog.h"
#include "mainwindow.h"

#include "ui_sdrdaemonchannelsourcegui.h"
#include "sdrdaemonchannelsource.h"
#include "sdrdaemonchannelsourcegui.h"


SDRDaemonChannelSourceGUI* SDRDaemonChannelSourceGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
	SDRDaemonChannelSourceGUI* gui = new SDRDaemonChannelSourceGUI(pluginAPI, deviceUISet, channelTx);
	return gui;
}

void SDRDaemonChannelSourceGUI::destroy()
{
    delete this;
}

void SDRDaemonChannelSourceGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString SDRDaemonChannelSourceGUI::getName() const
{
	return objectName();
}

qint64 SDRDaemonChannelSourceGUI::getCenterFrequency() const {
	return m_channelMarker.getCenterFrequency();
}

void SDRDaemonChannelSourceGUI::setCenterFrequency(qint64 centerFrequency __attribute__((unused)))
{
    // you can't do that center frquency is fixed to zero
	// m_channelMarker.setCenterFrequency(centerFrequency);
	// applySettings();
}

void SDRDaemonChannelSourceGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray SDRDaemonChannelSourceGUI::serialize() const
{
    return m_settings.serialize();
}

bool SDRDaemonChannelSourceGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool SDRDaemonChannelSourceGUI::handleMessage(const Message& message)
{
    if (SDRDaemonChannelSource::MsgConfigureSDRDaemonChannelSource::match(message))
    {
        const SDRDaemonChannelSource::MsgConfigureSDRDaemonChannelSource& cfg = (SDRDaemonChannelSource::MsgConfigureSDRDaemonChannelSource&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }  
    else
    {
        return false;
    }  
}

void SDRDaemonChannelSourceGUI::handleSourceMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void SDRDaemonChannelSourceGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
}

void SDRDaemonChannelSourceGUI::onMenuDialogCalled(const QPoint &p)
{
    BasicChannelSettingsDialog dialog(&m_channelMarker, this);
    dialog.move(p);
    dialog.exec();

    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    m_settings.m_title = m_channelMarker.getTitle();

    setWindowTitle(m_settings.m_title);
    setTitleColor(m_settings.m_rgbColor);

    applySettings();
}

SDRDaemonChannelSourceGUI::SDRDaemonChannelSourceGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::SDRDaemonChannelSourceGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
	m_doApplySettings(true),
    m_tickCount(0)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_sdrDaemonChannelSource = (SDRDaemonChannelSource*) channelTx;
	m_sdrDaemonChannelSource->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::cyan);
    m_channelMarker.setBandwidth(5000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("SDRDaemon sink");
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	m_settings.setChannelMarker(&m_channelMarker);

	m_deviceUISet->registerTxChannelInstance(SDRDaemonChannelSource::m_channelIdURI, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

	displaySettings();
    applySettings(true);
}

SDRDaemonChannelSourceGUI::~SDRDaemonChannelSourceGUI()
{
    m_deviceUISet->removeTxChannelInstance(this);
	delete m_sdrDaemonChannelSource; // TODO: check this: when the GUI closes it has to delete the channel
	delete ui;
}

void SDRDaemonChannelSourceGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SDRDaemonChannelSourceGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		setTitleColor(m_channelMarker.getColor());

        SDRDaemonChannelSource::MsgConfigureSDRDaemonChannelSource* message = SDRDaemonChannelSource::MsgConfigureSDRDaemonChannelSource::create(m_settings, force);
        m_sdrDaemonChannelSource->getInputMessageQueue()->push(message);
	}
}

void SDRDaemonChannelSourceGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(5000); // TODO
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    // TODO

    blockApplySettings(false);
}

void SDRDaemonChannelSourceGUI::leaveEvent(QEvent*)
{
	m_channelMarker.setHighlighted(false);
}

void SDRDaemonChannelSourceGUI::enterEvent(QEvent*)
{
	m_channelMarker.setHighlighted(true);
}

void SDRDaemonChannelSourceGUI::tick()
{
    // TODO if anything useful
}

void SDRDaemonChannelSourceGUI::on_dataAddress_returnPressed()
{
    m_settings.m_dataAddress = ui->dataAddress->text();
    applySettings();
}

void SDRDaemonChannelSourceGUI::on_dataPort_returnPressed()
{
    bool dataOk;
    int dataPort = ui->dataPort->text().toInt(&dataOk);

    if((!dataOk) || (dataPort < 1024) || (dataPort > 65535))
    {
        return;
    }
    else
    {
        m_settings.m_dataPort = dataPort;
    }

    applySettings();
}

void SDRDaemonChannelSourceGUI::on_dataApplyButton_clicked(bool checked __attribute__((unused)))
{
    m_settings.m_dataAddress = ui->dataAddress->text();

    bool dataOk;
    int udpDataPort = ui->dataPort->text().toInt(&dataOk);

    if((dataOk) && (udpDataPort >= 1024) && (udpDataPort < 65535))
    {
        m_settings.m_dataPort = udpDataPort;
    }

    applySettings();
}