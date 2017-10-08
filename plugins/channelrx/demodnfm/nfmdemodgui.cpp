#include "nfmdemodgui.h"

#include <device/devicesourceapi.h>
#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>

#include "ui_nfmdemodgui.h"
#include "dsp/nullsink.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"
#include "nfmdemod.h"

const QString NFMDemodGUI::m_channelID = "de.maintech.sdrangelove.channel.nfm";

const int NFMDemodGUI::m_rfBW[] = {
	5000, 6250, 8330, 10000, 12500, 15000, 20000, 25000, 40000
};
const int NFMDemodGUI::m_fmDev[] = { // corresponding FM deviations
    1000, 1500, 2000, 2000,  2000,  2500,  3000,  3500,  5000
};
const int NFMDemodGUI::m_nbRfBW = 9;

NFMDemodGUI* NFMDemodGUI::create(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI)
{
	NFMDemodGUI* gui = new NFMDemodGUI(pluginAPI, deviceAPI);
	return gui;
}

void NFMDemodGUI::destroy()
{
	delete this;
}

void NFMDemodGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString NFMDemodGUI::getName() const
{
	return objectName();
}

qint64 NFMDemodGUI::getCenterFrequency() const
{
	return m_channelMarker.getCenterFrequency();
}

void NFMDemodGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void NFMDemodGUI::resetToDefaults()
{
	blockApplySettings(true);

	ui->rfBW->setCurrentIndex(4);
	ui->afBW->setValue(3);
	ui->volume->setValue(20);
	ui->squelchGate->setValue(5);
	ui->squelch->setValue(-40);
	ui->deltaFrequency->setValue(0);
	ui->ctcssOn->setChecked(false);
	ui->audioMute->setChecked(false);

	blockApplySettings(false);
	applySettings();
}

QByteArray NFMDemodGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_channelMarker.getCenterFrequency());
	s.writeS32(2, ui->rfBW->currentIndex());
	s.writeS32(3, ui->afBW->value());
	s.writeS32(4, ui->volume->value());
	s.writeS32(5, ui->squelch->value());
	s.writeU32(7, m_channelMarker.getColor().rgb());
	s.writeS32(8, ui->ctcss->currentIndex());
	s.writeBool(9, ui->ctcssOn->isChecked());
	s.writeBool(10, ui->audioMute->isChecked());
	s.writeS32(11, ui->squelchGate->value());
	s.writeBool(12, ui->deltaSquelch->isChecked());
	s.writeBlob(13, m_channelMarker.serialize());
	return s.final();
}

bool NFMDemodGUI::deserialize(const QByteArray& data)
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
		quint32 u32tmp;
		qint32 tmp;
		bool boolTmp;

		blockApplySettings(true);
		m_channelMarker.blockSignals(true);

        d.readBlob(13, &bytetmp);
        m_channelMarker.deserialize(bytetmp);

		d.readS32(1, &tmp, 0);
		m_channelMarker.setCenterFrequency(tmp);
		d.readS32(2, &tmp, 4);
		ui->rfBW->setCurrentIndex(tmp);
		d.readS32(3, &tmp, 3);
		ui->afBW->setValue(tmp);
		d.readS32(4, &tmp, 20);
		ui->volume->setValue(tmp);
		d.readS32(5, &tmp, -40);
		ui->squelch->setValue(tmp);

		if(d.readU32(7, &u32tmp))
		{
			m_channelMarker.setColor(u32tmp);
		}

		d.readS32(8, &tmp, 0);
		ui->ctcss->setCurrentIndex(tmp);
		d.readBool(9, &boolTmp, false);
		ui->ctcssOn->setChecked(boolTmp);
		d.readBool(10, &boolTmp, false);
		ui->audioMute->setChecked(boolTmp);
		d.readS32(11, &tmp, 5);
		ui->squelchGate->setValue(tmp);
        d.readBool(12, &boolTmp, false);
        ui->deltaSquelch->setChecked(boolTmp);

        this->setWindowTitle(m_channelMarker.getTitle());
        displayUDPAddress();

		blockApplySettings(false);
		m_channelMarker.blockSignals(false);

		applySettings(true);
		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

