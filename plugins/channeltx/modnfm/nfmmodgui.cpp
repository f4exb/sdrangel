///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <QDockWidget>
#include <QMainWindow>
#include <QFileDialog>
#include <QTime>
#include <QDebug>

#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

#include "ui_nfmmodgui.h"
#include "nfmmodgui.h"


NFMModGUI* NFMModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    NFMModGUI* gui = new NFMModGUI(pluginAPI, deviceUISet, channelTx);
	return gui;
}

void NFMModGUI::destroy()
{
    delete this;
}

void NFMModGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString NFMModGUI::getName() const
{
	return objectName();
}

qint64 NFMModGUI::getCenterFrequency() const {
	return m_channelMarker.getCenterFrequency();
}

void NFMModGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void NFMModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray NFMModGUI::serialize() const
{
    return m_settings.serialize();
}

bool NFMModGUI::deserialize(const QByteArray& data)
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

bool NFMModGUI::handleMessage(const Message& message)
{
    if (NFMMod::MsgReportFileSourceStreamData::match(message))
    {
        m_recordSampleRate = ((NFMMod::MsgReportFileSourceStreamData&)message).getSampleRate();
        m_recordLength = ((NFMMod::MsgReportFileSourceStreamData&)message).getRecordLength();
        m_samplesCount = 0;
        updateWithStreamData();
        return true;
    }
    else if (NFMMod::MsgReportFileSourceStreamTiming::match(message))
    {
        m_samplesCount = ((NFMMod::MsgReportFileSourceStreamTiming&)message).getSamplesCount();
        updateWithStreamTime();
        return true;
    }
    else if (NFMMod::MsgConfigureNFMMod::match(message))
    {
        const NFMMod::MsgConfigureNFMMod& cfg = (NFMMod::MsgConfigureNFMMod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (CWKeyer::MsgConfigureCWKeyer::match(message))
    {
        const CWKeyer::MsgConfigureCWKeyer& cfg = (CWKeyer::MsgConfigureCWKeyer&) message;
        ui->cwKeyerGUI->displaySettings(cfg.getSettings());
        return true;
    }
    else
    {
        return false;
    }
}

void NFMModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void NFMModGUI::handleSourceMessages()
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

void NFMModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void NFMModGUI::on_rfBW_currentIndexChanged(int index)
{
	m_channelMarker.setBandwidth(NFMModSettings::getRFBW(index));
	m_settings.m_rfBandwidth = NFMModSettings::getRFBW(index);
	applySettings();
}

void NFMModGUI::on_afBW_valueChanged(int value)
{
	ui->afBWText->setText(QString("%1k").arg(value));
	m_settings.m_afBandwidth = value * 1000.0;
	applySettings();
}

void NFMModGUI::on_fmDev_valueChanged(int value)
{
	ui->fmDevText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_fmDeviation = value * 100.0;
	applySettings();
}

void NFMModGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_volumeFactor = value / 10.0;
	applySettings();
}

void NFMModGUI::on_toneFrequency_valueChanged(int value)
{
    ui->toneFrequencyText->setText(QString("%1k").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_toneFrequency = value * 10.0;
    applySettings();
}

void NFMModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
	applySettings();
}

void NFMModGUI::on_playLoop_toggled(bool checked)
{
    m_settings.m_playLoop = checked;
	applySettings();
}

void NFMModGUI::on_play_toggled(bool checked)
{
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    ui->morseKeyer->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? NFMModSettings::NFMModInputFile : NFMModSettings::NFMModInputNone;
    applySettings();
    ui->navTimeSlider->setEnabled(!checked);
    m_enableNavTime = !checked;
}

void NFMModGUI::on_tone_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    ui->morseKeyer->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? NFMModSettings::NFMModInputTone : NFMModSettings::NFMModInputNone;
    applySettings();
}

void NFMModGUI::on_morseKeyer_toggled(bool checked)
{
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    ui->play->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? NFMModSettings::NFMModInputCWTone : NFMModSettings::NFMModInputNone;
    applySettings();
}

void NFMModGUI::on_mic_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? NFMModSettings::NFMModInputAudio : NFMModSettings::NFMModInputNone;
    applySettings();
}

