#include "udpsrcgui.h"

#include "plugin/pluginapi.h"
#include "dsp/threadedsamplesink.h"
#include "dsp/channelizer.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "ui_udpsrcgui.h"
#include "mainwindow.h"
#include "udpsrc.h"

UDPSrcGUI* UDPSrcGUI::create(PluginAPI* pluginAPI)
{
	UDPSrcGUI* gui = new UDPSrcGUI(pluginAPI);
	return gui;
}

void UDPSrcGUI::destroy()
{
	delete this;
}

void UDPSrcGUI::setName(const QString& name)
{
	setObjectName(name);
}

qint64 UDPSrcGUI::getCenterFrequency() const
{
	return m_channelMarker.getCenterFrequency();
}

void UDPSrcGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

QString UDPSrcGUI::getName() const
{
	return objectName();
}

void UDPSrcGUI::resetToDefaults()
{
	blockApplySettings(true);
    
	ui->sampleFormat->setCurrentIndex(0);
	ui->sampleRate->setText("48000");
	ui->rfBandwidth->setText("32000");
	ui->udpAddress->setText("127.0.0.1");
	ui->udpPort->setText("9999");
	ui->spectrumGUI->resetToDefaults();
	ui->boost->setValue(1);

	blockApplySettings(false);
	applySettings();
}

QByteArray UDPSrcGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeBlob(1, saveState());
	s.writeS32(2, m_channelMarker.getCenterFrequency());
	s.writeS32(3, m_sampleFormat);
	s.writeReal(4, m_outputSampleRate);
	s.writeReal(5, m_rfBandwidth);
	s.writeS32(6, m_udpPort);
	s.writeBlob(7, ui->spectrumGUI->serialize());
	s.writeS32(8, (qint32)m_boost);
	s.writeS32(9, m_channelMarker.getCenterFrequency());
	s.writeString(10, m_udpAddress);
	return s.final();
}

bool UDPSrcGUI::deserialize(const QByteArray& data)
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
		QString strtmp;
		qint32 s32tmp;
		Real realtmp;
        
		blockApplySettings(true);
		m_channelMarker.blockSignals(true);
        
		d.readBlob(1, &bytetmp);
		restoreState(bytetmp);
		d.readS32(2, &s32tmp, 0);
		m_channelMarker.setCenterFrequency(s32tmp);
		d.readS32(3, &s32tmp, UDPSrc::FormatSSB);
		switch(s32tmp) {
			case UDPSrc::FormatSSB:
				ui->sampleFormat->setCurrentIndex(0);
				break;
			case UDPSrc::FormatNFM:
				ui->sampleFormat->setCurrentIndex(1);
				break;
			case UDPSrc::FormatS16LE:
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
		ui->udpPort->setText(QString("%1").arg(s32tmp));
		d.readBlob(7, &bytetmp);
		ui->spectrumGUI->deserialize(bytetmp);
		d.readS32(8, &s32tmp, 1);
		ui->boost->setValue(s32tmp);
		d.readS32(9, &s32tmp, 0);
		m_channelMarker.setCenterFrequency(s32tmp);
		d.readString(10, &strtmp, "127.0.0.1");
		ui->udpAddress->setText(strtmp);
        
		blockApplySettings(false);
		m_channelMarker.blockSignals(false);
        
		applySettings();
		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

bool UDPSrcGUI::handleMessage(const Message& message)
{
	qDebug() << "UDPSrcGUI::handleMessage";
	return false;
}

void UDPSrcGUI::channelMarkerChanged()
{
	applySettings();
}

void UDPSrcGUI::tick()
{
	Real powDb = CalcDb::dbPower(m_udpSrc->getMagSq());
	m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(QString::number(m_channelPowerDbAvg.average(), 'f', 1));
}

UDPSrcGUI::UDPSrcGUI(PluginAPI* pluginAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::UDPSrcGUI),
	m_pluginAPI(pluginAPI),
	m_udpSrc(0),
	m_channelMarker(this),
	m_channelPowerDbAvg(40,0),
	m_basicSettingsShown(false),
	m_doApplySettings(true)
{
	ui->setupUi(this);
	ui->connectedClientsBox->hide();
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));
	setAttribute(Qt::WA_DeleteOnClose, true);

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_udpSrc = new UDPSrc(m_pluginAPI->getMainWindowMessageQueue(), this, m_spectrumVis);
	m_channelizer = new Channelizer(m_udpSrc);
	m_threadedChannelizer = new ThreadedSampleSink(m_channelizer, this);
	DSPEngine::instance()->addThreadedSink(m_threadedChannelizer);

	ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->deltaFrequency->setValueRange(7, 0U, 9999999U);

	ui->glSpectrum->setCenterFrequency(0);
	ui->glSpectrum->setSampleRate(ui->sampleRate->text().toInt());
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	m_spectrumVis->configure(m_spectrumVis->getInputMessageQueue(), 64, 10, FFTWindow::BlackmanHarris);

	ui->glSpectrum->connectTimer(m_pluginAPI->getMainWindow()->getMasterTimer());
	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	//m_channelMarker = new ChannelMarker(this);
	m_channelMarker.setBandwidth(16000);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setColor(Qt::green);
	m_channelMarker.setVisible(true);
	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(channelMarkerChanged()));
	m_pluginAPI->addChannelMarker(&m_channelMarker);

	ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

	applySettings();
}