bool NFMDemodGUI::handleMessage(const Message& message __attribute__((unused)))
{
    if (NFMDemod::MsgReportCTCSSFreq::match(message))
    {
        NFMDemod::MsgReportCTCSSFreq& report = (NFMDemod::MsgReportCTCSSFreq&) message;
        setCtcssFreq(report.getFrequency());
        //qDebug("NFMDemodGUI::handleMessage: MsgReportCTCSSFreq: %f", report.getFrequency());
        return true;
    }

    return false;
}

void NFMDemodGUI::handleInputMessages()
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

void NFMDemodGUI::channelMarkerChanged()
{
    this->setWindowTitle(m_channelMarker.getTitle());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    m_settings.m_udpAddress = m_channelMarker.getUDPAddress(),
    m_settings.m_udpPort =  m_channelMarker.getUDPSendPort(),
    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    displayUDPAddress();
    applySettings();
}

void NFMDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void NFMDemodGUI::on_rfBW_currentIndexChanged(int index)
{
	qDebug() << "NFMDemodGUI::on_rfBW_currentIndexChanged" << index;
	//ui->rfBWText->setText(QString("%1 k").arg(m_rfBW[value] / 1000.0));
	m_channelMarker.setBandwidth(NFMDemodSettings::getRFBW(index));
	m_settings.m_rfBandwidth = NFMDemodSettings::getRFBW(index);
	m_settings.m_fmDeviation = NFMDemodSettings::getFMDev(index);
	applySettings();
}

void NFMDemodGUI::on_afBW_valueChanged(int value)
{
	ui->afBWText->setText(QString("%1 k").arg(value));
	m_settings.m_afBandwidth = value * 1000.0;
	applySettings();
}

void NFMDemodGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_volume = value / 10.0;
	applySettings();
}

void NFMDemodGUI::on_squelchGate_valueChanged(int value)
{
    ui->squelchGateText->setText(QString("%1").arg(value * 10.0f, 0, 'f', 0));
    m_settings.m_squelchGate = value;
	applySettings();
}

void NFMDemodGUI::on_deltaSquelch_toggled(bool checked)
{
    if (checked)
    {
        ui->squelchText->setText(QString("%1").arg((-ui->squelch->value()) / 10.0, 0, 'f', 1));
        ui->squelchText->setToolTip(tr("Squelch AF balance threshold (%)"));
        ui->squelch->setToolTip(tr("Squelch AF balance threshold (%)"));
    }
    else
    {
        ui->squelchText->setText(QString("%1").arg(ui->squelch->value() / 10.0, 0, 'f', 1));
        ui->squelchText->setToolTip(tr("Squelch power threshold (dB)"));
        ui->squelch->setToolTip(tr("Squelch AF balance threshold (%)"));
    }
    m_settings.m_deltaSquelch = checked;
    applySettings();
}

void NFMDemodGUI::on_squelch_valueChanged(int value)
{
    if (ui->deltaSquelch->isChecked())
    {
        ui->squelchText->setText(QString("%1").arg(-value / 10.0, 0, 'f', 1));
        ui->squelchText->setToolTip(tr("Squelch deviation threshold (%)"));
    }
    else
    {
        ui->squelchText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
        ui->squelchText->setToolTip(tr("Squelch power threshold (dB)"));
    }
    m_settings.m_squelch = value * 1.0;
	applySettings();
}

void NFMDemodGUI::on_ctcssOn_toggled(bool checked)
{
	m_settings.m_ctcssOn = checked;
	applySettings();
}

void NFMDemodGUI::on_audioMute_toggled(bool checked)
{
	m_settings.m_audioMute = checked;
	applySettings();
}

void NFMDemodGUI::on_copyAudioToUDP_toggled(bool checked)
{
    m_settings.m_copyAudioToUDP = checked;
    applySettings();
}

void NFMDemodGUI::on_ctcss_currentIndexChanged(int index)
{
	m_settings.m_ctcssIndex = index;
	applySettings();
}

void NFMDemodGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
	/*
	if((widget == ui->spectrumContainer) && (m_nfmDemod != NULL))
		m_nfmDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/
}

void NFMDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    BasicChannelSettingsDialog dialog(&m_channelMarker, this);
    dialog.move(p);
    dialog.exec();
}

NFMDemodGUI::NFMDemodGUI(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::NFMDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceAPI(deviceAPI),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_doApplySettings(true),
	m_squelchOpen(false),
	m_tickCount(0)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);

	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_nfmDemod = new NFMDemod(m_deviceAPI);
	m_nfmDemod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    blockApplySettings(true);

    ui->rfBW->clear();

    for (int i = 0; i < m_nbRfBW; i++) {
        ui->rfBW->addItem(QString("%1").arg(m_rfBW[i] / 1000.0, 0, 'f', 2));
    }

    int ctcss_nbTones;
    const Real *ctcss_tones = m_nfmDemod->getCtcssToneSet(ctcss_nbTones);

    ui->ctcss->addItem("--");

    for (int i=0; i<ctcss_nbTones; i++)
    {
        ui->ctcss->addItem(QString("%1").arg(ctcss_tones[i]));
    }

    blockApplySettings(false);

	ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }"); // squelch closed

	ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
	ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

	//m_channelMarker = new ChannelMarker(this);
	m_channelMarker.setColor(Qt::red);
	m_channelMarker.setBandwidth(12500);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setVisible(true);
    m_channelMarker.setTitle("NFM Demodulator");
    m_channelMarker.setUDPAddress("127.0.0.1");
    m_channelMarker.setUDPSendPort(9999);
    m_channelMarker.setVisible(true);
    setTitleColor(m_channelMarker.getColor());

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(channelMarkerChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
	m_deviceAPI->addChannelMarker(&m_channelMarker);
	m_deviceAPI->addRollupWidget(this);

	QChar delta = QChar(0x94, 0x03);
	ui->deltaSquelch->setText(delta);

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	applySettings(true);
}

NFMDemodGUI::~NFMDemodGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	delete m_nfmDemod;
	//delete m_channelMarker;
	delete ui;
}

void NFMDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		qDebug() << "NFMDemodGUI::applySettings";

		setTitleColor(m_channelMarker.getColor());

        NFMDemod::MsgConfigureChannelizer* channelConfigMsg = NFMDemod::MsgConfigureChannelizer::create(
                48000, m_channelMarker.getCenterFrequency());
        m_nfmDemod->getInputMessageQueue()->push(channelConfigMsg);

        ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

        NFMDemod::MsgConfigureNFMDemod* message = NFMDemod::MsgConfigureNFMDemod::create( m_settings, force);
        m_nfmDemod->getInputMessageQueue()->push(message);
	}
}

void NFMDemodGUI::displayUDPAddress()
{
    ui->copyAudioToUDP->setToolTip(QString("Copy audio output to UDP %1:%2").arg(m_channelMarker.getUDPAddress()).arg(m_channelMarker.getUDPSendPort()));
}

void NFMDemodGUI::leaveEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(false);
	blockApplySettings(false);
}

void NFMDemodGUI::enterEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(true);
	blockApplySettings(false);
}

void NFMDemodGUI::setCtcssFreq(Real ctcssFreq)
{
	if (ctcssFreq == 0)
	{
		ui->ctcssText->setText("--");
	}
	else
	{
		ui->ctcssText->setText(QString("%1").arg(ctcssFreq));
	}
}

void NFMDemodGUI::blockApplySettings(bool block)
{
	m_doApplySettings = !block;
}

void NFMDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_nfmDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1 dB").arg(powDbAvg, 0, 'f', 1));
    }

    bool squelchOpen = m_nfmDemod->getSquelchOpen();

	if (squelchOpen != m_squelchOpen)
	{
		if (squelchOpen) {
			ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
		} else {
			ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
		}

        m_squelchOpen = squelchOpen;
	}

	m_tickCount++;
}
