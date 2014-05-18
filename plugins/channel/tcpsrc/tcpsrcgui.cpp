#include "tcpsrcgui.h"
#include "plugin/pluginapi.h"
#include "tcpsrc.h"
#include "dsp/channelizer.h"
#include "dsp/spectrumvis.h"
#include "dsp/threadedsamplesink.h"
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

void TCPSrcGUI::resetToDefaults()
{
	ui->sampleFormat->setCurrentIndex(0);
	ui->sampleRate->setText("25000");
	ui->rfBandwidth->setText("20000");
	ui->tcpPort->setText("9999");
	ui->spectrumGUI->resetToDefaults();
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
	s.writeU32(8, m_channelMarker->getColor().rgb());
	return s.final();
}

bool TCPSrcGUI::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
		QByteArray bytetmp;
		qint32 s32tmp;
		quint32 u32tmp;
		Real realtmp;
		d.readBlob(1, &bytetmp);
		restoreState(bytetmp);
		d.readS32(2, &s32tmp, 0);
		m_channelMarker->setCenterFrequency(s32tmp);
		d.readS32(3, &s32tmp, TCPSrc::FormatS8);
		switch(s32tmp) {
			case TCPSrc::FormatS8:
				ui->sampleFormat->setCurrentIndex(0);
				break;
			case TCPSrc::FormatS16LE:
				ui->sampleFormat->setCurrentIndex(1);
				break;
			default:
				ui->sampleFormat->setCurrentIndex(0);
				break;
		}
		d.readReal(4, &realtmp, 25000);
		ui->sampleRate->setText(QString("%1").arg(realtmp, 0));
		d.readReal(5, &realtmp, 20000);
		ui->rfBandwidth->setText(QString("%1").arg(realtmp, 0));
		d.readS32(6, &s32tmp, 9999);
		ui->tcpPort->setText(QString("%1").arg(s32tmp));
		d.readBlob(7, &bytetmp);
		ui->spectrumGUI->deserialize(bytetmp);
		if(d.readU32(8, &u32tmp))
			m_channelMarker->setColor(u32tmp);
		applySettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

bool TCPSrcGUI::handleMessage(Message* message)
{
	if(TCPSrc::MsgTCPSrcConnection::match(message)) {
		TCPSrc::MsgTCPSrcConnection* con = (TCPSrc::MsgTCPSrcConnection*)message;
		if(con->getConnect())
			addConnection(con->getID(), con->getPeerAddress(), con->getPeerPort());
		else delConnection(con->getID());
		message->completed();
		return true;
	} else {
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
	m_basicSettingsShown(false)
{
	ui->setupUi(this);
	ui->connectedClientsBox->hide();
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));
	setAttribute(Qt::WA_DeleteOnClose, true);

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_tcpSrc = new TCPSrc(m_pluginAPI->getMainWindowMessageQueue(), this, m_spectrumVis);
	m_channelizer = new Channelizer(m_tcpSrc);
	m_threadedSampleSink = new ThreadedSampleSink(m_channelizer);
	m_pluginAPI->addSampleSink(m_threadedSampleSink);

	ui->glSpectrum->setCenterFrequency(0);
	ui->glSpectrum->setSampleRate(ui->sampleRate->text().toInt());
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	m_spectrumVis->configure(m_threadedSampleSink->getMessageQueue(), 64, 10, FFTWindow::BlackmanHarris);

	m_channelMarker = new ChannelMarker(this);
	m_channelMarker->setBandwidth(25000);
	m_channelMarker->setCenterFrequency(0);
	m_channelMarker->setVisible(true);
	connect(m_channelMarker, SIGNAL(changed()), this, SLOT(channelMarkerChanged()));
	m_pluginAPI->addChannelMarker(m_channelMarker);

	ui->spectrumGUI->setBuddies(m_threadedSampleSink->getMessageQueue(), m_spectrumVis, ui->glSpectrum);

	applySettings();
}

TCPSrcGUI::~TCPSrcGUI()
{
	m_pluginAPI->removeChannelInstance(this);
	m_pluginAPI->removeSampleSink(m_threadedSampleSink);
	delete m_threadedSampleSink;
	delete m_channelizer;
	delete m_tcpSrc;
	delete m_spectrumVis;
	delete m_channelMarker;
	delete ui;
}

void TCPSrcGUI::applySettings()
{
	bool ok;

	Real outputSampleRate = ui->sampleRate->text().toDouble(&ok);
	if((!ok) || (outputSampleRate < 100))
		outputSampleRate = 25000;
	Real rfBandwidth = ui->rfBandwidth->text().toDouble(&ok);
	if((!ok) || (rfBandwidth > outputSampleRate))
		rfBandwidth = outputSampleRate;
	int tcpPort = ui->tcpPort->text().toInt(&ok);
	if((!ok) || (tcpPort < 1) || (tcpPort > 65535))
		tcpPort = 9999;

	setTitleColor(m_channelMarker->getColor());
	ui->sampleRate->setText(QString("%1").arg(outputSampleRate, 0));
	ui->rfBandwidth->setText(QString("%1").arg(rfBandwidth, 0));
	ui->tcpPort->setText(QString("%1").arg(tcpPort));
	m_channelMarker->disconnect(this, SLOT(channelMarkerChanged()));
	m_channelMarker->setBandwidth((int)rfBandwidth);
	connect(m_channelMarker, SIGNAL(changed()), this, SLOT(channelMarkerChanged()));
	ui->glSpectrum->setSampleRate(outputSampleRate);

	m_channelizer->configure(m_threadedSampleSink->getMessageQueue(),
		outputSampleRate,
		m_channelMarker->getCenterFrequency());

	TCPSrc::SampleFormat sampleFormat;
	switch(ui->sampleFormat->currentIndex()) {
		case 0:
			sampleFormat = TCPSrc::FormatS8;
			break;
		case 1:
			sampleFormat = TCPSrc::FormatS16LE;
			break;
		default:
			sampleFormat = TCPSrc::FormatS8;
			break;
	}

	m_sampleFormat = sampleFormat;
	m_outputSampleRate = outputSampleRate;
	m_rfBandwidth = rfBandwidth;
	m_tcpPort = tcpPort;

	m_tcpSrc->configure(m_threadedSampleSink->getMessageQueue(),
		sampleFormat,
		outputSampleRate,
		rfBandwidth,
		tcpPort);

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

void TCPSrcGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
	if((widget == ui->spectrumBox) && (m_tcpSrc != NULL))
		m_tcpSrc->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
}

void TCPSrcGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown) {
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
	for(int i = 0; i < ui->connections->topLevelItemCount(); i++) {
		if(ui->connections->topLevelItem(i)->type() == id) {
			delete ui->connections->topLevelItem(i);
			ui->connectedClientsBox->setWindowTitle(tr("Connected Clients (%1)").arg(ui->connections->topLevelItemCount()));
			return;
		}
	}
}
