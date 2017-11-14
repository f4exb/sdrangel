///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
#include <QMessageBox>

#include <cmath>

#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "dsp/dspengine.h"
#include "util/db.h"
#include "mainwindow.h"

#include "ui_atvmodgui.h"
#include "atvmodgui.h"

ATVModGUI* ATVModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    ATVModGUI* gui = new ATVModGUI(pluginAPI, deviceUISet, channelTx);
	return gui;
}

void ATVModGUI::destroy()
{
    delete this;
}

void ATVModGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString ATVModGUI::getName() const
{
	return objectName();
}

qint64 ATVModGUI::getCenterFrequency() const {
	return m_channelMarker.getCenterFrequency();
}

void ATVModGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void ATVModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
}

QByteArray ATVModGUI::serialize() const
{
    return m_settings.serialize();
}

bool ATVModGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(true); // will have true
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        displaySettings();
        applySettings(true); // will have true
        return false;
    }
}

bool ATVModGUI::handleMessage(const Message& message)
{
    if (ATVMod::MsgReportVideoFileSourceStreamData::match(message))
    {
        m_videoFrameRate = ((ATVMod::MsgReportVideoFileSourceStreamData&)message).getFrameRate();
        m_videoLength = ((ATVMod::MsgReportVideoFileSourceStreamData&)message).getVideoLength();
        m_frameCount = 0;
        updateWithStreamData();
        return true;
    }
    else if (ATVMod::MsgReportVideoFileSourceStreamTiming::match(message))
    {
        m_frameCount = ((ATVMod::MsgReportVideoFileSourceStreamTiming&)message).getFrameCount();
        updateWithStreamTime();
        return true;
    }
    else if (ATVMod::MsgReportCameraData::match(message))
    {
        ATVMod::MsgReportCameraData& rpt = (ATVMod::MsgReportCameraData&) message;
        ui->cameraDeviceNumber->setText(tr("#%1").arg(rpt.getdeviceNumber()));
        ui->camerFPS->setText(tr("%1 FPS").arg(rpt.getFPS(), 0, 'f', 2));
        ui->cameraImageSize->setText(tr("%1x%2").arg(rpt.getWidth()).arg(rpt.getHeight()));
        ui->cameraManualFPSText->setText(tr("%1 FPS").arg(rpt.getFPSManual(), 0, 'f', 1));
        ui->cameraManualFPSEnable->setChecked(rpt.getFPSManualEnable());
        ui->cameraManualFPS->setValue((int) rpt.getFPSManual()*10.0f);

        int status = rpt.getStatus();

        if (status == 1) // camera FPS scan is startng
        {
            m_camBusyFPSMessageBox = new QMessageBox();
            m_camBusyFPSMessageBox->setText("Computing camera FPS. Please wait…");
            m_camBusyFPSMessageBox->setStandardButtons(0);
            m_camBusyFPSMessageBox->show();
        }
        else if (status == 2) // camera FPS scan is finished
        {
            m_camBusyFPSMessageBox->close();
            if (m_camBusyFPSMessageBox) delete m_camBusyFPSMessageBox;
            m_camBusyFPSMessageBox = 0;
        }

    	return true;
    }
    else if (ATVMod::MsgReportEffectiveSampleRate::match(message))
    {
        int sampleRate = ((ATVMod::MsgReportEffectiveSampleRate&)message).getSampleRate();
        uint32_t nbPointsPerLine = ((ATVMod::MsgReportEffectiveSampleRate&)message).gatNbPointsPerLine();
        ui->channelSampleRateText->setText(tr("%1k").arg(sampleRate/1000.0f, 0, 'f', 2));
        ui->nbPointsPerLineText->setText(tr("%1p").arg(nbPointsPerLine));
        setRFFiltersSlidersRange(sampleRate);
        return true;
    }
    else
    {
        return false;
    }
}

void ATVModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void ATVModGUI::setRFFiltersSlidersRange(int sampleRate)
{
    int scaleFactor = (int) std::log10(sampleRate/2);
    m_rfSliderDivisor = std::pow(10.0, scaleFactor-1);

    if (sampleRate/m_rfSliderDivisor < 50) {
        m_rfSliderDivisor /= 10;
    }

    if ((ui->modulation->currentIndex() == (int) ATVModSettings::ATVModulationLSB) ||
        (ui->modulation->currentIndex() == (int) ATVModSettings::ATVModulationUSB) ||
        (ui->modulation->currentIndex() == (int) ATVModSettings::ATVModulationVestigialLSB) ||
        (ui->modulation->currentIndex() == (int) ATVModSettings::ATVModulationVestigialUSB))
    {
        ui->rfBW->setMaximum((sampleRate) / (2*m_rfSliderDivisor));
        ui->rfOppBW->setMaximum((sampleRate) / (2*m_rfSliderDivisor));
    }
    else
    {
        ui->rfBW->setMaximum((sampleRate) / m_rfSliderDivisor);
        ui->rfOppBW->setMaximum((sampleRate) / m_rfSliderDivisor);
    }

    ui->rfBWText->setText(QString("%1k").arg((ui->rfBW->value()*m_rfSliderDivisor) / 1000.0, 0, 'f', 0));
    ui->rfOppBWText->setText(QString("%1k").arg((ui->rfOppBW->value()*m_rfSliderDivisor) / 1000.0, 0, 'f', 0));
}

