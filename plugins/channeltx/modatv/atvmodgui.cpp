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

#include "device/devicesinkapi.h"
#include "dsp/upchannelizer.h"

#include "dsp/threadedbasebandsamplesource.h"
#include "ui_atvmodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"
#include "atvmodgui.h"

const QString ATVModGUI::m_channelID = "sdrangel.channeltx.modatv";

ATVModGUI* ATVModGUI::create(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI)
{
    ATVModGUI* gui = new ATVModGUI(pluginAPI, deviceAPI);
	return gui;
}

void ATVModGUI::destroy()
{
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
	blockApplySettings(true);

	ui->rfBW->setValue(10);
	ui->uniformLevel->setValue(35);
	ui->standard->setCurrentIndex(0);
	ui->inputSelect->setCurrentIndex(0);
	ui->deltaFrequency->setValue(0);
	ui->modulation->setCurrentIndex(0);

	blockApplySettings(false);
	applySettings();
}

QByteArray ATVModGUI::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(1, m_channelMarker.getCenterFrequency());
	s.writeS32(2, ui->rfBW->value());
	s.writeS32(3, ui->uniformLevel->value());
	s.writeS32(4, ui->standard->currentIndex());
    s.writeS32(5, ui->inputSelect->currentIndex());
	s.writeU32(6, m_channelMarker.getColor().rgb());
    s.writeS32(8, ui->modulation->currentIndex());

	return s.final();
}

bool ATVModGUI::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid())
    {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1)
    {
		QByteArray bytetmp;
		quint32 u32tmp;
		qint32 tmp;

		blockApplySettings(true);
		m_channelMarker.blockSignals(true);

		d.readS32(1, &tmp, 0);
		m_channelMarker.setCenterFrequency(tmp);
		d.readS32(2, &tmp, 4);
		ui->rfBW->setValue(tmp);
		d.readS32(3, &tmp, 100);
		ui->uniformLevel->setValue(tmp);
		d.readS32(4, &tmp, 0);
		ui->standard->setCurrentIndex(tmp);
        d.readS32(5, &tmp, 0);
        ui->inputSelect->setCurrentIndex(tmp);

        if(d.readU32(6, &u32tmp))
        {
			m_channelMarker.setColor(u32tmp);
        }

        d.readS32(8, &tmp, 0);
        ui->modulation->setCurrentIndex(tmp);

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
    else
    {
        return false;
    }
}

void ATVModGUI::viewChanged()
{
	applySettings();
}

void ATVModGUI::channelizerOutputSampleRateChanged()
{
    if ((ui->modulation->currentIndex() == (int) ATVMod::ATVModulationLSB) ||
        (ui->modulation->currentIndex() == (int) ATVMod::ATVModulationUSB))
    {
        ui->rfBW->setMaximum(m_channelizer->getOutputSampleRate() / 200000);
    }
    else
    {
        ui->rfBW->setMaximum(m_channelizer->getOutputSampleRate() / 100000);
    }
}

void ATVModGUI::handleSourceMessages()
{
    Message* message;

    while ((message = m_atvMod->getOutputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void ATVModGUI::on_deltaMinus_toggled(bool minus)
{
	int deltaFrequency = m_channelMarker.getCenterFrequency();
	bool minusDelta = (deltaFrequency < 0);

	if (minus ^ minusDelta) // sign change
	{
		m_channelMarker.setCenterFrequency(-deltaFrequency);
	}
}

void ATVModGUI::on_deltaFrequency_changed(quint64 value)
{
	if (ui->deltaMinus->isChecked()) {
		m_channelMarker.setCenterFrequency(-value);
	} else {
		m_channelMarker.setCenterFrequency(value);
	}
}

void ATVModGUI::on_modulation_currentIndexChanged(int index)
{
    if (index == (int) ATVMod::ATVModulationLSB)
    {
        ui->rfBW->setMaximum(m_channelizer->getOutputSampleRate() / 200000);
        m_channelMarker.setBandwidth(-ui->rfBW->value()*200000);
        m_channelMarker.setSidebands(ChannelMarker::lsb);
    }
    else if (index == (int) ATVMod::ATVModulationUSB)
    {
        ui->rfBW->setMaximum(m_channelizer->getOutputSampleRate() / 200000);
        m_channelMarker.setBandwidth(ui->rfBW->value()*200000);
        m_channelMarker.setSidebands(ChannelMarker::usb);
    }
    else
    {
        ui->rfBW->setMaximum(m_channelizer->getOutputSampleRate() / 100000);
        m_channelMarker.setBandwidth(ui->rfBW->value()*100000);
        m_channelMarker.setSidebands(ChannelMarker::dsb);
    }

    applySettings();
}

void ATVModGUI::on_rfBW_valueChanged(int value)
{
	ui->rfBWText->setText(QString("%1 MHz").arg(value / 10.0, 0, 'f', 1));

    if (ui->modulation->currentIndex() == (int) ATVMod::ATVModulationLSB)
    {
        m_channelMarker.setBandwidth(-ui->rfBW->value()*200000);
    }
    else if (ui->modulation->currentIndex() == (int) ATVMod::ATVModulationUSB)
    {
        m_channelMarker.setBandwidth(ui->rfBW->value()*200000);
    }
    else
    {
        m_channelMarker.setBandwidth(ui->rfBW->value()*100000);
    }

	applySettings();
}

void ATVModGUI::on_standard_currentIndexChanged(int index)
{
    applySettings();
}

void ATVModGUI::on_uniformLevel_valueChanged(int value)
{
	ui->uniformLevelText->setText(QString("%1").arg(value));
	applySettings();
}

void ATVModGUI::on_inputSelect_currentIndexChanged(int index)
{
    applySettings();
}

void ATVModGUI::on_channelMute_toggled(bool checked)
{
	applySettings();
}

void ATVModGUI::on_imageFileDialog_clicked(bool checked)
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

void ATVModGUI::on_videoFileDialog_clicked(bool checked)
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open video file"), ".", tr("Video Files (*.avi *.mpg *.mp4 *.mov *.m4v *.vob *.wmv)"));

    if (fileName != "")
    {
        m_videoFileName = fileName;
        ui->videoFileText->setText(m_videoFileName);
        configureVideoFileName();
    }
}

void ATVModGUI::on_playLoop_toggled(bool checked)
{
    applySettings();
}

void ATVModGUI::on_playVideo_toggled(bool checked)
{
    ui->navTimeSlider->setEnabled(!checked);
    m_enableNavTime = !checked;
    applySettings();
}

void ATVModGUI::on_navTimeSlider_valueChanged(int value)
{
    if (m_enableNavTime && ((value >= 0) && (value <= 100)))
    {
        int seekFame = (m_videoLength * value) / 100;
        ATVMod::MsgConfigureVideoFileSourceSeek* message = ATVMod::MsgConfigureVideoFileSourceSeek::create(value);
        m_atvMod->getInputMessageQueue()->push(message);
    }
}

void ATVModGUI::on_playCamera_toggled(bool checked)
{
    applySettings();
}

void ATVModGUI::on_camSelect_currentIndexChanged(int index)
{
    ATVMod::MsgConfigureCameraIndex* message = ATVMod::MsgConfigureCameraIndex::create(index);
    m_atvMod->getInputMessageQueue()->push(message);
}

void ATVModGUI::on_overlayTextShow_toggled(bool checked)
{
    ATVMod::MsgConfigureShowOverlayText* message = ATVMod::MsgConfigureShowOverlayText::create(checked);
    m_atvMod->getInputMessageQueue()->push(message);
}

void ATVModGUI::on_overlayText_textEdited(const QString& arg1)
{
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

void ATVModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
}

void ATVModGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown) {
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(&m_channelMarker, this);
		bcsw->show();
	}
}