UDPSrcGUI::~UDPSrcGUI()
{
	m_pluginAPI->removeChannelInstance(this);
	DSPEngine::instance()->removeThreadedSink(m_threadedChannelizer);
	delete m_threadedChannelizer;
	delete m_channelizer;
	delete m_udpSrc;
	delete m_spectrumVis;
	//delete m_channelMarker;
	delete ui;
}

void UDPSrcGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void UDPSrcGUI::applySettings()
{
	if (m_doApplySettings)
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

		m_udpAddress = ui->udpAddress->text();
		int udpPort = ui->udpPort->text().toInt(&ok);

		if((!ok) || (udpPort < 1) || (udpPort > 65535))
		{
			udpPort = 9999;
		}

		int boost = ui->boost->value();

		setTitleColor(m_channelMarker.getColor());
		ui->deltaFrequency->setValue(abs(m_channelMarker.getCenterFrequency()));
		ui->deltaMinus->setChecked(m_channelMarker.getCenterFrequency() < 0);
		ui->sampleRate->setText(QString("%1").arg(outputSampleRate, 0));
		ui->rfBandwidth->setText(QString("%1").arg(rfBandwidth, 0));
		//ui->udpAddress->setText(m_udpAddress);
		ui->udpPort->setText(QString("%1").arg(udpPort));
		ui->boost->setValue(boost);
		m_channelMarker.disconnect(this, SLOT(channelMarkerChanged()));
		m_channelMarker.setBandwidth((int)rfBandwidth);
		connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(channelMarkerChanged()));
		ui->glSpectrum->setSampleRate(outputSampleRate);

		m_channelizer->configure(m_channelizer->getInputMessageQueue(),
			outputSampleRate,
			m_channelMarker.getCenterFrequency());

		UDPSrc::SampleFormat sampleFormat;

		switch(ui->sampleFormat->currentIndex())
		{
			case 0:
				sampleFormat = UDPSrc::FormatSSB;
				break;
			case 1:
				sampleFormat = UDPSrc::FormatNFM;
				break;
			case 2:
				sampleFormat = UDPSrc::FormatS16LE;
				break;
			default:
				sampleFormat = UDPSrc::FormatSSB;
				break;
		}

		m_sampleFormat = sampleFormat;
		m_outputSampleRate = outputSampleRate;
		m_rfBandwidth = rfBandwidth;
		m_udpPort = udpPort;
		m_boost = boost;

		m_udpSrc->configure(m_udpSrc->getInputMessageQueue(),
			sampleFormat,
			outputSampleRate,
			rfBandwidth,
			m_udpAddress,
			udpPort,
			boost);

		ui->applyBtn->setEnabled(false);
	}
}

void UDPSrcGUI::on_deltaMinus_toggled(bool minus)
{
	int deltaFrequency = m_channelMarker.getCenterFrequency();
	bool minusDelta = (deltaFrequency < 0);

	if (minus ^ minusDelta) // sign change
	{
		m_channelMarker.setCenterFrequency(-deltaFrequency);
	}
}

void UDPSrcGUI::on_deltaFrequency_changed(quint64 value)
{
	if (ui->deltaMinus->isChecked()) {
		m_channelMarker.setCenterFrequency(-value);
	} else {
		m_channelMarker.setCenterFrequency(value);
	}
}

void UDPSrcGUI::on_sampleFormat_currentIndexChanged(int index)
{
	ui->applyBtn->setEnabled(true);
}

void UDPSrcGUI::on_sampleRate_textEdited(const QString& arg1)
{
	ui->applyBtn->setEnabled(true);
}

void UDPSrcGUI::on_rfBandwidth_textEdited(const QString& arg1)
{
	ui->applyBtn->setEnabled(true);
}

void UDPSrcGUI::on_udpPort_textEdited(const QString& arg1)
{
	ui->applyBtn->setEnabled(true);
}

void UDPSrcGUI::on_applyBtn_clicked()
{
	applySettings();
}

void UDPSrcGUI::on_boost_valueChanged(int value)
{
	ui->boost->setValue(value);
	ui->boostText->setText(QString("%1").arg(value));
	ui->applyBtn->setEnabled(true);
}

void UDPSrcGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
	if ((widget == ui->spectrumBox) && (m_udpSrc != 0))
	{
		m_udpSrc->setSpectrum(m_udpSrc->getInputMessageQueue(), rollDown);
	}
}

void UDPSrcGUI::onMenuDoubleClicked()
{
	if (!m_basicSettingsShown)
	{
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(&m_channelMarker, this);
		bcsw->show();
	}
}