int ATVModGUI::getNbLines()
{
    switch(ui->nbLines->currentIndex())
    {
    case 0:
        return 640;
        break;
    case 2:
        return 525;
        break;
    case 3:
        return 480;
        break;
    case 4:
        return 405;
        break;
    case 5:
        return 360;
        break;
    case 6:
        return 343;
        break;
    case 7:
        return 240;
        break;
    case 8:
        return 180;
        break;
    case 9:
        return 120;
        break;
    case 10:
        return 90;
        break;
    case 11:
        return 60;
        break;
    case 12:
        return 32;
        break;
    case 1:
    default:
        return 625;
        break;
    }
}

int ATVModGUI::getNbLinesIndex(int nbLines)
{
    if (nbLines < 32) {
        return 1;
    } else if (nbLines < 60) {
        return 12;
    } else if (nbLines < 90) {
        return 11;
    } else if (nbLines < 120) {
        return 10;
    } else if (nbLines < 180) {
        return 9;
    } else if (nbLines < 240) {
        return 8;
    } else if (nbLines < 343) {
        return 7;
    } else if (nbLines < 360) {
        return 6;
    } else if (nbLines < 405) {
        return 5;
    } else if (nbLines < 480) {
        return 4;
    } else if (nbLines < 525) {
        return 3;
    } else if (nbLines < 625) {
        return 2;
    } else if (nbLines < 640) {
        return 1;
    } else {
        return 0;
    }
}

int ATVModGUI::getFPS()
{
    switch(ui->fps->currentIndex())
    {
    case 0:
        return 30;
        break;
    case 2:
        return 20;
        break;
    case 3:
        return 16;
        break;
    case 4:
        return 12;
        break;
    case 5:
        return 10;
        break;
    case 6:
        return 8;
        break;
    case 7:
        return 5;
        break;
    case 8:
        return 2;
        break;
    case 9:
        return 1;
        break;
    case 1:
    default:
        return 25;
        break;
    }
}

int ATVModGUI::getFPSIndex(int fps)
{
    if (fps < 1) {
        return 1;
    } else if (fps < 2) {
        return 9;
    } else if (fps < 5) {
        return 8;
    } else if (fps < 8) {
        return 7;
    } else if (fps < 10) {
        return 6;
    } else if (fps < 12) {
        return 5;
    } else if (fps < 16) {
        return 4;
    } else if (fps < 20) {
        return 3;
    } else if (fps < 25) {
        return 2;
    } else if (fps < 30) {
        return 1;
    } else {
        return 0;
    }
}

void ATVModGUI::handleSourceMessages()
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

void ATVModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    applySettings();
}

void ATVModGUI::on_modulation_currentIndexChanged(int index)
{
    m_settings.m_atvModulation = (ATVModSettings::ATVModulation) index;
    setRFFiltersSlidersRange(m_atvMod->getEffectiveSampleRate());
    setChannelMarkerBandwidth();
    applySettings();
}

void ATVModGUI::on_rfScaling_valueChanged(int value)
{
    ui->rfScalingText->setText(tr("%1").arg(value));
    m_settings.m_rfScalingFactor = value * 327.68f;
    applySettings();
}

