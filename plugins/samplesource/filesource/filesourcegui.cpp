///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>

#include <QTime>
#include <QDateTime>
#include <QString>
#include <QFileDialog>
#include <QMessageBox>

#include "ui_filesourcegui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/crightclickenabler.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

#include "mainwindow.h"

#include "filesourcegui.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"

FileSourceGui::FileSourceGui(DeviceUISet *deviceUISet, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::FileSourceGui),
	m_deviceUISet(deviceUISet),
	m_settings(),
	m_doApplySettings(true),
	m_sampleSource(0),
	m_acquisition(false),
	m_fileName("..."),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_recordLength(0),
	m_startingTimeStamp(0),
	m_samplesCount(0),
	m_tickCount(0),
	m_enableNavTime(false),
	m_lastEngineState(DeviceAPI::StNotStarted)
{
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->centerFrequency->setValueRange(7, 0, pow(10,7));
	ui->fileNameText->setText(m_fileName);
	ui->crcLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");

	connect(&(m_deviceUISet->m_deviceAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

    CRightClickEnabler *startStopRightClickEnabler = new CRightClickEnabler(ui->startStop);
    connect(startStopRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

	setAccelerationCombo();
	displaySettings();

	ui->navTimeSlider->setEnabled(false);
	ui->acceleration->setEnabled(false);

    m_sampleSource = m_deviceUISet->m_deviceAPI->getSampleSource();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);
}

FileSourceGui::~FileSourceGui()
{
	delete ui;
}

void FileSourceGui::destroy()
{
	delete this;
}

void FileSourceGui::setName(const QString& name)
{
	setObjectName(name);
}

QString FileSourceGui::getName() const
{
	return objectName();
}

void FileSourceGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 FileSourceGui::getCenterFrequency() const
{
	return m_centerFrequency;
}

void FileSourceGui::setCenterFrequency(qint64 centerFrequency)
{
	m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

QByteArray FileSourceGui::serialize() const
{
	return m_settings.serialize();
}

bool FileSourceGui::deserialize(const QByteArray& data)
{
	if(m_settings.deserialize(data)) {
		displaySettings();
		sendSettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

void FileSourceGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_deviceSampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("FileSourceGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();

            delete message;
        }
        else
        {
            if (handleMessage(*message))
            {
                delete message;
            }
        }
    }
}

bool FileSourceGui::handleMessage(const Message& message)
{
    if (FileSourceInput::MsgConfigureFileSource::match(message))
    {
        const FileSourceInput::MsgConfigureFileSource& cfg = (FileSourceInput::MsgConfigureFileSource&) message;
        m_settings = cfg.getSettings();
        displaySettings();
        return true;
    }
    else if (FileSourceInput::MsgReportFileSourceAcquisition::match(message))
	{
		m_acquisition = ((FileSourceInput::MsgReportFileSourceAcquisition&)message).getAcquisition();
		updateWithAcquisition();
		return true;
	}
	else if (FileSourceInput::MsgReportFileSourceStreamData::match(message))
	{
		m_sampleRate = ((FileSourceInput::MsgReportFileSourceStreamData&)message).getSampleRate();
		m_sampleSize = ((FileSourceInput::MsgReportFileSourceStreamData&)message).getSampleSize();
		m_centerFrequency = ((FileSourceInput::MsgReportFileSourceStreamData&)message).getCenterFrequency();
		m_startingTimeStamp = ((FileSourceInput::MsgReportFileSourceStreamData&)message).getStartingTimeStamp();
		m_recordLength = ((FileSourceInput::MsgReportFileSourceStreamData&)message).getRecordLength();
		updateWithStreamData();
		return true;
	}
	else if (FileSourceInput::MsgReportFileSourceStreamTiming::match(message))
	{
		m_samplesCount = ((FileSourceInput::MsgReportFileSourceStreamTiming&)message).getSamplesCount();
		updateWithStreamTime();
		return true;
	}
	else if (FileSourceInput::MsgStartStop::match(message))
    {
	    FileSourceInput::MsgStartStop& notif = (FileSourceInput::MsgStartStop&) message;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);

        return true;
    }
	else if (FileSourceInput::MsgPlayPause::match(message))
	{
	    FileSourceInput::MsgPlayPause& notif = (FileSourceInput::MsgPlayPause&) message;
	    bool checked = notif.getPlayPause();
	    ui->play->setChecked(checked);
	    ui->navTimeSlider->setEnabled(!checked);
	    ui->acceleration->setEnabled(!checked);
	    m_enableNavTime = !checked;

	    return true;
	}
	else if (FileSourceInput::MsgReportHeaderCRC::match(message))
	{
		FileSourceInput::MsgReportHeaderCRC& notif = (FileSourceInput::MsgReportHeaderCRC&) message;
		if (notif.isOK()) {
			ui->crcLabel->setStyleSheet("QLabel { background-color : green; }");
		} else {
			ui->crcLabel->setStyleSheet("QLabel { background-color : red; }");
		}

		return true;
	}
	else
	{
		return false;
	}
}

void FileSourceGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_deviceSampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_deviceSampleRate / 1000));
}

void FileSourceGui::displaySettings()
{
    blockApplySettings(true);
    ui->playLoop->setChecked(m_settings.m_loop);
    ui->acceleration->setCurrentIndex(FileSourceSettings::getAccelerationIndex(m_settings.m_accelerationFactor));
    blockApplySettings(false);
}

void FileSourceGui::sendSettings()
{
}

void FileSourceGui::on_playLoop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        m_settings.m_loop = checked;
        FileSourceInput::MsgConfigureFileSource *message = FileSourceInput::MsgConfigureFileSource::create(m_settings, false);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void FileSourceGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        FileSourceInput::MsgStartStop *message = FileSourceInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void FileSourceGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DeviceAPI::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DeviceAPI::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DeviceAPI::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DeviceAPI::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

