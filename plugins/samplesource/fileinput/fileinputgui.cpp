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
#include <QResizeEvent>

#include "ui_fileinputgui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

#include "mainwindow.h"

#include "fileinputgui.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"

FileInputGUI::FileInputGUI(DeviceUISet *deviceUISet, QWidget* parent) :
	DeviceGUI(parent),
	ui(new Ui::FileInputGUI),
	m_settings(),
	m_doApplySettings(true),
	m_sampleSource(0),
	m_acquisition(false),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_recordLengthMuSec(0),
	m_startingTimeStamp(0),
	m_samplesCount(0),
	m_tickCount(0),
	m_enableNavTime(false),
	m_lastEngineState(DeviceAPI::StNotStarted)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    ui->setupUi(getContents());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    getContents()->setStyleSheet("#FileInputGUI { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/fileinput/readme.md";
	ui->crcLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");

	connect(&(m_deviceUISet->m_deviceAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

	setAccelerationCombo();
	displaySettings();

	ui->navTimeSlider->setEnabled(false);
	ui->acceleration->setEnabled(false);

    m_sampleSource = m_deviceUISet->m_deviceAPI->getSampleSource();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);

    makeUIConnections();
}

FileInputGUI::~FileInputGUI()
{
    qDebug("FileInputGUI::~FileInputGUI");
    m_statusTimer.stop();
	delete ui;
    qDebug("FileInputGUI::~FileInputGUI: end");
}

void FileInputGUI::destroy()
{
	delete this;
}

void FileInputGUI::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

QByteArray FileInputGUI::serialize() const
{
	return m_settings.serialize();
}

bool FileInputGUI::deserialize(const QByteArray& data)
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

void FileInputGUI::resizeEvent(QResizeEvent* size)
{
    adjustSize();
    size->accept();
}

void FileInputGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_deviceSampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("FileInputGUI::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

bool FileInputGUI::handleMessage(const Message& message)
{
    if (FileInput::MsgConfigureFileInput::match(message))
    {
        const FileInput::MsgConfigureFileInput& cfg = (FileInput::MsgConfigureFileInput&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        displaySettings();
        return true;
    }
    else if (FileInput::MsgReportFileSourceAcquisition::match(message))
	{
		m_acquisition = ((FileInput::MsgReportFileSourceAcquisition&)message).getAcquisition();
		updateWithAcquisition();
		return true;
	}
	else if (FileInput::MsgReportFileInputStreamData::match(message))
	{
		m_sampleRate = ((FileInput::MsgReportFileInputStreamData&)message).getSampleRate();
		m_sampleSize = ((FileInput::MsgReportFileInputStreamData&)message).getSampleSize();
		m_centerFrequency = ((FileInput::MsgReportFileInputStreamData&)message).getCenterFrequency();
		m_startingTimeStamp = ((FileInput::MsgReportFileInputStreamData&)message).getStartingTimeStamp();
		m_recordLengthMuSec = ((FileInput::MsgReportFileInputStreamData&)message).getRecordLengthMuSec();
		updateWithStreamData();
		return true;
	}
	else if (FileInput::MsgReportFileInputStreamTiming::match(message))
	{
		m_samplesCount = ((FileInput::MsgReportFileInputStreamTiming&)message).getSamplesCount();
		updateWithStreamTime();
		return true;
	}
	else if (FileInput::MsgStartStop::match(message))
    {
	    FileInput::MsgStartStop& notif = (FileInput::MsgStartStop&) message;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);

        return true;
    }
	else if (FileInput::MsgPlayPause::match(message))
	{
	    FileInput::MsgPlayPause& notif = (FileInput::MsgPlayPause&) message;
	    bool checked = notif.getPlayPause();
	    ui->play->setChecked(checked);
	    ui->navTimeSlider->setEnabled(!checked);
	    ui->acceleration->setEnabled(!checked);
	    m_enableNavTime = !checked;

	    return true;
	}
	else if (FileInput::MsgReportHeaderCRC::match(message))
	{
		FileInput::MsgReportHeaderCRC& notif = (FileInput::MsgReportHeaderCRC&) message;
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

void FileInputGUI::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_deviceSampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_deviceSampleRate / 1000));
}

void FileInputGUI::displaySettings()
{
    blockApplySettings(true);
    ui->playLoop->setChecked(m_settings.m_loop);
    ui->acceleration->setCurrentIndex(FileInputSettings::getAccelerationIndex(m_settings.m_accelerationFactor));
    if (!m_settings.m_fileName.isEmpty() && (m_settings.m_fileName != ui->fileNameText->text()))
    {
	ui->crcLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	configureFileName();
    }
    ui->fileNameText->setText(m_settings.m_fileName);
    blockApplySettings(false);
}

void FileInputGUI::sendSettings()
{
}

void FileInputGUI::on_playLoop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        m_settings.m_loop = checked;
        FileInput::MsgConfigureFileInput *message = FileInput::MsgConfigureFileInput::create(m_settings, QList<QString>{"loop"}, false);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void FileInputGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        FileInput::MsgStartStop *message = FileInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void FileInputGUI::updateStatus()
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

void FileInputGUI::on_play_toggled(bool checked)
{
	FileInput::MsgConfigureFileInputWork* message = FileInput::MsgConfigureFileInputWork::create(checked);
	m_sampleSource->getInputMessageQueue()->push(message);
	ui->navTimeSlider->setEnabled(!checked);
	ui->acceleration->setEnabled(!checked);
	m_enableNavTime = !checked;
}

void FileInputGUI::on_navTimeSlider_valueChanged(int value)
{
	if (m_enableNavTime && ((value >= 0) && (value <= 1000)))
	{
		FileInput::MsgConfigureFileSourceSeek* message = FileInput::MsgConfigureFileSourceSeek::create(value);
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}

void FileInputGUI::on_showFileDialog_clicked(bool checked)
{
    (void) checked;
	QString fileName = QFileDialog::getOpenFileName(this,
	    tr("Open I/Q record file"), ".", tr("SDR I/Q Files (*.sdriq *.wav)"), 0, QFileDialog::DontUseNativeDialog);

	if (fileName != "")
	{
		m_settings.m_fileName = fileName;
		ui->fileNameText->setText(m_settings.m_fileName);
		ui->crcLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
		configureFileName();
	}
}

void FileInputGUI::on_acceleration_currentIndexChanged(int index)
{
    if (m_doApplySettings)
    {
        m_settings.m_accelerationFactor = FileInputSettings::getAccelerationValue(index);
        FileInput::MsgConfigureFileInput *message = FileInput::MsgConfigureFileInput::create(m_settings, QList<QString>{"accelerationFactor"}, false);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void FileInputGUI::configureFileName()
{
	qDebug() << "FileInputGUI::configureFileName: " << m_settings.m_fileName.toStdString().c_str();
	FileInput::MsgConfigureFileSourceName* message = FileInput::MsgConfigureFileSourceName::create(m_settings.m_fileName);
	m_sampleSource->getInputMessageQueue()->push(message);
}

void FileInputGUI::updateWithAcquisition()
{
	ui->play->setEnabled(m_acquisition);
	ui->play->setChecked(m_acquisition);
	ui->showFileDialog->setEnabled(!m_acquisition);
}

void FileInputGUI::updateWithStreamData()
{
	ui->centerFrequency->setText(tr("%L1").arg(m_centerFrequency));
	ui->sampleRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
	ui->sampleSizeText->setText(tr("%1b").arg(m_sampleSize));
	ui->play->setEnabled(m_acquisition);
	QTime recordLength(0, 0, 0, 0);
	recordLength = recordLength.addMSecs(m_recordLengthMuSec/1000UL);
	QString s_time = recordLength.toString("HH:mm:ss.zzz");
	ui->recordLengthText->setText(s_time);
	updateWithStreamTime();
}

void FileInputGUI::updateWithStreamTime()
{
    qint64 t_sec = 0;
    qint64 t_msec = 0;

	if (m_sampleRate > 0)
	{
		t_sec = m_samplesCount / m_sampleRate;
        t_msec = (m_samplesCount - (t_sec * m_sampleRate)) * 1000LL / m_sampleRate;
	}

	QTime t(0, 0, 0, 0);
	t = t.addSecs(t_sec);
	t = t.addMSecs(t_msec);
	QString s_timems = t.toString("HH:mm:ss.zzz");
	ui->relTimeText->setText(s_timems);

    qint64 startingTimeStampMsec = m_startingTimeStamp;
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(startingTimeStampMsec);
    dt = dt.addSecs(t_sec);
    dt = dt.addMSecs(t_msec);
	QString s_date = dt.toString("yyyy-MM-dd HH:mm:ss.zzz");
	ui->absTimeText->setText(s_date);

	if (!m_enableNavTime)
	{
		float posRatio = (float) (t_sec*1000000L + t_msec*1000L) / (float) m_recordLengthMuSec;
		ui->navTimeSlider->setValue((int) (posRatio * 1000.0));
	}
}

void FileInputGUI::tick()
{
	if ((++m_tickCount & 0xf) == 0) {
		FileInput::MsgConfigureFileInputStreamTiming* message = FileInput::MsgConfigureFileInputStreamTiming::create();
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}

void FileInputGUI::setAccelerationCombo()
{
    ui->acceleration->blockSignals(true);
    ui->acceleration->clear();
    ui->acceleration->addItem(QString("1"));

    for (unsigned int i = 0; i <= FileInputSettings::m_accelerationMaxScale; i++)
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

void FileInputGUI::setNumberStr(int n, QString& s)
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

void FileInputGUI::openDeviceSettingsDialog(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuDeviceSettings)
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

    resetContextMenuType();
}

void FileInputGUI::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &FileInputGUI::on_startStop_toggled);
    QObject::connect(ui->playLoop, &ButtonSwitch::toggled, this, &FileInputGUI::on_playLoop_toggled);
    QObject::connect(ui->play, &ButtonSwitch::toggled, this, &FileInputGUI::on_play_toggled);
    QObject::connect(ui->navTimeSlider, &QSlider::valueChanged, this, &FileInputGUI::on_navTimeSlider_valueChanged);
    QObject::connect(ui->showFileDialog, &QPushButton::clicked, this, &FileInputGUI::on_showFileDialog_clicked);
    QObject::connect(ui->acceleration, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FileInputGUI::on_acceleration_currentIndexChanged);
}