void ATVModGUI::on_fmExcursion_valueChanged(int value)
{
    ui->fmExcursionText->setText(tr("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_fmExcursion = value / 1000.0; // pro mill
    applySettings();
}

void ATVModGUI::on_rfBW_valueChanged(int value)
{
	ui->rfBWText->setText(QString("%1k").arg((value*m_rfSliderDivisor) / 1000.0, 0, 'f', 0));
	m_settings.m_rfBandwidth = value * m_rfSliderDivisor * 1.0f;
	setChannelMarkerBandwidth();
	applySettings();
}

void ATVModGUI::on_rfOppBW_valueChanged(int value)
{
    ui->rfOppBWText->setText(QString("%1k").arg((value*m_rfSliderDivisor) / 1000.0, 0, 'f', 0));
    m_settings.m_rfOppBandwidth = value * m_rfSliderDivisor * 1.0f;
    setChannelMarkerBandwidth();
    applySettings();
}

void ATVModGUI::setChannelMarkerBandwidth()
{
    if (ui->modulation->currentIndex() == (int) ATVModSettings::ATVModulationLSB)
    {
        m_channelMarker.setBandwidth(-ui->rfBW->value()*m_rfSliderDivisor*2);
        m_channelMarker.setSidebands(ChannelMarker::lsb);
    }
    else if (ui->modulation->currentIndex() == (int) ATVModSettings::ATVModulationVestigialLSB)
    {
        m_channelMarker.setBandwidth(ui->rfBW->value()*m_rfSliderDivisor);
        m_channelMarker.setOppositeBandwidth(ui->rfOppBW->value()*m_rfSliderDivisor);
        m_channelMarker.setSidebands(ChannelMarker::vlsb);
    }
    else if (ui->modulation->currentIndex() == (int) ATVModSettings::ATVModulationUSB)
    {
        m_channelMarker.setBandwidth(ui->rfBW->value()*m_rfSliderDivisor*2);
        m_channelMarker.setSidebands(ChannelMarker::usb);
    }
    else if (ui->modulation->currentIndex() == (int) ATVModSettings::ATVModulationVestigialUSB)
    {
        m_channelMarker.setBandwidth(ui->rfBW->value()*m_rfSliderDivisor);
        m_channelMarker.setOppositeBandwidth(ui->rfOppBW->value()*m_rfSliderDivisor);
        m_channelMarker.setSidebands(ChannelMarker::vusb);
    }
    else
    {
        m_channelMarker.setBandwidth(ui->rfBW->value()*m_rfSliderDivisor);
        m_channelMarker.setSidebands(ChannelMarker::dsb);
    }
}

void ATVModGUI::on_nbLines_currentIndexChanged(int index __attribute__((unused)))
{
    m_settings.m_nbLines = getNbLines();
    applySettings();
}

void ATVModGUI::on_fps_currentIndexChanged(int index __attribute__((unused)))
{
    m_settings.m_fps = getFPS();
    applySettings();
}


void ATVModGUI::on_standard_currentIndexChanged(int index)
{
    m_settings.m_atvStd = (ATVModSettings::ATVStd) index;
    applySettings();
}

void ATVModGUI::on_uniformLevel_valueChanged(int value)
{
	ui->uniformLevelText->setText(QString("%1").arg(value));
	m_settings.m_uniformLevel = value / 100.0f;
	applySettings();
}

void ATVModGUI::on_invertVideo_clicked(bool checked)
{
    m_settings.m_invertedVideo = checked;
    applySettings();
}

void ATVModGUI::on_inputSelect_currentIndexChanged(int index)
{
    m_settings.m_atvModInput = (ATVModSettings::ATVModInput) index;
    applySettings();
}

void ATVModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
	applySettings();
}

void ATVModGUI::on_forceDecimator_toggled(bool checked)
{
    m_settings.m_forceDecimator = checked;
    applySettings();
}

void ATVModGUI::on_imageFileDialog_clicked(bool checked __attribute__((unused)))
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open image file"), ".", tr("Image Files (*.png *.jpg *.bmp *.gif *.tiff)"));

    if (fileName != "")
    {
        m_imageFileName = fileName;
        ui->imageFileText->setText(m_imageFileName);
        configureImageFileName();
    }
}

void ATVModGUI::on_videoFileDialog_clicked(bool checked __attribute__((unused)))
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open video file"), ".", tr("Video Files (*.avi *.mpg *.mp4 *.mov *.m4v *.mkv *.vob *.wmv)"));

    if (fileName != "")
    {
        m_videoFileName = fileName;
        ui->videoFileText->setText(m_videoFileName);
        configureVideoFileName();
    }
}

void ATVModGUI::on_playLoop_toggled(bool checked)
{
    m_settings.m_videoPlayLoop = checked;
    applySettings();
}

void ATVModGUI::on_playVideo_toggled(bool checked)
{
    m_settings.m_videoPlay = checked;
    ui->navTimeSlider->setEnabled(!checked);
    m_enableNavTime = !checked;
    applySettings();
}

void ATVModGUI::on_navTimeSlider_valueChanged(int value)
{
    if (m_enableNavTime && ((value >= 0) && (value <= 100)))
    {
        ATVMod::MsgConfigureVideoFileSourceSeek* message = ATVMod::MsgConfigureVideoFileSourceSeek::create(value);
        m_atvMod->getInputMessageQueue()->push(message);
    }
}