ATVModGUI::ATVModGUI(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::ATVModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceAPI(deviceAPI),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_doApplySettings(true),
	m_channelPowerDbAvg(20,0),
    m_videoLength(0),
    m_videoFrameRate(48000),
    m_frameCount(0),
    m_tickCount(0),
    m_enableNavTime(false),
    m_camBusyFPSMessageBox(0)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_atvMod = new ATVMod();
	m_channelizer = new UpChannelizer(m_atvMod);
	m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
	//m_pluginAPI->addThreadedSink(m_threadedChannelizer);
    connect(m_channelizer, SIGNAL(outputSampleRateChanged()), this, SLOT(channelizerOutputSampleRateChanged()));
    m_deviceAPI->addThreadedSource(m_threadedChannelizer);

	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));

	//m_channelMarker = new ChannelMarker(this);
	m_channelMarker.setColor(Qt::white);
	m_channelMarker.setBandwidth(5000);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
    m_deviceAPI->addChannelMarker(&m_channelMarker);
    m_deviceAPI->addRollupWidget(this);

    resetToDefaults();

	connect(m_atvMod->getOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    connect(m_atvMod, SIGNAL(levelChanged(qreal, qreal, int)), ui->volumeMeter, SLOT(levelChanged(qreal, qreal, int)));

    std::vector<int> cameraNumbers;
    m_atvMod->getCameraNumbers(cameraNumbers);

    for (std::vector<int>::iterator it = cameraNumbers.begin(); it != cameraNumbers.end(); ++it) {
        ui->camSelect->addItem(tr("%1").arg(*it));
    }
}

ATVModGUI::~ATVModGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
	delete m_threadedChannelizer;
	delete m_channelizer;
	delete m_atvMod;
	//delete m_channelMarker;
	delete ui;
}

void ATVModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void ATVModGUI::applySettings()
{
	if (m_doApplySettings)
	{
		setTitleColor(m_channelMarker.getColor());

		m_channelizer->configure(m_channelizer->getInputMessageQueue(),
		    m_channelizer->getOutputSampleRate(),
			m_channelMarker.getCenterFrequency());

		ui->deltaFrequency->setValue(abs(m_channelMarker.getCenterFrequency()));
		ui->deltaMinus->setChecked(m_channelMarker.getCenterFrequency() < 0);

		m_atvMod->configure(m_atvMod->getInputMessageQueue(),
			ui->rfBW->value() * 100000.0f,
			(ATVMod::ATVStd) ui->standard->currentIndex(),
			(ATVMod::ATVModInput) ui->inputSelect->currentIndex(),
			ui->uniformLevel->value() / 100.0f,
			(ATVMod::ATVModulation) ui->modulation->currentIndex(),
			ui->playLoop->isChecked(),
			ui->playVideo->isChecked(),
			ui->playCamera->isChecked(),
			ui->channelMute->isChecked());
	}
}

void ATVModGUI::leaveEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(false);
	blockApplySettings(false);
}

void ATVModGUI::enterEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(true);
	blockApplySettings(false);
}

void ATVModGUI::tick()
{
	Real powDb = CalcDb::dbPower(m_atvMod->getMagSq());
	m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(QString::number(m_channelPowerDbAvg.average(), 'f', 1));

    if (((++m_tickCount & 0xf) == 0) && (ui->inputSelect->currentIndex() == (int) ATVMod::ATVModInputVideo))
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