void NFMModGUI::on_navTimeSlider_valueChanged(int value)
{
    if (m_enableNavTime && ((value >= 0) && (value <= 100)))
    {
        int t_sec = (m_recordLength * value) / 100;
        QTime t(0, 0, 0, 0);
        t = t.addSecs(t_sec);

        NFMMod::MsgConfigureFileSourceSeek* message = NFMMod::MsgConfigureFileSourceSeek::create(value);
        m_nfmMod->getInputMessageQueue()->push(message);
    }
}

void NFMModGUI::on_showFileDialog_clicked(bool checked __attribute__((unused)))
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open raw audio file"), ".", tr("Raw audio Files (*.raw)"));

    if (fileName != "")
    {
        m_fileName = fileName;
        ui->recordFileText->setText(m_fileName);
        ui->play->setEnabled(true);
        configureFileName();
    }
}

void NFMModGUI::on_ctcss_currentIndexChanged(int index)
{
    m_settings.m_ctcssIndex = index;
    applySettings();
}

void NFMModGUI::on_ctcssOn_toggled(bool checked)
{
    m_settings.m_ctcssOn = checked;
    applySettings();
}


void NFMModGUI::configureFileName()
{
    qDebug() << "FileSourceGui::configureFileName: " << m_fileName.toStdString().c_str();
    NFMMod::MsgConfigureFileSourceName* message = NFMMod::MsgConfigureFileSourceName::create(m_fileName);
    m_nfmMod->getInputMessageQueue()->push(message);
}

void NFMModGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
}

NFMModGUI::NFMModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::NFMModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
	m_doApplySettings(true),
	m_channelPowerDbAvg(20,0),
    m_recordLength(0),
    m_recordSampleRate(48000),
    m_samplesCount(0),
    m_tickCount(0),
    m_enableNavTime(false)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);

    blockApplySettings(true);

    ui->rfBW->clear();
    for (int i = 0; i < NFMModSettings::m_nbRfBW; i++) {
        ui->rfBW->addItem(QString("%1").arg(NFMModSettings::getRFBW(i) / 1000.0, 0, 'f', 2));
    }
    ui->rfBW->setCurrentIndex(6);

    blockApplySettings(false);

	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

	m_nfmMod = (NFMMod*) channelTx; //new NFMMod(m_deviceUISet->m_deviceSinkAPI);
	m_nfmMod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::red);
    m_channelMarker.setBandwidth(12500);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("NFM Modulator");
    m_channelMarker.setUDPAddress("127.0.0.1");
    m_channelMarker.setUDPSendPort(9999);
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	m_deviceUISet->registerTxChannelInstance(NFMMod::m_channelIdURI, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    ui->play->setEnabled(false);
    ui->play->setChecked(false);
    ui->tone->setChecked(false);
    ui->mic->setChecked(false);

    for (int i=0; i< NFMModSettings::m_nbCTCSSFreqs; i++)
    {
        ui->ctcss->addItem(QString("%1").arg((double) NFMModSettings::getCTCSSFreq(i), 0, 'f', 1));
    }

    ui->cwKeyerGUI->setBuddies(m_nfmMod->getInputMessageQueue(), m_nfmMod->getCWKeyer());

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	connect(m_nfmMod, SIGNAL(levelChanged(qreal, qreal, int)), ui->volumeMeter, SLOT(levelChanged(qreal, qreal, int)));

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setCWKeyerGUI(ui->cwKeyerGUI);

    displaySettings();
    applySettings();
}

NFMModGUI::~NFMModGUI()
{
    m_deviceUISet->removeTxChannelInstance(this);
	delete m_nfmMod; // TODO: check this: when the GUI closes it has to delete the modulator
	delete ui;
}

void NFMModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void NFMModGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		NFMMod::MsgConfigureChannelizer *msgChan = NFMMod::MsgConfigureChannelizer::create(
		        48000, m_channelMarker.getCenterFrequency());
		m_nfmMod->getInputMessageQueue()->push(msgChan);

		NFMMod::MsgConfigureNFMMod *msg = NFMMod::MsgConfigureNFMMod::create(m_settings, force);
		m_nfmMod->getInputMessageQueue()->push(msg);
	}
}

void NFMModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBW->setCurrentIndex(NFMModSettings::getRFBWIndex(m_settings.m_rfBandwidth));

    ui->afBWText->setText(QString("%1k").arg(m_settings.m_afBandwidth / 1000.0));
    ui->afBW->setValue(m_settings.m_afBandwidth / 1000.0);

    ui->fmDevText->setText(QString("%1k").arg(m_settings.m_fmDeviation / 1000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 100.0);

    ui->volumeText->setText(QString("%1").arg(m_settings.m_volumeFactor, 0, 'f', 1));
    ui->volume->setValue(m_settings.m_volumeFactor * 10.0);

    ui->toneFrequencyText->setText(QString("%1k").arg(m_settings.m_toneFrequency / 1000.0, 0, 'f', 2));
    ui->toneFrequency->setValue(m_settings.m_toneFrequency / 10.0);

    ui->ctcssOn->setChecked(m_settings.m_ctcssOn);
    ui->ctcss->setCurrentIndex(m_settings.m_ctcssIndex);

    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->playLoop->setChecked(m_settings.m_playLoop);

    ui->tone->setEnabled((m_settings.m_modAFInput == NFMModSettings::NFMModInputAF::NFMModInputTone) || (m_settings.m_modAFInput == NFMModSettings::NFMModInputAF::NFMModInputNone));
    ui->mic->setEnabled((m_settings.m_modAFInput == NFMModSettings::NFMModInputAF::NFMModInputAudio) || (m_settings.m_modAFInput == NFMModSettings::NFMModInputAF::NFMModInputNone));
    ui->play->setEnabled((m_settings.m_modAFInput == NFMModSettings::NFMModInputAF::NFMModInputFile) || (m_settings.m_modAFInput == NFMModSettings::NFMModInputAF::NFMModInputNone));
    ui->morseKeyer->setEnabled((m_settings.m_modAFInput == NFMModSettings::NFMModInputAF::NFMModInputCWTone) || (m_settings.m_modAFInput == NFMModSettings::NFMModInputAF::NFMModInputNone));

    ui->tone->setChecked(m_settings.m_modAFInput == NFMModSettings::NFMModInputAF::NFMModInputTone);
    ui->mic->setChecked(m_settings.m_modAFInput == NFMModSettings::NFMModInputAF::NFMModInputAudio);
    ui->play->setChecked(m_settings.m_modAFInput == NFMModSettings::NFMModInputAF::NFMModInputFile);
    ui->morseKeyer->setChecked(m_settings.m_modAFInput == NFMModSettings::NFMModInputAF::NFMModInputCWTone);

    blockApplySettings(false);
}


void NFMModGUI::leaveEvent(QEvent*)
{
	m_channelMarker.setHighlighted(false);
}

void NFMModGUI::enterEvent(QEvent*)
{
	m_channelMarker.setHighlighted(true);
}

void NFMModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_nfmMod->getMagSq());
	m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.average(), 0, 'f', 1));

    if (((++m_tickCount & 0xf) == 0) && (m_settings.m_modAFInput == NFMModSettings::NFMModInputFile))
    {
        NFMMod::MsgConfigureFileSourceStreamTiming* message = NFMMod::MsgConfigureFileSourceStreamTiming::create();
        m_nfmMod->getInputMessageQueue()->push(message);
    }
}

void NFMModGUI::updateWithStreamData()
{
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_recordLength);
    QString s_time = recordLength.toString("hh:mm:ss");
    ui->recordLengthText->setText(s_time);
    updateWithStreamTime();
}

void NFMModGUI::updateWithStreamTime()
{
    int t_sec = 0;
    int t_msec = 0;

    if (m_recordSampleRate > 0)
    {
        t_msec = ((m_samplesCount * 1000) / m_recordSampleRate) % 1000;
        t_sec = m_samplesCount / m_recordSampleRate;
    }

    QTime t(0, 0, 0, 0);
    t = t.addSecs(t_sec);
    t = t.addMSecs(t_msec);
    QString s_timems = t.toString("hh:mm:ss.zzz");
    QString s_time = t.toString("hh:mm:ss");
    ui->relTimeText->setText(s_timems);

    if (!m_enableNavTime)
    {
        float posRatio = (float) t_sec / (float) m_recordLength;
        ui->navTimeSlider->setValue((int) (posRatio * 100.0));
    }
}
