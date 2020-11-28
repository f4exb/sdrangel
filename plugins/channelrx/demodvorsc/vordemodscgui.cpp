///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <limits>

#include <QDebug>

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "util/morse.h"
#include "util/units.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"

#include "ui_vordemodscgui.h"
#include "vordemodsc.h"
#include "vordemodscreport.h"
#include "vordemodscgui.h"

VORDemodSCGUI* VORDemodSCGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    VORDemodSCGUI* gui = new VORDemodSCGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void VORDemodSCGUI::destroy()
{
    delete this;
}

void VORDemodSCGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray VORDemodSCGUI::serialize() const
{
    return m_settings.serialize();
}

bool VORDemodSCGUI::deserialize(const QByteArray& data)
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

bool VORDemodSCGUI::handleMessage(const Message& message)
{
    if (VORDemodSC::MsgConfigureVORDemod::match(message))
    {
        qDebug("VORDemodSCGUI::handleMessage: VORDemodSC::MsgConfigureVORDemod");
        const VORDemodSC::MsgConfigureVORDemod& cfg = (VORDemodSC::MsgConfigureVORDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_basebandSampleRate = notif.getSampleRate();
        return true;
    }
    else if (VORDemodSCReport::MsgReportRadial::match(message))
    {
        VORDemodSCReport::MsgReportRadial& report = (VORDemodSCReport::MsgReportRadial&) message;

        // Display radial and signal magnitudes
        Real varMagDB = std::round(20.0*std::log10(report.getVarMag()));
        Real refMagDB = std::round(20.0*std::log10(report.getRefMag()));

        bool validRadial = (refMagDB > m_settings.m_refThresholdDB) && (varMagDB > m_settings.m_varThresholdDB);

        ui->radialText->setText(tr("%1°").arg(std::round(report.getRadial())));

        if (validRadial) {
            ui->radialText->setStyleSheet("QLabel { color: white }");
        } else {
            ui->radialText->setStyleSheet("QLabel { color: red }");
        }

        ui->refText->setText(tr("%1 dB").arg(refMagDB));

        if (refMagDB > m_settings.m_refThresholdDB) {
            ui->refText->setStyleSheet("QLabel { color: white }");
        } else {
            ui->refText->setStyleSheet("QLabel { color: red }");
        }

        ui->varText->setText(tr("%1 dB").arg(varMagDB));

        if (varMagDB > m_settings.m_varThresholdDB) {
            ui->varText->setStyleSheet("QLabel { color: white }");
        } else {
            ui->varText->setStyleSheet("QLabel { color: red }");
        }

        return true;
    }
    else if (VORDemodSCReport::MsgReportIdent::match(message))
    {
        VORDemodSCReport::MsgReportIdent& report = (VORDemodSCReport::MsgReportIdent&) message;

        QString ident = report.getIdent();
        QString identString = Morse::toString(ident); // Convert Morse to a string

        ui->identText->setText(identString);
        ui->morseText->setText(Morse::toSpacedUnicode(ident));

        // Idents should only be two or three characters, so filter anything else
        // other than TEST which indicates a VOR is under maintainance (may also be TST)
        if (((identString.size() >= 2) && (identString.size() <= 3)) || (identString == "TEST"))
        {
            ui->identText->setStyleSheet("QLabel { color: white }");
            ui->morseText->setStyleSheet("QLabel { color: white }");
        }
        else
        {
            ui->identText->setStyleSheet("QLabel { color: yellow }");
            ui->morseText->setStyleSheet("QLabel { color: yellow }");
        }

        return true;
    }

    return false;
}

void VORDemodSCGUI::handleInputMessages()
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

void VORDemodSCGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void VORDemodSCGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void VORDemodSCGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void VORDemodSCGUI::on_thresh_valueChanged(int value)
{
    ui->threshText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_identThreshold = value / 10.0;
    applySettings();
}

void VORDemodSCGUI::on_volume_valueChanged(int value)
{
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_volume = value / 10.0;
    applySettings();
}

void VORDemodSCGUI::on_squelch_valueChanged(int value)
{
    ui->squelchText->setText(QString("%1 dB").arg(value));
    m_settings.m_squelch = value;
    applySettings();
}

void VORDemodSCGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
    applySettings();
}

void VORDemodSCGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

void VORDemodSCGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }
    else if ((m_contextMenuType == ContextMenuStreamSettings) && (m_deviceUISet->m_deviceMIMOEngine))
    {
        DeviceStreamSelectionDialog dialog(this);
        dialog.setNumberOfStreams(m_vorDemod->getNumberOfDeviceStreams());
        dialog.setStreamIndex(m_settings.m_streamIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
        displayStreamIndex();
        applySettings();
    }

    resetContextMenuType();
}

VORDemodSCGUI::VORDemodSCGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::VORDemodSCGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_doApplySettings(true),
    m_squelchOpen(false),
    m_tickCount(0)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose, true);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_vorDemod = reinterpret_cast<VORDemodSC*>(rxChannel);
    m_vorDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

	m_channelMarker.blockSignals(true);
	m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(2*VORDemodSCSettings::VORDEMOD_CHANNEL_BANDWIDTH);
	m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("VOR Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);

    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    displaySettings();
    applySettings(true);
}

VORDemodSCGUI::~VORDemodSCGUI()
{
    delete ui;
}

void VORDemodSCGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void VORDemodSCGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        VORDemodSC::MsgConfigureVORDemod* message = VORDemodSC::MsgConfigureVORDemod::create( m_settings, force);
        m_vorDemod->getInputMessageQueue()->push(message);
    }
}

void VORDemodSCGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(2*VORDemodSCSettings::VORDEMOD_CHANNEL_BANDWIDTH);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->thresh->setValue(m_settings.m_identThreshold * 10.0);
    ui->threshText->setText(QString("%1").arg(m_settings.m_identThreshold, 0, 'f', 1));

    ui->volume->setValue(m_settings.m_volume * 10.0);
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 0, 'f', 1));

    ui->squelch->setValue(m_settings.m_squelch);
    ui->squelchText->setText(QString("%1 dB").arg(m_settings.m_squelch));

    ui->audioMute->setChecked(m_settings.m_audioMute);

    displayStreamIndex();

    blockApplySettings(false);
}

void VORDemodSCGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void VORDemodSCGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void VORDemodSCGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
}

void VORDemodSCGUI::audioSelect()
{
    qDebug("VORDemodSCGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void VORDemodSCGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_vorDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    int audioSampleRate = m_vorDemod->getAudioSampleRate();
    bool squelchOpen = m_vorDemod->getSquelchOpen();

    if (squelchOpen != m_squelchOpen)
    {
        if (audioSampleRate < 0) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : red; }");
        } else if (squelchOpen) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }

        m_squelchOpen = squelchOpen;
    }

    m_tickCount++;
}
