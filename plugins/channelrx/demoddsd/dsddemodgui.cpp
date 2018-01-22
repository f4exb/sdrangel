///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "dsddemodgui.h"

#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"
#include <dsp/downchannelizer.h>
#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>

#include "dsp/threadedbasebandsamplesink.h"
#include "ui_dsddemodgui.h"
#include "dsp/scopevis.h"
#include "gui/glscope.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

#include "dsddemodbaudrates.h"
#include "dsddemod.h"

DSDDemodGUI* DSDDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    DSDDemodGUI* gui = new DSDDemodGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void DSDDemodGUI::destroy()
{
	delete this;
}

void DSDDemodGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString DSDDemodGUI::getName() const
{
	return objectName();
}

qint64 DSDDemodGUI::getCenterFrequency() const
{
	return m_channelMarker.getCenterFrequency();
}

void DSDDemodGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void DSDDemodGUI::resetToDefaults()
{
	m_settings.resetToDefaults();
	blockApplySettings(true);
	displaySettings();
	blockApplySettings(false);
	applySettings();
}

QByteArray DSDDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool DSDDemodGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool DSDDemodGUI::handleMessage(const Message& message __attribute__((unused)))
{
	return false;
}

void DSDDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void DSDDemodGUI::on_rfBW_valueChanged(int value)
{
	m_channelMarker.setBandwidth(value * 100);
	m_settings.m_rfBandwidth = value * 100.0;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void DSDDemodGUI::on_demodGain_valueChanged(int value)
{
    m_settings.m_demodGain = value / 100.0;
    ui->demodGainText->setText(QString("%1").arg(value / 100.0, 0, 'f', 2));
	applySettings();
}

void DSDDemodGUI::on_fmDeviation_valueChanged(int value)
{
    m_settings.m_fmDeviation = value * 100.0;
    ui->fmDeviationText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void DSDDemodGUI::on_volume_valueChanged(int value)
{
    m_settings.m_volume= value / 10.0;
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    applySettings();
}

void DSDDemodGUI::on_baudRate_currentIndexChanged(int index)
{
    m_settings.m_baudRate = DSDDemodBaudRates::getRate(index);
    applySettings();
}

void DSDDemodGUI::on_enableCosineFiltering_toggled(bool enable)
{
    m_settings.m_enableCosineFiltering = enable;
	applySettings();
}

void DSDDemodGUI::on_syncOrConstellation_toggled(bool checked)
{
    m_settings.m_syncOrConstellation = checked;
    applySettings();
}

void DSDDemodGUI::on_slot1On_toggled(bool checked)
{
    m_settings.m_slot1On = checked;
    applySettings();
}

void DSDDemodGUI::on_slot2On_toggled(bool checked)
{
    m_settings.m_slot2On = checked;
    applySettings();
}

void DSDDemodGUI::on_tdmaStereoSplit_toggled(bool checked)
{
    m_settings.m_tdmaStereo = checked;
    applySettings();
}

void DSDDemodGUI::on_squelchGate_valueChanged(int value)
{
    m_settings.m_squelchGate = value;
    ui->squelchGateText->setText(QString("%1").arg(value * 10.0, 0, 'f', 0));
	applySettings();
}

void DSDDemodGUI::on_squelch_valueChanged(int value)
{
	ui->squelchText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_squelch = value / 10.0;
	applySettings();
}

void DSDDemodGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
    applySettings();
}

void DSDDemodGUI::on_highPassFilter_toggled(bool checked)
{
    m_settings.m_highPassFilter = checked;
    applySettings();
}

void DSDDemodGUI::on_symbolPLLLock_toggled(bool checked)
{
    if (checked) {
        ui->symbolPLLLock->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    } else {
        ui->symbolPLLLock->setStyleSheet("QToolButton { background:rgb(53,53,53); }");
    }
    m_settings.m_pllLock = checked;
    applySettings();
}

void DSDDemodGUI::on_udpOutput_toggled(bool checked)
{
    m_settings.m_udpCopyAudio = checked;
    applySettings();
}

void DSDDemodGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
	/*
	if((widget == ui->spectrumContainer) && (DSDDemodGUI != NULL))
		m_dsdDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/
}

void DSDDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    //qDebug("DSDDemodGUI::onMenuDialogCalled: x: %d y: %d", p.x(), p.y());
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

DSDDemodGUI::DSDDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::DSDDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
	m_doApplySettings(true),
	m_signalFormat(signalFormatNone),
	m_enableCosineFiltering(false),
	m_syncOrConstellation(false),
	m_slot1On(false),
	m_slot2On(false),
	m_tdmaStereo(false),
	m_squelchOpen(false),
	m_tickCount(0)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);

	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_scopeVis = new ScopeVis(SDR_RX_SCALEF, ui->glScope);
	m_dsdDemod = (DSDDemod*) rxChannel; //new DSDDemod(m_deviceUISet->m_deviceSourceAPI);
	m_dsdDemod->setScopeSink(m_scopeVis);
	m_dsdDemod->setMessageQueueToGUI(getInputMessageQueue());

    ui->glScope->setSampleRate(48000);
    m_scopeVis->setSampleRate(48000);

	ui->glScope->connectTimer(MainWindow::getInstance()->getMasterTimer());

	connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
	ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
	m_channelMarker.setColor(Qt::cyan);
	m_channelMarker.setBandwidth(10000);
	m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("DSD Demodulator");
    m_channelMarker.setUDPAddress("127.0.0.1");
    m_channelMarker.setUDPSendPort(9999);
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	m_deviceUISet->registerRxChannelInstance(DSDDemod::m_channelIdURI, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

	ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

	m_settings.setChannelMarker(&m_channelMarker);
	m_settings.setScopeGUI(ui->scopeGUI);

	updateMyPosition();
	displaySettings();
	applySettings(true);
}

DSDDemodGUI::~DSDDemodGUI()
{
    m_deviceUISet->removeRxChannelInstance(this);
	delete m_dsdDemod; // TODO: check this: when the GUI closes it has to delete the demodulator
	delete ui;
}

void DSDDemodGUI::updateMyPosition()
{
    float latitude = MainWindow::getInstance()->getMainSettings().getLatitude();
    float longitude = MainWindow::getInstance()->getMainSettings().getLongitude();

    if ((m_myLatitude != latitude) || (m_myLongitude != longitude))
    {
        m_dsdDemod->configureMyPosition(m_dsdDemod->getInputMessageQueue(), latitude, longitude);
        m_myLatitude = latitude;
        m_myLongitude = longitude;
    }
}

void DSDDemodGUI::displayUDPAddress()
{
    ui->udpOutput->setToolTip(QString("Copy audio output to UDP %1:%2").arg(m_settings.m_udpAddress).arg(m_settings.m_udpPort));
}

void DSDDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    setTitleColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    displayUDPAddress();

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);
    ui->rfBWText->setText(QString("%1k").arg(ui->rfBW->value() / 10.0, 0, 'f', 1));

    ui->fmDeviation->setValue(m_settings.m_fmDeviation / 100.0);
    ui->fmDeviationText->setText(QString("%1k").arg(ui->fmDeviation->value() / 10.0, 0, 'f', 1));

    ui->squelch->setValue(m_settings.m_squelch * 10.0);
    ui->squelchText->setText(QString("%1").arg(ui->squelch->value() / 10.0, 0, 'f', 1));

    ui->squelchGate->setValue(m_settings.m_squelchGate);
    ui->squelchGateText->setText(QString("%1").arg(ui->squelchGate->value() * 10.0, 0, 'f', 0));

    ui->demodGain->setValue(m_settings.m_demodGain * 100.0);
    ui->demodGainText->setText(QString("%1").arg(ui->demodGain->value() / 100.0, 0, 'f', 2));

    ui->volume->setValue(m_settings.m_volume * 10.0);
    ui->volumeText->setText(QString("%1").arg(ui->volume->value() / 10.0, 0, 'f', 1));

    ui->enableCosineFiltering->setChecked(m_settings.m_enableCosineFiltering);
    ui->syncOrConstellation->setChecked(m_settings.m_syncOrConstellation);
    ui->slot1On->setChecked(m_settings.m_slot1On);
    ui->slot2On->setChecked(m_settings.m_slot2On);
    ui->tdmaStereoSplit->setChecked(m_settings.m_tdmaStereo);
    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->udpOutput->setChecked(m_settings.m_udpCopyAudio);
    ui->symbolPLLLock->setChecked(m_settings.m_pllLock);

    ui->baudRate->setCurrentIndex(DSDDemodBaudRates::getRateIndex(m_settings.m_baudRate));

    blockApplySettings(false);
}

void DSDDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		qDebug() << "DSDDemodGUI::applySettings";

        DSDDemod::MsgConfigureChannelizer* channelConfigMsg = DSDDemod::MsgConfigureChannelizer::create(
                48000, m_channelMarker.getCenterFrequency());
        m_dsdDemod->getInputMessageQueue()->push(channelConfigMsg);

        DSDDemod::MsgConfigureDSDDemod* message = DSDDemod::MsgConfigureDSDDemod::create( m_settings, force);
        m_dsdDemod->getInputMessageQueue()->push(message);
	}
}

void DSDDemodGUI::leaveEvent(QEvent*)
{
	m_channelMarker.setHighlighted(false);
}

void DSDDemodGUI::enterEvent(QEvent*)
{
	m_channelMarker.setHighlighted(true);
}

void DSDDemodGUI::blockApplySettings(bool block)
{
	m_doApplySettings = !block;
}

void DSDDemodGUI::formatStatusText()
{
    switch (m_dsdDemod->getDecoder().getSyncType())
    {
    case DSDcc::DSDDecoder::DSDSyncDMRDataMS:
    case DSDcc::DSDDecoder::DSDSyncDMRDataP:
    case DSDcc::DSDDecoder::DSDSyncDMRVoiceMS:
    case DSDcc::DSDDecoder::DSDSyncDMRVoiceP:
        if (m_signalFormat != signalFormatDMR)
        {
            strcpy(m_formatStatusText, "Sta: __ S1: __________________________ S2: __________________________");
        }

        switch (m_dsdDemod->getDecoder().getStationType())
        {
        case DSDcc::DSDDecoder::DSDBaseStation:
            memcpy(&m_formatStatusText[5], "BS ", 3);
            break;
        case DSDcc::DSDDecoder::DSDMobileStation:
            memcpy(&m_formatStatusText[5], "MS ", 3);
            break;
        default:
            memcpy(&m_formatStatusText[5], "NA ", 3);
            break;
        }

        memcpy(&m_formatStatusText[12], m_dsdDemod->getDecoder().getDMRDecoder().getSlot0Text(), 26);
        memcpy(&m_formatStatusText[43], m_dsdDemod->getDecoder().getDMRDecoder().getSlot1Text(), 26);
        m_signalFormat = signalFormatDMR;
        break;
    case DSDcc::DSDDecoder::DSDSyncDStarHeaderN:
    case DSDcc::DSDDecoder::DSDSyncDStarHeaderP:
    case DSDcc::DSDDecoder::DSDSyncDStarN:
    case DSDcc::DSDDecoder::DSDSyncDStarP:
        if (m_signalFormat != signalFormatDStar)
        {
                                     //           1    1    2    2    3    3    4    4    5    5    6    6    7    7    8
                                     // 0....5....0....5....0....5....0....5....0....5....0....5....0....5....0....5....0..
            strcpy(m_formatStatusText, "________/____>________|________>________|____________________|______:___/_____._");
                                     // MY            UR       RPT1     RPT2     Info                 Loc    Target
        }

        {
            const std::string& rpt1 = m_dsdDemod->getDecoder().getDStarDecoder().getRpt1();
            const std::string& rpt2 = m_dsdDemod->getDecoder().getDStarDecoder().getRpt2();
            const std::string& mySign = m_dsdDemod->getDecoder().getDStarDecoder().getMySign();
            const std::string& yrSign = m_dsdDemod->getDecoder().getDStarDecoder().getYourSign();

            if (rpt1.length() > 0) { // 0 or 8
                memcpy(&m_formatStatusText[23], rpt1.c_str(), 8);
            }
            if (rpt2.length() > 0) { // 0 or 8
                memcpy(&m_formatStatusText[32], rpt2.c_str(), 8);
            }
            if (yrSign.length() > 0) { // 0 or 8
                memcpy(&m_formatStatusText[14], yrSign.c_str(), 8);
            }
            if (mySign.length() > 0) { // 0 or 13
                memcpy(&m_formatStatusText[0], mySign.c_str(), 13);
            }
            memcpy(&m_formatStatusText[41], m_dsdDemod->getDecoder().getDStarDecoder().getInfoText(), 20);
            memcpy(&m_formatStatusText[62], m_dsdDemod->getDecoder().getDStarDecoder().getLocator(), 6);
            snprintf(&m_formatStatusText[69], 82-69, "%03d/%07.1f",
                    m_dsdDemod->getDecoder().getDStarDecoder().getBearing(),
                    m_dsdDemod->getDecoder().getDStarDecoder().getDistance());
        }

        m_formatStatusText[82] = '\0';
        m_signalFormat = signalFormatDStar;
        break;
    case DSDcc::DSDDecoder::DSDSyncDPMR:
        snprintf(m_formatStatusText, 82, "%s CC: %04d OI: %08d CI: %08d",
                DSDcc::DSDdPMR::dpmrFrameTypes[(int) m_dsdDemod->getDecoder().getDPMRDecoder().getFrameType()],
                m_dsdDemod->getDecoder().getDPMRDecoder().getColorCode(),
                m_dsdDemod->getDecoder().getDPMRDecoder().getOwnId(),
                m_dsdDemod->getDecoder().getDPMRDecoder().getCalledId());
        m_signalFormat = signalFormatDPMR;
        break;
    case DSDcc::DSDDecoder::DSDSyncYSF:
        //           1    1    2    2    3    3    4    4    5    5    6    6    7    7    8
        // 0....5....0....5....0....5....0....5....0....5....0....5....0....5....0....5....0..
        // C V2 RI 0:7 WL000|ssssssssss>dddddddddd |UUUUUUUUUU>DDDDDDDDDD|44444
    	if (m_dsdDemod->getDecoder().getYSFDecoder().getFICHError() == DSDcc::DSDYSF::FICHNoError)
    	{
            snprintf(m_formatStatusText, 82, "%s ", DSDcc::DSDYSF::ysfChannelTypeText[(int) m_dsdDemod->getDecoder().getYSFDecoder().getFICH().getFrameInformation()]);
    	}
    	else
    	{
            snprintf(m_formatStatusText, 82, "%d ", (int) m_dsdDemod->getDecoder().getYSFDecoder().getFICHError());
    	}

        snprintf(&m_formatStatusText[2], 80, "%s %s %d:%d %c%c",
                DSDcc::DSDYSF::ysfDataTypeText[(int) m_dsdDemod->getDecoder().getYSFDecoder().getFICH().getDataType()],
                DSDcc::DSDYSF::ysfCallModeText[(int) m_dsdDemod->getDecoder().getYSFDecoder().getFICH().getCallMode()],
				m_dsdDemod->getDecoder().getYSFDecoder().getFICH().getBlockTotal(),
				m_dsdDemod->getDecoder().getYSFDecoder().getFICH().getFrameTotal(),
				(m_dsdDemod->getDecoder().getYSFDecoder().getFICH().isNarrowMode() ? 'N' : 'W'),
				(m_dsdDemod->getDecoder().getYSFDecoder().getFICH().isInternetPath() ? 'I' : 'L'));

        if (m_dsdDemod->getDecoder().getYSFDecoder().getFICH().isSquelchCodeEnabled())
    	{
            snprintf(&m_formatStatusText[14], 82-14, "%03d", m_dsdDemod->getDecoder().getYSFDecoder().getFICH().getSquelchCode());
    	}
    	else
    	{
            strncpy(&m_formatStatusText[14], "---", 82-14);
    	}

        char dest[13];

        if ( m_dsdDemod->getDecoder().getYSFDecoder().radioIdMode())
        {
            snprintf(dest, 12, "%-5s:%-5s",
                    m_dsdDemod->getDecoder().getYSFDecoder().getDestId(),
                    m_dsdDemod->getDecoder().getYSFDecoder().getSrcId());
        }
        else
        {
            snprintf(dest, 11, "%-10s", m_dsdDemod->getDecoder().getYSFDecoder().getDest());
        }

        snprintf(&m_formatStatusText[17], 82-17, "|%-10s>%s|%-10s>%-10s|%-5s",
                m_dsdDemod->getDecoder().getYSFDecoder().getSrc(),
                dest,
                m_dsdDemod->getDecoder().getYSFDecoder().getUplink(),
                m_dsdDemod->getDecoder().getYSFDecoder().getDownlink(),
                m_dsdDemod->getDecoder().getYSFDecoder().getRem4());

        m_signalFormat = signalFormatYSF;
    	break;
    default:
        m_signalFormat = signalFormatNone;
        m_formatStatusText[0] = '\0';
        break;
    }

    m_formatStatusText[82] = '\0'; // guard
}

void DSDDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void DSDDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void DSDDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_dsdDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1 dB").arg(powDbAvg, 0, 'f', 1));
    }

	bool squelchOpen = m_dsdDemod->getSquelchOpen();

	if (squelchOpen != m_squelchOpen)
	{
		if (squelchOpen) {
			ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
		} else {
			ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
		}

        m_squelchOpen = squelchOpen;
	}

	// "slow" updates

	if (m_tickCount % 10 == 0)
	{
	    ui->inLevelText->setText(QString::number(m_dsdDemod->getDecoder().getInLevel()));
        ui->inCarrierPosText->setText(QString::number(m_dsdDemod->getDecoder().getCarrierPos()));
        ui->zcPosText->setText(QString::number(m_dsdDemod->getDecoder().getZeroCrossingPos()));
        ui->symbolSyncQualityText->setText(QString::number(m_dsdDemod->getDecoder().getSymbolSyncQuality()));

        if (m_dsdDemod->getDecoder().getVoice1On()) {
            ui->slot1On->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->slot1On->setStyleSheet("QToolButton { background-color : rgb(79,79,79); }");
        }

        if (m_dsdDemod->getDecoder().getVoice2On()) {
            ui->slot2On->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->slot2On->setStyleSheet("QToolButton { background-color : rgb(79,79,79); }");
        }

        const char *frameTypeText = m_dsdDemod->getDecoder().getFrameTypeText();

	    if (frameTypeText[0] == '\0') {
	        ui->syncText->setStyleSheet("QLabel { background:rgb(53,53,53); }"); // turn off background
	    } else {
            ui->syncText->setStyleSheet("QLabel { background:rgb(37,53,39); }"); // turn on background
	    }

	    ui->syncText->setText(QString(frameTypeText));

	    formatStatusText();
	    ui->formatStatusText->setText(QString(m_formatStatusText));

	    if (m_formatStatusText[0] == '\0') {
	        ui->formatStatusText->setStyleSheet("QLabel { background:rgb(53,53,53); }"); // turn off background
	    } else {
            ui->formatStatusText->setStyleSheet("QLabel { background:rgb(37,53,39); }"); // turn on background
	    }

        if (m_squelchOpen && ui->symbolPLLLock->isChecked() && m_dsdDemod->getDecoder().getSymbolPLLLocked()) {
            ui->symbolPLLLock->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->symbolPLLLock->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }
	}

	m_tickCount++;
}