void ATVModGUI::on_playCamera_toggled(bool checked)
{
    m_settings.m_cameraPlay = checked;
    applySettings();
}

void ATVModGUI::on_camSelect_currentIndexChanged(int index)
{
    ATVMod::MsgConfigureCameraIndex* message = ATVMod::MsgConfigureCameraIndex::create(index);
    m_atvMod->getInputMessageQueue()->push(message);
}

void ATVModGUI::on_cameraManualFPSEnable_toggled(bool checked)
{
	ATVMod::MsgConfigureCameraData* message = ATVMod::MsgConfigureCameraData::create(
			ui->camSelect->currentIndex(),
			checked,
			ui->cameraManualFPS->value() / 10.0f);
    m_atvMod->getInputMessageQueue()->push(message);
}

void ATVModGUI::on_cameraManualFPS_valueChanged(int value)
{
    ui->cameraManualFPSText->setText(tr("%1 FPS").arg(value / 10.0f, 0, 'f', 1));
	ATVMod::MsgConfigureCameraData* message = ATVMod::MsgConfigureCameraData::create(
			ui->camSelect->currentIndex(),
			ui->cameraManualFPSEnable->isChecked(),
			value / 10.0f);
    m_atvMod->getInputMessageQueue()->push(message);
}

void ATVModGUI::on_overlayTextShow_toggled(bool checked)
{
    ATVMod::MsgConfigureShowOverlayText* message = ATVMod::MsgConfigureShowOverlayText::create(checked);
    m_atvMod->getInputMessageQueue()->push(message);
}

void ATVModGUI::on_overlayText_textEdited(const QString& arg1 __attribute__((unused)))
{
    m_settings.m_overlayText = arg1;
    ATVMod::MsgConfigureOverlayText* message = ATVMod::MsgConfigureOverlayText::create(ui->overlayText->text());
    m_atvMod->getInputMessageQueue()->push(message);
}

void ATVModGUI::configureImageFileName()
{
    qDebug() << "ATVModGUI::configureImageFileName: " << m_imageFileName.toStdString().c_str();
    ATVMod::MsgConfigureImageFileName* message = ATVMod::MsgConfigureImageFileName::create(m_imageFileName);
    m_atvMod->getInputMessageQueue()->push(message);
}

void ATVModGUI::configureVideoFileName()
{
    qDebug() << "ATVModGUI::configureVideoFileName: " << m_videoFileName.toStdString().c_str();
    ATVMod::MsgConfigureVideoFileName* message = ATVMod::MsgConfigureVideoFileName::create(m_videoFileName);
    m_atvMod->getInputMessageQueue()->push(message);
}

void ATVModGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
}

ATVModGUI::ATVModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::ATVModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
	m_doApplySettings(true),
	m_channelPowerDbAvg(20,0),
    m_videoLength(0),
    m_videoFrameRate(48000),
    m_frameCount(0),
    m_tickCount(0),
    m_enableNavTime(false),
    m_camBusyFPSMessageBox(0),
    m_rfSliderDivisor(100000)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

	m_atvMod = (ATVMod*) channelTx; //new ATVMod(m_deviceUISet->m_deviceSinkAPI);
	m_atvMod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
	m_channelMarker.setColor(m_settings.m_rgbColor);
	m_channelMarker.setBandwidth(5000);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setTitle("ATV Modulator");
    m_channelMarker.setUDPAddress("127.0.0.1");
    m_channelMarker.setUDPSendPort(9999);
	m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);

	m_deviceUISet->registerTxChannelInstance(ATVMod::m_channelID, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    resetToDefaults();

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    connect(m_atvMod, SIGNAL(levelChanged(qreal, qreal, int)), ui->volumeMeter, SLOT(levelChanged(qreal, qreal, int)));

    std::vector<int> cameraNumbers;
    m_atvMod->getCameraNumbers(cameraNumbers);

    for (std::vector<int>::iterator it = cameraNumbers.begin(); it != cameraNumbers.end(); ++it) {
        ui->camSelect->addItem(tr("%1").arg(*it));
    }

    QChar delta = QChar(0x94, 0x03);
    ui->fmExcursionLabel->setText(delta);

    displaySettings();
    applySettings(true);
}

ATVModGUI::~ATVModGUI()
{
    m_deviceUISet->removeTxChannelInstance(this);
	delete m_atvMod; // TODO: check this: when the GUI closes it has to delete the modulator
	delete ui;
}

void ATVModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void ATVModGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		ATVMod::MsgConfigureChannelizer *msgChan = ATVMod::MsgConfigureChannelizer::create(
		        m_channelMarker.getCenterFrequency());
        m_atvMod->getInputMessageQueue()->push(msgChan);

		ATVMod::MsgConfigureATVMod *msg = ATVMod::MsgConfigureATVMod::create(m_settings, force);
		m_atvMod->getInputMessageQueue()->push(msg);
	}
}

void ATVModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    setChannelMarkerBandwidth();
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);

    ui->modulation->setCurrentIndex((int) m_settings.m_atvModulation);
    setRFFiltersSlidersRange(m_atvMod->getEffectiveSampleRate());

    ui->rfBW->setValue(roundf(m_settings.m_rfBandwidth / m_rfSliderDivisor));
    ui->rfBWText->setText(QString("%1k").arg((ui->rfBW->value()*m_rfSliderDivisor) / 1000.0, 0, 'f', 0));

    ui->rfOppBW->setValue(roundf(m_settings.m_rfOppBandwidth / m_rfSliderDivisor));
    ui->rfOppBWText->setText(QString("%1k").arg((ui->rfOppBW->value()*m_rfSliderDivisor) / 1000.0, 0, 'f', 0));


    ui->forceDecimator->setChecked(m_settings.m_forceDecimator);
    ui->channelMute->setChecked(m_settings.m_channelMute);

    ui->fmExcursion->setValue(roundf(m_settings.m_fmExcursion * 1000.0));
    ui->fmExcursionText->setText(tr("%1").arg(ui->fmExcursion->value() / 10.0, 0, 'f', 1));

    ui->rfScaling->setValue(roundf(m_settings.m_rfScalingFactor / 327.68f));
    ui->rfScalingText->setText(tr("%1").arg(ui->rfScaling->value()));

    int validNbLinesIndex = getNbLinesIndex(m_settings.m_nbLines);
    ui->nbLines->setCurrentIndex(validNbLinesIndex);
    m_settings.m_nbLines = getNbLines(); // normalize
    int validFPSIndex = getFPSIndex(m_settings.m_fps);
    ui->fps->setCurrentIndex(validFPSIndex);
    m_settings.m_fps = getFPS(); // normalize

    ui->standard->setCurrentIndex((int) m_settings.m_atvStd);
    ui->inputSelect->setCurrentIndex((int) m_settings.m_atvModInput);

    ui->invertVideo->setChecked(m_settings.m_invertedVideo);

    ui->uniformLevel->setValue(roundf(m_settings.m_uniformLevel * 100.0));
    ui->uniformLevelText->setText(QString("%1").arg(ui->uniformLevel->value()));

    ui->overlayText->setText(m_settings.m_overlayText);

    ATVMod::MsgConfigureOverlayText* message = ATVMod::MsgConfigureOverlayText::create(ui->overlayText->text());
    m_atvMod->getInputMessageQueue()->push(message);

    blockApplySettings(false);
}

void ATVModGUI::leaveEvent(QEvent*)
{
	m_channelMarker.setHighlighted(false);
}

void ATVModGUI::enterEvent(QEvent*)
{
	m_channelMarker.setHighlighted(true);
}

void ATVModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_atvMod->getMagSq());
	m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.average(), 0, 'f', 1));

    if (((++m_tickCount & 0xf) == 0) && (ui->inputSelect->currentIndex() == (int) ATVModSettings::ATVModInputVideo))
    {
        ATVMod::MsgConfigureVideoFileSourceStreamTiming* message = ATVMod::MsgConfigureVideoFileSourceStreamTiming::create();
        m_atvMod->getInputMessageQueue()->push(message);
    }
}

void ATVModGUI::updateWithStreamData()
{
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_videoLength / m_videoFrameRate);
    QString s_time = recordLength.toString("hh:mm:ss");
    ui->recordLengthText->setText(s_time);
    updateWithStreamTime();
}

void ATVModGUI::updateWithStreamTime()
{
    int t_sec = 0;
    int t_msec = 0;

    if (m_videoFrameRate > 0.0f)
    {
        float secs = m_frameCount / m_videoFrameRate;
        t_sec = (int) secs;
        t_msec = (int) ((secs - t_sec) * 1000.0f);
    }

    QTime t(0, 0, 0, 0);
    t = t.addSecs(t_sec);
    t = t.addMSecs(t_msec);
    QString s_timems = t.toString("hh:mm:ss.zzz");
    QString s_time = t.toString("hh:mm:ss");
    ui->relTimeText->setText(s_timems);

    if (!m_enableNavTime)
    {
        float posRatio = (t_sec *  m_videoFrameRate) / m_videoLength;
        ui->navTimeSlider->setValue((int) (posRatio * 100.0));
    }
}

