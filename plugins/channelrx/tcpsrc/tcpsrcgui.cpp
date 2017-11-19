#include "tcpsrcgui.h"

#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "ui_tcpsrcgui.h"
#include "mainwindow.h"
#include "tcpsrc.h"

TCPSrcGUI* TCPSrcGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	TCPSrcGUI* gui = new TCPSrcGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void TCPSrcGUI::destroy()
{
	delete this;
}

void TCPSrcGUI::setName(const QString& name)
{
	setObjectName(name);
}

qint64 TCPSrcGUI::getCenterFrequency() const
{
	return m_channelMarker.getCenterFrequency();
}

void TCPSrcGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

QString TCPSrcGUI::getName() const
{
	return objectName();
}

void TCPSrcGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings();
}

QByteArray TCPSrcGUI::serialize() const
{
    return m_settings.serialize();
}

bool TCPSrcGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        qDebug("TCPSrcGUI::deserialize: m_squelchGate: %d", m_settings.m_squelchGate);
        displaySettings();
        applySettings();
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool TCPSrcGUI::handleMessage(const Message& message)
{
	if (TCPSrc::MsgTCPSrcConnection::match(message))
	{
		TCPSrc::MsgTCPSrcConnection& con = (TCPSrc::MsgTCPSrcConnection&) message;

		if(con.getConnect())
		{
			addConnection(con.getID(), con.getPeerAddress(), con.getPeerPort());
		}
		else
		{
			delConnection(con.getID());
		}

		qDebug() << "TCPSrcGUI::handleMessage: TCPSrc::MsgTCPSrcConnection: " << con.getConnect()
				<< " ID: " << con.getID()
				<< " peerAddress: " << con.getPeerAddress()
				<< " peerPort: " << con.getPeerPort();

		return true;
	}
	else
	{
		return false;
	}
}

void TCPSrcGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void TCPSrcGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void TCPSrcGUI::tick()
{
    double powDb = CalcDb::dbPower(m_tcpSrc->getMagSq());
	m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(QString::number(m_channelPowerDbAvg.average(), 'f', 1));
}