void FileSourceGui::on_play_toggled(bool checked)
{
	FileSourceInput::MsgConfigureFileSourceWork* message = FileSourceInput::MsgConfigureFileSourceWork::create(checked);
	m_sampleSource->getInputMessageQueue()->push(message);
	ui->navTimeSlider->setEnabled(!checked);
	ui->acceleration->setEnabled(!checked);
	m_enableNavTime = !checked;
}

void FileSourceGui::on_navTimeSlider_valueChanged(int value)
{
	if (m_enableNavTime && ((value >= 0) && (value <= 1000)))
	{
		FileSourceInput::MsgConfigureFileSourceSeek* message = FileSourceInput::MsgConfigureFileSourceSeek::create(value);
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}

void FileSourceGui::on_showFileDialog_clicked(bool checked)
{
    (void) checked;
	QString fileName = QFileDialog::getOpenFileName(this,
	    tr("Open I/Q record file"), ".", tr("SDR I/Q Files (*.sdriq)"), 0, QFileDialog::DontUseNativeDialog);

	if (fileName != "")
	{
		m_fileName = fileName;
		ui->fileNameText->setText(m_fileName);
		ui->crcLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
		configureFileName();
	}
}

void FileSourceGui::on_acceleration_currentIndexChanged(int index)
{
    if (m_doApplySettings)
    {
        m_settings.m_accelerationFactor = FileSourceSettings::getAccelerationValue(index);
        FileSourceInput::MsgConfigureFileSource *message = FileSourceInput::MsgConfigureFileSource::create(m_settings, false);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void FileSourceGui::configureFileName()
{
	qDebug() << "FileSourceGui::configureFileName: " << m_fileName.toStdString().c_str();
	FileSourceInput::MsgConfigureFileSourceName* message = FileSourceInput::MsgConfigureFileSourceName::create(m_fileName);
	m_sampleSource->getInputMessageQueue()->push(message);
}

void FileSourceGui::updateWithAcquisition()
{
	ui->play->setEnabled(m_acquisition);
	ui->play->setChecked(m_acquisition);
	ui->showFileDialog->setEnabled(!m_acquisition);
}

void FileSourceGui::updateWithStreamData()
{
	ui->centerFrequency->setValue(m_centerFrequency/1000);
	ui->sampleRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
	ui->sampleSizeText->setText(tr("%1b").arg(m_sampleSize));
	ui->play->setEnabled(m_acquisition);
	QTime recordLength(0, 0, 0, 0);
	recordLength = recordLength.addSecs(m_recordLength);
	QString s_time = recordLength.toString("HH:mm:ss");
	ui->recordLengthText->setText(s_time);
	updateWithStreamTime();
}

void FileSourceGui::updateWithStreamTime()
{
    qint64 t_sec = 0;
    qint64 t_msec = 0;

	if (m_sampleRate > 0){
		t_sec = m_samplesCount / m_sampleRate;
        t_msec = (m_samplesCount - (t_sec * m_sampleRate)) * 1000LL / m_sampleRate;
	}

	QTime t(0, 0, 0, 0);
	t = t.addSecs(t_sec);
	t = t.addMSecs(t_msec);
	QString s_timems = t.toString("HH:mm:ss.zzz");
	ui->relTimeText->setText(s_timems);

    qint64 startingTimeStampMsec = m_startingTimeStamp * 1000LL;
	QDateTime dt = QDateTime::fromMSecsSinceEpoch(startingTimeStampMsec);
    dt = dt.addSecs(t_sec);
    dt = dt.addMSecs(t_msec);
	QString s_date = dt.toString("yyyy-MM-dd HH:mm:ss.zzz");
	ui->absTimeText->setText(s_date);

	if (!m_enableNavTime)
	{
		float posRatio = (float) t_sec / (float) m_recordLength;
		ui->navTimeSlider->setValue((int) (posRatio * 1000.0));
	}
}

void FileSourceGui::tick()
{
	if ((++m_tickCount & 0xf) == 0) {
		FileSourceInput::MsgConfigureFileSourceStreamTiming* message = FileSourceInput::MsgConfigureFileSourceStreamTiming::create();
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}

void FileSourceGui::setAccelerationCombo()
{
    ui->acceleration->blockSignals(true);
    ui->acceleration->clear();
    ui->acceleration->addItem(QString("1"));

    for (unsigned int i = 0; i <= FileSourceSettings::m_accelerationMaxScale; i++)
    {
        QString s;
        int m = pow(10.0, i);
        int x = 2*m;
        setNumberStr(x, s);
        ui->acceleration->addItem(s);
        x = 5*m;
        setNumberStr(x, s);
        ui->acceleration->addItem(s);
        x = 10*m;
        setNumberStr(x, s);
        ui->acceleration->addItem(s);
    }

    ui->acceleration->blockSignals(false);
}

void FileSourceGui::setNumberStr(int n, QString& s)
{
    if (n < 1000) {
        s = tr("%1").arg(n);
    } else if (n < 100000) {
        s = tr("%1k").arg(n/1000);
    } else if (n < 1000000) {
        s = tr("%1e5").arg(n/100000);
    } else if (n < 1000000000) {
        s = tr("%1M").arg(n/1000000);
    } else {
        s = tr("%1G").arg(n/1000000000);
    }
}

void FileSourceGui::openDeviceSettingsDialog(const QPoint& p)
{
    BasicDeviceSettingsDialog dialog(this);
    dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
    dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
    dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
    dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);

    dialog.move(p);
    dialog.exec();

    m_settings.m_useReverseAPI = dialog.useReverseAPI();
    m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
    m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
    m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();

    sendSettings();
}
