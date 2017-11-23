#include "wfmdemodgui.h"

#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"
#include <dsp/downchannelizer.h>
#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>

#include "ui_wfmdemodgui.h"
#include "dsp/dspengine.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "mainwindow.h"

#include "wfmdemod.h"

WFMDemodGUI* WFMDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	WFMDemodGUI* gui = new WFMDemodGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void WFMDemodGUI::destroy()
{
	delete this;
}

void WFMDemodGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString WFMDemodGUI::getName() const
{
	return objectName();
}

qint64 WFMDemodGUI::getCenterFrequency() const
{
	return m_channelMarker.getCenterFrequency();
}

void WFMDemodGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void WFMDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings();
}

QByteArray WFMDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool WFMDemodGUI::deserialize(const QByteArray& data)
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

bool WFMDemodGUI::handleMessage(const Message& message __attribute__((unused)))
{
	return false;
}

void WFMDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void WFMDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void WFMDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void WFMDemodGUI::on_rfBW_currentIndexChanged(int index)
{
    m_channelMarker.setBandwidth(WFMDemodSettings::getRFBW(index));
    m_settings.m_rfBandwidth = WFMDemodSettings::getRFBW(index);
    applySettings();
}

void WFMDemodGUI::on_afBW_valueChanged(int value)
{
    ui->afBWText->setText(QString("%1 kHz").arg(value));
    m_settings.m_afBandwidth = value * 1000.0;
	applySettings();
}

void WFMDemodGUI::on_volume_valueChanged(int value)
{
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_volume = value / 10.0;
	applySettings();
}

void WFMDemodGUI::on_squelch_valueChanged(int value)
{
	ui->squelchText->setText(QString("%1 dB").arg(value));
    m_settings.m_squelch = value;
	applySettings();
}

void WFMDemodGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
    applySettings();
}

void WFMDemodGUI::on_copyAudioToUDP_toggled(bool checked)
{
    m_settings.m_copyAudioToUDP = checked;
    applySettings();
}

void WFMDemodGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
}

void WFMDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    BasicChannelSettingsDialog dialog(&m_channelMarker, this);
    dialog.move(p);
    dialog.exec();

    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    m_settings.m_udpAddress = m_channelMarker.getUDPAddress(),
    m_settings.m_udpPort =  m_channelMarker.getUDPSendPort(),
    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    m_settings.m_title = m_channelMarker.getTitle();

    setWindowTitle(m_settings.m_title);
    setTitleColor(m_settings.m_rgbColor);
    displayUDPAddress();

    applySettings();
}

WFMDemodGUI::WFMDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::WFMDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_channelPowerDbAvg(20,0)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_wfmDemod = (WFMDemod*) rxChannel; //new WFMDemod(m_deviceUISet->m_deviceSourceAPI);

	connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    blockApplySettings(true);
    ui->rfBW->clear();
    for (int i = 0; i < WFMDemodSettings::m_nbRFBW; i++) {
        ui->rfBW->addItem(QString("%1").arg(WFMDemodSettings::getRFBW(i) / 1000.0, 0, 'f', 2));
    }
    ui->rfBW->setCurrentIndex(6);
    blockApplySettings(false);

    m_channelMarker.blockSignals(true);
	m_channelMarker.setBandwidth(WFMDemodSettings::getRFBW(4));
	m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("WFM Demodulator");
    m_channelMarker.setUDPAddress("127.0.0.1");
    m_channelMarker.setUDPSendPort(9999);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);

	m_deviceUISet->registerRxChannelInstance(WFMDemod::m_channelIdURI, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

    displaySettings();
	applySettings(true);
}

WFMDemodGUI::~WFMDemodGUI()
{
    m_deviceUISet->removeRxChannelInstance(this);
	delete m_wfmDemod; // TODO: check this: when the GUI closes it has to delete the demodulator
	//delete m_channelMarker;
	delete ui;
}

void WFMDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void WFMDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        WFMDemod::MsgConfigureChannelizer *msgChan = WFMDemod::MsgConfigureChannelizer::create(
                requiredBW(WFMDemodSettings::getRFBW(ui->rfBW->currentIndex())),
                m_channelMarker.getCenterFrequency());
        m_wfmDemod->getInputMessageQueue()->push(msgChan);

        WFMDemod::MsgConfigureWFMDemod* msgConfig = WFMDemod::MsgConfigureWFMDemod::create( m_settings, force);
        m_wfmDemod->getInputMessageQueue()->push(msgConfig);
	}
}

void WFMDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    displayUDPAddress();

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBW->setCurrentIndex(WFMDemodSettings::getRFBWIndex(m_settings.m_rfBandwidth));

    ui->afBW->setValue(m_settings.m_afBandwidth/1000.0);
    ui->afBWText->setText(QString("%1 kHz").arg(m_settings.m_afBandwidth/1000.0));

    ui->volume->setValue(m_settings.m_volume * 10.0);
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 0, 'f', 1));

    ui->squelch->setValue(m_settings.m_squelch);
    ui->squelchText->setText(QString("%1 dB").arg(m_settings.m_squelch));

    blockApplySettings(false);
}

void WFMDemodGUI::displayUDPAddress()
{
    ui->copyAudioToUDP->setToolTip(QString("Copy audio output to UDP %1:%2").arg(m_settings.m_udpAddress).arg(m_settings.m_udpPort));
}

void WFMDemodGUI::leaveEvent(QEvent*)
{
	m_channelMarker.setHighlighted(false);
}

void WFMDemodGUI::enterEvent(QEvent*)
{
	m_channelMarker.setHighlighted(true);
}

void WFMDemodGUI::tick()
{
//	Real powDb = CalcDb::dbPower(m_wfmDemod->getMagSq());
//	m_channelPowerDbAvg.feed(powDb);
//	ui->channelPower->setText(QString::number(m_channelPowerDbAvg.average(), 'f', 1));

    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_wfmDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    bool squelchOpen = m_wfmDemod->getSquelchOpen();

    if (squelchOpen != m_squelchOpen)
    {
        if (squelchOpen) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }

        m_squelchOpen = squelchOpen;
    }
}
