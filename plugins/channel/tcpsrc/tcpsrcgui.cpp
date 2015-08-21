#include "tcpsrcgui.h"
#include "plugin/pluginapi.h"
#include "tcpsrc.h"
#include "dsp/threadedsamplesink.h"
#include "dsp/channelizer.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "gui/basicchannelsettingswidget.h"
#include "ui_tcpsrcgui.h"

TCPSrcGUI* TCPSrcGUI::create(PluginAPI* pluginAPI)
{
	TCPSrcGUI* gui = new TCPSrcGUI(pluginAPI);
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
	return m_channelMarker->getCenterFrequency();
}

QString TCPSrcGUI::getName() const
{
	return objectName();
}

void TCPSrcGUI::resetToDefaults()
{
	blockApplySettings(true);
    
	ui->sampleFormat->setCurrentIndex(0);
	ui->sampleRate->setText("48000");
	ui->rfBandwidth->setText("32000");
	ui->tcpPort->setText("9999");
	ui->spectrumGUI->resetToDefaults();
	ui->boost->setValue(1);

	blockApplySettings(false);
	applySettings();
}

QByteArray TCPSrcGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeBlob(1, saveState());
	s.writeS32(2, m_channelMarker->getCenterFrequency());
	s.writeS32(3, m_sampleFormat);
	s.writeReal(4, m_outputSampleRate);
	s.writeReal(5, m_rfBandwidth);
	s.writeS32(6, m_tcpPort);
	s.writeBlob(7, ui->spectrumGUI->serialize());
	s.writeS32(8, (qint32)m_boost);
	return s.final();
}

bool TCPSrcGUI::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid())
	{
		resetToDefaults();
		return false;
	}

	if (d.getVersion() == 1)
	{
		QByteArray bytetmp;
		qint32 s32tmp;
		Real realtmp;
        
		blockApplySettings(true);
		m_channelMarker->blockSignals(true);
        
		d.readBlob(1, &bytetmp);
		restoreState(bytetmp);
		d.readS32(2, &s32tmp, 0);
		m_channelMarker->setCenterFrequency(s32tmp);
		d.readS32(3, &s32tmp, TCPSrc::FormatSSB);
		switch(s32tmp) {
			case TCPSrc::FormatSSB:
				ui->sampleFormat->setCurrentIndex(0);
				break;
			case TCPSrc::FormatNFM:
				ui->sampleFormat->setCurrentIndex(1);
				break;
			case TCPSrc::FormatS16LE:
				ui->sampleFormat->setCurrentIndex(2);
				break;
			default:
				ui->sampleFormat->setCurrentIndex(0);
				break;
		}
		d.readReal(4, &realtmp, 48000);
		ui->sampleRate->setText(QString("%1").arg(realtmp, 0));
		d.readReal(5, &realtmp, 32000);
		ui->rfBandwidth->setText(QString("%1").arg(realtmp, 0));
		d.readS32(6, &s32tmp, 9999);
		ui->tcpPort->setText(QString("%1").arg(s32tmp));
		d.readBlob(7, &bytetmp);
		ui->spectrumGUI->deserialize(bytetmp);
		d.readS32(8, &s32tmp, 1);
		ui->boost->setValue(s32tmp);
        
		blockApplySettings(false);
		m_channelMarker->blockSignals(false);
        
		applySettings();
		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

bool TCPSrcGUI::handleMessage(const Message& message)
{
	qDebug() << "TCPSrcGUI::handleMessage";

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

		qDebug() << "  - TCPSrc::MsgTCPSrcConnection: ID: " << con.getID()
				<< " peerAddress: " << con.getPeerAddress()
				<< " peerPort: " << con.getPeerPort();

		return true;
	}
	else
	{
		return false;
	}
}

void TCPSrcGUI::channelMarkerChanged()
{
	applySettings();
}

TCPSrcGUI::TCPSrcGUI(PluginAPI* pluginAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::TCPSrcGUI),
	m_pluginAPI(pluginAPI),
	m_tcpSrc(NULL),
	m_basicSettingsShown(false),
	m_doApplySettings(true)
{
	ui->setupUi(this);
	ui->connectedClientsBox->hide();
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));
	setAttribute(Qt::WA_DeleteOnClose, true);

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_tcpSrc = new TCPSrc(m_pluginAPI->getMainWindowMessageQueue(), this, m_spectrumVis);
	m_channelizer = new Channelizer(m_tcpSrc);
	m_threadedChannelizer = new ThreadedSampleSink(m_channelizer, this);
	DSPEngine::instance()->addThreadedSink(m_threadedChannelizer);

	ui->glSpectrum->setCenterFrequency(0);
	ui->glSpectrum->setSampleRate(ui->sampleRate->text().toInt());
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	m_spectrumVis->configure(m_spectrumVis->getInputMessageQueue(), 64, 10, FFTWindow::BlackmanHarris);

	m_channelMarker = new ChannelMarker(this);
	m_channelMarker->setBandwidth(16000);
	m_channelMarker->setCenterFrequency(0);
	m_channelMarker->setColor(Qt::green);
	m_channelMarker->setVisible(true);
	connect(m_channelMarker, SIGNAL(changed()), this, SLOT(channelMarkerChanged()));
	m_pluginAPI->addChannelMarker(m_channelMarker);

	ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

	applySettings();
}