TCPSrcGUI::TCPSrcGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::TCPSrcGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_tcpSrc(0),
	m_channelMarker(this),
	m_channelPowerDbAvg(40,0),
	m_rfBandwidthChanged(false),
	m_doApplySettings(true)
{
	ui->setupUi(this);
	ui->connectedClientsBox->hide();
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	setAttribute(Qt::WA_DeleteOnClose, true);

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_tcpSrc = (TCPSrc*) rxChannel; //new TCPSrc(m_deviceUISet->m_deviceSourceAPI);
	m_tcpSrc->setSpectrum(m_spectrumVis);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
	ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

	ui->glSpectrum->setCenterFrequency(0);
	ui->glSpectrum->setSampleRate(ui->sampleRate->text().toInt());
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	m_spectrumVis->configure(m_spectrumVis->getInputMessageQueue(), 64, 10, FFTWindow::BlackmanHarris);

	ui->glSpectrum->connectTimer(MainWindow::getInstance()->getMasterTimer());
	connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	m_channelMarker.blockSignals(true);
	m_channelMarker.setBandwidth(16000);
	m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	setTitleColor(m_channelMarker.getColor());

	m_deviceUISet->registerRxChannelInstance(TCPSrc::m_channelID, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

	ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setChannelMarker(&m_channelMarker);

    displaySettings();
    applySettings();
}

TCPSrcGUI::~TCPSrcGUI()
{
    m_deviceUISet->removeRxChannelInstance(this);
	delete m_tcpSrc; // TODO: check this: when the GUI closes it has to delete the demodulator
	delete m_spectrumVis;
	delete ui;
}

void TCPSrcGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void TCPSrcGUI::applySettings()
{
	if (m_doApplySettings)
	{
		ui->glSpectrum->setSampleRate(m_settings.m_outputSampleRate);

        TCPSrc::MsgConfigureChannelizer* channelConfigMsg = TCPSrc::MsgConfigureChannelizer::create(
                m_settings.m_outputSampleRate, m_channelMarker.getCenterFrequency());
        m_tcpSrc->getInputMessageQueue()->push(channelConfigMsg);

        TCPSrc::MsgConfigureTCPSrc* message = TCPSrc::MsgConfigureTCPSrc::create( m_settings, false);
        m_tcpSrc->getInputMessageQueue()->push(message);
	}
}

void TCPSrcGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void TCPSrcGUI::on_sampleFormat_currentIndexChanged(int index)
{
    setSampleFormat(index);

    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void TCPSrcGUI::on_sampleRate_textEdited(const QString& arg1 __attribute__((unused)))
{
    bool ok;
    Real outputSampleRate = ui->sampleRate->text().toDouble(&ok);

    if((!ok) || (outputSampleRate < 1000))
    {
        m_settings.m_outputSampleRate = 48000;
        ui->sampleRate->setText(QString("%1").arg(outputSampleRate, 0));
    }
    else
    {
        m_settings.m_outputSampleRate = outputSampleRate;
    }

    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void TCPSrcGUI::on_rfBandwidth_textEdited(const QString& arg1 __attribute__((unused)))
{
    bool ok;
    Real rfBandwidth = ui->rfBandwidth->text().toDouble(&ok);

    if((!ok) || (rfBandwidth > m_settings.m_outputSampleRate))
    {
        m_settings.m_rfBandwidth = m_settings.m_outputSampleRate;
        ui->rfBandwidth->setText(QString("%1").arg(m_settings.m_rfBandwidth, 0));
    }
    else
    {
        m_settings.m_rfBandwidth = rfBandwidth;
    }

    m_rfBandwidthChanged = true;

    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void TCPSrcGUI::on_tcpPort_textEdited(const QString& arg1 __attribute__((unused)))
{
    bool ok;
    int tcpPort = ui->tcpPort->text().toInt(&ok);

    if((!ok) || (tcpPort < 1) || (tcpPort > 65535))
    {
        m_settings.m_tcpPort = 9999;
        ui->tcpPort->setText(QString("%1").arg(m_settings.m_tcpPort, 0));
    }
    else
    {
        m_settings.m_tcpPort = tcpPort;
    }

	ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void TCPSrcGUI::on_applyBtn_clicked()
{
    if (m_rfBandwidthChanged)
    {
        blockApplySettings(true);
        m_channelMarker.setBandwidth((int) m_settings.m_rfBandwidth);
        m_rfBandwidthChanged = false;
        blockApplySettings(false);
    }

    ui->glSpectrum->setSampleRate(m_settings.m_outputSampleRate);

    ui->applyBtn->setEnabled(false);
    ui->applyBtn->setStyleSheet("QPushButton { background:rgb(79,79,79); }");

	applySettings();
}

void TCPSrcGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->sampleRate->setText(QString("%1").arg(m_settings.m_outputSampleRate, 0));
    setSampleFormatIndex(m_settings.m_sampleFormat);

    ui->rfBandwidth->setText(QString("%1").arg(m_settings.m_rfBandwidth, 0));

    ui->volume->setValue(m_settings.m_volume);
    ui->volumeText->setText(QString("%1").arg(ui->volume->value()));

    ui->glSpectrum->setSampleRate(m_settings.m_outputSampleRate);

    blockApplySettings(false);
}

void TCPSrcGUI::setSampleFormatIndex(const TCPSrcSettings::SampleFormat& sampleFormat)
{
    switch(sampleFormat)
    {
        case TCPSrcSettings::FormatS16LE:
            ui->sampleFormat->setCurrentIndex(0);
            break;
        case TCPSrcSettings::FormatNFM:
            ui->sampleFormat->setCurrentIndex(1);
            break;
        case TCPSrcSettings::FormatSSB:
            ui->sampleFormat->setCurrentIndex(2);
            break;
        default:
            ui->sampleFormat->setCurrentIndex(0);
            break;
    }
}

void TCPSrcGUI::setSampleFormat(int index)
{
    switch(index)
    {
        case 0:
            m_settings.m_sampleFormat = TCPSrcSettings::FormatS16LE;
            break;
        case 1:
            m_settings.m_sampleFormat = TCPSrcSettings::FormatNFM;
            break;
        case 2:
            m_settings.m_sampleFormat = TCPSrcSettings::FormatSSB;
            break;
        default:
            m_settings.m_sampleFormat = TCPSrcSettings::FormatS16LE;
            break;
    }
}

void TCPSrcGUI::on_volume_valueChanged(int value)
{
	ui->volume->setValue(value);
	ui->volumeText->setText(QString("%1").arg(value));
	ui->applyBtn->setEnabled(true);
}

void TCPSrcGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
	if ((widget == ui->spectrumBox) && (m_tcpSrc != 0))
	{
		m_tcpSrc->setSpectrum(m_tcpSrc->getInputMessageQueue(), rollDown);
	}
}

void TCPSrcGUI::addConnection(quint32 id, const QHostAddress& peerAddress, int peerPort)
{
	QStringList l;
	l.append(QString("%1:%2").arg(peerAddress.toString()).arg(peerPort));
	new QTreeWidgetItem(ui->connections, l, id);
	ui->connectedClientsBox->setWindowTitle(tr("Connected Clients (%1)").arg(ui->connections->topLevelItemCount()));
}

void TCPSrcGUI::delConnection(quint32 id)
{
	for(int i = 0; i < ui->connections->topLevelItemCount(); i++)
	{
		if(ui->connections->topLevelItem(i)->type() == (int)id)
		{
			delete ui->connections->topLevelItem(i);
			ui->connectedClientsBox->setWindowTitle(tr("Connected Clients (%1)").arg(ui->connections->topLevelItemCount()));
			return;
		}
	}
}