TCPSrcGUI::~TCPSrcGUI()
{
	m_pluginAPI->removeChannelInstance(this);
	DSPEngine::instance()->removeThreadedSink(m_threadedChannelizer);
	delete m_threadedChannelizer;
	delete m_channelizer;
	delete m_tcpSrc;
	delete m_spectrumVis;
	delete m_channelMarker;
	delete ui;
}

void TCPSrcGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void TCPSrcGUI::applySettings()
{
	bool ok;

	Real outputSampleRate = ui->sampleRate->text().toDouble(&ok);

	if((!ok) || (outputSampleRate < 1000))
	{
		outputSampleRate = 48000;
	}

	Real rfBandwidth = ui->rfBandwidth->text().toDouble(&ok);

	if((!ok) || (rfBandwidth > outputSampleRate))
	{
		rfBandwidth = outputSampleRate;
	}

	int tcpPort = ui->tcpPort->text().toInt(&ok);

	if((!ok) || (tcpPort < 1) || (tcpPort > 65535))
	{
		tcpPort = 9999;
	}

	int boost = ui->boost->value();

	setTitleColor(m_channelMarker->getColor());
	ui->sampleRate->setText(QString("%1").arg(outputSampleRate, 0));
	ui->rfBandwidth->setText(QString("%1").arg(rfBandwidth, 0));
	ui->tcpPort->setText(QString("%1").arg(tcpPort));
	ui->boost->setValue(boost);
	m_channelMarker->disconnect(this, SLOT(channelMarkerChanged()));
	m_channelMarker->setBandwidth((int)rfBandwidth);
	connect(m_channelMarker, SIGNAL(changed()), this, SLOT(channelMarkerChanged()));
	ui->glSpectrum->setSampleRate(outputSampleRate);

	m_channelizer->configure(m_channelizer->getInputMessageQueue(),
		outputSampleRate,
		m_channelMarker->getCenterFrequency());

	TCPSrc::SampleFormat sampleFormat;

	switch(ui->sampleFormat->currentIndex())
	{
		case 0:
			sampleFormat = TCPSrc::FormatSSB;
			break;
		case 1:
			sampleFormat = TCPSrc::FormatNFM;
			break;
		case 2:
			sampleFormat = TCPSrc::FormatS16LE;
			break;
		default:
			sampleFormat = TCPSrc::FormatSSB;
			break;
	}

	m_sampleFormat = sampleFormat;
	m_outputSampleRate = outputSampleRate;
	m_rfBandwidth = rfBandwidth;
	m_tcpPort = tcpPort;
	m_boost = boost;

	m_tcpSrc->configure(m_tcpSrc->getInputMessageQueue(),
		sampleFormat,
		outputSampleRate,
		rfBandwidth,
		tcpPort,
		boost);

	ui->applyBtn->setEnabled(false);
}

void TCPSrcGUI::on_sampleFormat_currentIndexChanged(int index)
{
	ui->applyBtn->setEnabled(true);
}

void TCPSrcGUI::on_sampleRate_textEdited(const QString& arg1)
{
	ui->applyBtn->setEnabled(true);
}

void TCPSrcGUI::on_rfBandwidth_textEdited(const QString& arg1)
{
	ui->applyBtn->setEnabled(true);
}

void TCPSrcGUI::on_tcpPort_textEdited(const QString& arg1)
{
	ui->applyBtn->setEnabled(true);
}

void TCPSrcGUI::on_applyBtn_clicked()
{
	applySettings();
}

void TCPSrcGUI::on_boost_valueChanged(int value)
{
	ui->boost->setValue(value);
	ui->applyBtn->setEnabled(true);
}

void TCPSrcGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
	if ((widget == ui->spectrumBox) && (m_tcpSrc != 0))
	{
		m_tcpSrc->setSpectrum(m_tcpSrc->getInputMessageQueue(), rollDown);
	}
}

void TCPSrcGUI::onMenuDoubleClicked()
{
	if (!m_basicSettingsShown)
	{
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(m_channelMarker, this);
		bcsw->show();
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
