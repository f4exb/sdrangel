///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include "ui_sigmffileinputgui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/filerecordinterface.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"

#include "mainwindow.h"

#include "recordinfodialog.h"
#include "sigmffileinputgui.h"

SigMFFileInputGUI::SigMFFileInputGUI(DeviceUISet *deviceUISet, QWidget* parent) :
	DeviceGUI(parent),
	ui(new Ui::SigMFFileInputGUI),
	m_settings(),
    m_forceSettings(false),
    m_currentTrackIndex(0),
	m_doApplySettings(true),
	m_sampleSource(0),
	m_startStop(false),
    m_trackMode(false),
	m_metaFileName("..."),
	m_sampleRate(48000),
	m_centerFrequency(0),
	m_recordLength(0),
	m_startingTimeStamp(0),
	m_samplesCount(0),
	m_tickCount(0),
	m_enableTrackNavTime(false),
	m_enableFullNavTime(false),
	m_lastEngineState(DeviceAPI::StNotStarted)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/samplesource/sigmffileinput/readme.md";
    QWidget *contents = getContents();
	ui->setupUi(contents);
    sizeToContents();
    contents->setStyleSheet("#SigMFFileInputGUI { background-color: rgb(64, 64, 64); }");

    ui->fileNameText->setText(m_metaFileName);
	ui->crcLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    ui->captureTable->setSelectionMode(QAbstractItemView::NoSelection);

	connect(&(m_deviceUISet->m_deviceAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

	setAccelerationCombo();
	displaySettings();
    makeUIConnections();
    updateStartStop();

	ui->trackNavTimeSlider->setEnabled(false);
    ui->fullNavTimeSlider->setEnabled(false);
	ui->acceleration->setEnabled(false);
    ui->playFull->setEnabled(false);
    ui->playFull->setChecked(false);
    ui->playTrack->setEnabled(false);
    ui->playTrack->setChecked(false);

    m_sampleSource = m_deviceUISet->m_deviceAPI->getSampleSource();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);
}

SigMFFileInputGUI::~SigMFFileInputGUI()
{
    m_statusTimer.stop();
	delete ui;
}

void SigMFFileInputGUI::destroy()
{
	delete this;
}

void SigMFFileInputGUI::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
    m_forceSettings = true;
	sendSettings();
}

QByteArray SigMFFileInputGUI::serialize() const
{
	return m_settings.serialize();
}

bool SigMFFileInputGUI::deserialize(const QByteArray& data)
{
	if(m_settings.deserialize(data))
    {
		displaySettings();
        m_forceSettings = true;
		sendSettings();
		return true;
	} else
    {
		resetToDefaults();
		return false;
	}
}

void SigMFFileInputGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_deviceSampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("SigMFFileInputGUI::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

bool SigMFFileInputGUI::handleMessage(const Message& message)
{
    if (SigMFFileInput::MsgConfigureSigMFFileInput::match(message))
    {
        const SigMFFileInput::MsgConfigureSigMFFileInput& cfg = (SigMFFileInput::MsgConfigureSigMFFileInput&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        displaySettings();
        return true;
    }
    else if (SigMFFileInput::MsgReportStartStop::match(message))
	{
        SigMFFileInput::MsgReportStartStop& report = (SigMFFileInput::MsgReportStartStop&) message;
		m_startStop = report.getStartStop();
		updateStartStop();
		return true;
	}
    else if (SigMFFileInput::MsgReportMetaData::match(message))
    {
        SigMFFileInput::MsgReportMetaData& report = (SigMFFileInput::MsgReportMetaData&) message;
        m_metaInfo = report.getMetaInfo();

        m_recordInfo = QString("Meta file..: %1\n").arg(m_metaFileName);

        if (m_metaInfo.m_sdrAngelVersion.size() == 0)
        {
            if (m_metaInfo.m_description.size() > 0) {
                m_recordInfo += QString("Description: %1\n").arg(m_metaInfo.m_description);
            }
            if (m_metaInfo.m_author.size() > 0) {
                m_recordInfo += QString("Author.....: %1\n").arg(m_metaInfo.m_author);
            }
            if (m_metaInfo.m_license.size() > 0) {
                m_recordInfo += QString("License....: %1\n").arg(m_metaInfo.m_license);
            }
            if (m_metaInfo.m_sigMFVersion.size() > 0) {
                m_recordInfo += QString("Version....: %1\n").arg(m_metaInfo.m_sigMFVersion);
            }
            if (m_metaInfo.m_hw.size() > 0) {
                m_recordInfo += QString("Hardware...: %1\n").arg(m_metaInfo.m_hw);
            }

            m_recordInfo += QString("Data type..: %1\n").arg(m_metaInfo.m_dataTypeStr);
            m_recordInfo += QString("Swap I/Q...: %1\n").arg(m_metaInfo.m_dataType.m_swapIQ ? "yes" : "no");
            m_recordInfo += QString("Nb samples.: %1 (%2S)\n").arg(m_metaInfo.m_totalSamples).arg(displayScaled(m_metaInfo.m_totalSamples, 3));
            m_recordInfo += QString("Nb captures: %1\n").arg(m_metaInfo.m_nbCaptures);
            m_recordInfo += QString("Nb annot...: %1\n").arg(m_metaInfo.m_nbAnnotations);

            ui->infoSummaryText->setText("Not recorded with SDRangel");
        }
        else
        {
            m_recordInfo += QString("Recorder...: %1\n").arg(m_metaInfo.m_recorder);
            m_recordInfo += QString("Hardware...: %1\n").arg(m_metaInfo.m_hw);
            m_recordInfo += QString("Data type..: %1\n").arg(m_metaInfo.m_dataTypeStr);
            m_recordInfo += QString("Core SRate.: %1 S/s\n").arg(m_metaInfo.m_coreSampleRate);
            m_recordInfo += QString("Nb samples.: %1 (%2S)\n").arg(m_metaInfo.m_totalSamples).arg(displayScaled(m_metaInfo.m_totalSamples, 3));
            m_recordInfo += QString("Nb captures: %1\n").arg(m_metaInfo.m_nbCaptures);
            m_recordInfo += QString("SDRangel application info:\n");
            m_recordInfo += QString("Version....: v%1\n").arg(m_metaInfo.m_sdrAngelVersion);
            m_recordInfo += QString("Qt version.: %1\n").arg(m_metaInfo.m_qtVersion);
            m_recordInfo += QString("Rx bits....: %1 bits\n").arg(m_metaInfo.m_rxBits);
            m_recordInfo += QString("Arch.......: %1\n").arg(m_metaInfo.m_arch);
            m_recordInfo += QString("O/S........: %1\n").arg(m_metaInfo.m_os);

            ui->infoSummaryText->setText(QString("%1 Rx %2 bits v%3")
                .arg(m_metaInfo.m_recorder)
                .arg(m_metaInfo.m_rxBits)
                .arg(m_metaInfo.m_sdrAngelVersion));
        }

        m_captures = report.getCaptures();
        addCaptures(m_captures);
        m_centerFrequency = m_captures.size() > 0 ? m_captures.at(0).m_centerFrequency : 0;
        m_recordLength = m_captures.size() > 0 ? m_captures.at(0).m_length : m_metaInfo.m_totalSamples;
        m_startingTimeStamp = m_captures.size() > 0 ? m_captures.at(0).m_tsms : 0;
        m_sampleRate = m_metaInfo.m_coreSampleRate;
        m_sampleSize = m_metaInfo.m_dataType.m_sampleBits;

        QTime recordLength(0, 0, 0, 0);
        recordLength = recordLength.addMSecs(m_metaInfo.m_totalTimeMs);
        QString s_time = recordLength.toString("HH:mm:ss");
        ui->fullRecordLengthText->setText(s_time);
        m_trackSamplesCount = 0;
        m_trackTimeStart = 0;

    	ui->sampleSizeText->setText(tr("%1%2%3b")
        .arg(m_metaInfo.m_dataType.m_complex ? "c" : "r")
        .arg(m_metaInfo.m_dataType.m_floatingPoint ? "f" : m_metaInfo.m_dataType.m_signed ? "i" : "u")
        .arg(m_sampleSize));

        updateWithStreamData();

        return true;
    }
	else if (SigMFFileInput::MsgReportTrackChange::match(message))
	{
        SigMFFileInput::MsgReportTrackChange& report = (SigMFFileInput::MsgReportTrackChange&) message;
        m_currentTrackIndex = report.getTrackIndex();
        qDebug("SigMFFileInputGUI::handleMessage MsgReportTrackChange: m_currentTrackIndex: %d", m_currentTrackIndex);
        m_centerFrequency = m_captures.at(m_currentTrackIndex).m_centerFrequency;
        m_sampleRate = m_captures.at(m_currentTrackIndex).m_sampleRate;
        m_recordLength = m_captures.at(m_currentTrackIndex).m_length;
        m_startingTimeStamp = m_captures.at(m_currentTrackIndex).m_tsms;
        m_samplesCount = m_captures.at(m_currentTrackIndex).m_sampleStart;
        m_trackSamplesCount = 0;
        updateWithStreamData();

		return true;
	}
	else if (SigMFFileInput::MsgReportFileInputStreamTiming::match(message))
	{
        SigMFFileInput::MsgReportFileInputStreamTiming& report = (SigMFFileInput::MsgReportFileInputStreamTiming&) message;
		m_samplesCount = report.getSamplesCount();
        m_trackSamplesCount = report.getTrackSamplesCount();
        m_trackTimeStart = report.getTrackTimeStart();
        m_trackNumber = report.getTrackNumber();
		updateWithStreamTime();
		return true;
	}
	else if (SigMFFileInput::MsgStartStop::match(message))
    {
	    SigMFFileInput::MsgStartStop& notif = (SigMFFileInput::MsgStartStop&) message;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);

        return true;
    }
	else if (SigMFFileInput::MsgReportCRC::match(message))
	{
		SigMFFileInput::MsgReportCRC& notif = (SigMFFileInput::MsgReportCRC&) message;

		if (notif.isOK()) {
			ui->crcLabel->setStyleSheet("QLabel { background-color : green; }");
		} else {
			ui->crcLabel->setStyleSheet("QLabel { background-color : red; }");
		}

		return true;
	}
	else if (SigMFFileInput::MsgReportTotalSamplesCheck::match(message))
	{
		SigMFFileInput::MsgReportTotalSamplesCheck& notif = (SigMFFileInput::MsgReportTotalSamplesCheck&) message;

		if (notif.isOK()) {
			ui->totalLabel->setStyleSheet("QLabel { background-color : green; }");
		} else {
			ui->totalLabel->setStyleSheet("QLabel { background-color : red; }");
		}

		return true;
	}
	else
	{
		return false;
	}
}

void SigMFFileInputGUI::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_deviceSampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_deviceSampleRate / 1000));
}

void SigMFFileInputGUI::displaySettings()
{
    blockApplySettings(true);
    ui->playTrackLoop->setChecked(m_settings.m_trackLoop);
    ui->playFullLoop->setChecked(m_settings.m_fullLoop);
    ui->acceleration->setCurrentIndex(SigMFFileInputSettings::getAccelerationIndex(m_settings.m_accelerationFactor));
    blockApplySettings(false);
}

QString SigMFFileInputGUI::displayScaled(uint64_t value, int precision)
{
    if (value < 1000) {
        return tr("%1").arg(QString::number(value, 'f', precision));
    } else if (value < 1000000) {
        return tr("%1k").arg(QString::number(value / 1000.0, 'f', precision));
    } else if (value < 1000000000) {
        return tr("%1M").arg(QString::number(value / 1000000.0, 'f', precision));
    } else if (value < 1000000000000) {
        return tr("%1G").arg(QString::number(value / 1000000000.0, 'f', precision));
    } else {
        return tr("%1").arg(QString::number(value, 'e', precision));
    }
}

void SigMFFileInputGUI::addCaptures(const QList<SigMFFileCapture>& captures)
{
    ui->captureTable->setRowCount(captures.size());

    for (int i = 0; i < captures.size(); i++)
    {
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(captures.at(i).m_tsms);
        unsigned int sampleRate = captures.at(i).m_sampleRate;
        ui->captureTable->setItem(i, 0, new QTableWidgetItem(dateTime.toString("yyyy-MM-ddTHH:mm:ss")));
        ui->captureTable->setItem(i, 1, new QTableWidgetItem(displayScaled(captures.at(i).m_centerFrequency, 5)));
        ui->captureTable->setItem(i, 2, new QTableWidgetItem(displayScaled(sampleRate, 2)));
        unsigned int milliseconds = (captures.at(i).m_length * 1000) / sampleRate;
        QTime t = QTime::fromMSecsSinceStartOfDay(milliseconds);
        ui->captureTable->setItem(i, 3, new QTableWidgetItem(t.toString("HH:mm:ss")));

        for (int j = 0; j < 4; j++)
        {
            ui->captureTable->item(i, j)->setFlags(ui->captureTable->item(i, j)->flags() & ~Qt::ItemIsEditable);
            ui->captureTable->item(i, j)->setTextAlignment(Qt::AlignRight);
        }
    }

    ui->captureTable->resizeRowsToContents();
    ui->captureTable->resizeColumnsToContents();
}

void SigMFFileInputGUI::sendSettings()
{
    if (m_doApplySettings)
    {
        SigMFFileInput::MsgConfigureSigMFFileInput *message = SigMFFileInput::MsgConfigureSigMFFileInput::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
    }
}

void SigMFFileInputGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        SigMFFileInput::MsgStartStop *message = SigMFFileInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void SigMFFileInputGUI::on_infoDetails_clicked()
{
    RecordInfoDialog infoDialog(m_recordInfo, this);
    infoDialog.exec();
}

void SigMFFileInputGUI::on_captureTable_itemSelectionChanged()
{
    QList<QTableWidgetItem *> selectedItems = ui->captureTable->selectedItems();

    if (selectedItems.size() == 0)
    {
        qDebug("SigMFFileInputGUI::on_captureTable_itemSelectionChanged: no selection");
    }
    else
    {
        int trackIndex = selectedItems.front()->row();
        qDebug("SigMFFileInputGUI::on_captureTable_itemSelectionChanged: row: %d", trackIndex);
        SigMFFileInput::MsgConfigureTrackIndex *message = SigMFFileInput::MsgConfigureTrackIndex::create(trackIndex);
        m_sampleSource->getInputMessageQueue()->push(message);

		ui->trackNavTimeSlider->setValue(0);
        float posRatio = (float) m_captures[trackIndex].m_sampleStart / (float) m_metaInfo.m_totalSamples;
        ui->fullNavTimeSlider->setValue((int) (posRatio * 1000.0));
    }
}

void SigMFFileInputGUI::on_trackNavTimeSlider_valueChanged(int value)
{
	if (m_enableTrackNavTime && ((value >= 0) && (value <= 1000)))
	{
		SigMFFileInput::MsgConfigureTrackSeek* message = SigMFFileInput::MsgConfigureTrackSeek::create(value);
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}

void SigMFFileInputGUI::on_fullNavTimeSlider_valueChanged(int value)
{
	if (m_enableFullNavTime && ((value >= 0) && (value <= 1000)))
	{
		SigMFFileInput::MsgConfigureFileSeek* message = SigMFFileInput::MsgConfigureFileSeek::create(value);
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}

void SigMFFileInputGUI::on_playTrackLoop_toggled(bool checked)
{
    m_settings.m_trackLoop = checked;
    m_settingsKeys.append("trackLoop");
    sendSettings();
}

void SigMFFileInputGUI::on_playTrack_toggled(bool checked)
{
	SigMFFileInput::MsgConfigureTrackWork* message = SigMFFileInput::MsgConfigureTrackWork::create(checked);
	m_sampleSource->getInputMessageQueue()->push(message);
	ui->trackNavTimeSlider->setEnabled(!checked);
    ui->fullNavTimeSlider->setEnabled(!checked);
	ui->acceleration->setEnabled(!checked);
	m_enableTrackNavTime = !checked;
    m_enableFullNavTime = !checked;
    ui->playFull->setEnabled(!checked);
    ui->playFull->setChecked(false);
    ui->captureTable->setSelectionMode(checked ? QAbstractItemView::NoSelection : QAbstractItemView::ExtendedSelection);
}

void SigMFFileInputGUI::on_playFullLoop_toggled(bool checked)
{
    m_settings.m_fullLoop = checked;
    m_settingsKeys.append("fullLoop");
    sendSettings();
}

void SigMFFileInputGUI::on_playFull_toggled(bool checked)
{
	SigMFFileInput::MsgConfigureFileWork* message = SigMFFileInput::MsgConfigureFileWork::create(checked);
	m_sampleSource->getInputMessageQueue()->push(message);
    ui->trackNavTimeSlider->setEnabled(!checked);
	ui->fullNavTimeSlider->setEnabled(!checked);
	ui->acceleration->setEnabled(!checked);
	m_enableTrackNavTime = !checked;
	m_enableFullNavTime = !checked;
    ui->playTrack->setEnabled(!checked);
    ui->playTrack->setChecked(false);
    ui->captureTable->setSelectionMode(checked ? QAbstractItemView::NoSelection : QAbstractItemView::ExtendedSelection);
}

void SigMFFileInputGUI::on_acceleration_currentIndexChanged(int index)
{
    m_settings.m_accelerationFactor = SigMFFileInputSettings::getAccelerationValue(index);
    m_settingsKeys.append("accelerationFactor");
    sendSettings();
}

void SigMFFileInputGUI::updateStatus()
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

void SigMFFileInputGUI::on_showFileDialog_clicked(bool checked)
{
    (void) checked;
	QString fileName = QFileDialog::getOpenFileName(
            this,
	        tr("Open SigMF I/Q record file"),
            ".",
            tr("SigMF Files (*.sigmf-meta)"),
            nullptr,
            QFileDialog::DontUseNativeDialog
    );

	if (fileName != "")
	{
		m_metaFileName = fileName;
		ui->fileNameText->setText(m_metaFileName);
		ui->crcLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
		configureFileName();
	}
}

void SigMFFileInputGUI::configureFileName()
{
	qDebug() << "SigMFFileInputGUI::configureFileName: " << m_metaFileName.toStdString().c_str();

    QString fileBase;
    FileRecordInterface::RecordType recordType = FileRecordInterface::guessTypeFromFileName(m_metaFileName, fileBase);

    if (recordType == FileRecordInterface::RecordTypeSigMF)
    {
        m_settings.m_fileName = fileBase;
        m_settingsKeys.append("fileName");
        sendSettings();
    }
}

void SigMFFileInputGUI::updateStartStop()
{
    qDebug("SigMFFileInputGUI::updateStartStop: %s", m_startStop ? "start" : "stop");
    // always start in file mode
    ui->playFull->setEnabled(m_startStop);
    ui->playFull->setChecked(m_startStop);
    ui->playTrack->setEnabled(false);
    ui->playTrack->setChecked(false);
    ui->trackNavTimeSlider->setEnabled(false);
    ui->fullNavTimeSlider->setEnabled(false);
	ui->showFileDialog->setEnabled(!m_startStop);
    ui->captureTable->setSelectionMode(QAbstractItemView::NoSelection);
}

void SigMFFileInputGUI::updateWithStreamData()
{
    ui->captureTable->blockSignals(true);
    ui->captureTable->setRangeSelected(
        QTableWidgetSelectionRange(0, 0, ui->captureTable->rowCount() - 1, ui->captureTable->columnCount() - 1), false);
    ui->captureTable->setRangeSelected(
        QTableWidgetSelectionRange(m_currentTrackIndex, 0, m_currentTrackIndex, ui->captureTable->columnCount() - 1), true);
    ui->captureTable->blockSignals(false);

    ui->trackNumberText->setText(tr("%1").arg(m_currentTrackIndex + 1, 3, 10, QChar('0')));
    ui->centerFrequency->setText(tr("%L1").arg(m_centerFrequency));
	ui->sampleRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
	QTime recordLength(0, 0, 0, 0);
	recordLength = recordLength.addSecs(m_recordLength / m_sampleRate);
	QString s_time = recordLength.toString("HH:mm:ss");
	ui->trackRecordLengthText->setText(s_time);

	updateWithStreamTime();
}

void SigMFFileInputGUI::updateWithStreamTime()
{
    qint64 track_sec = 0;
    qint64 track_msec = 0;

	if (m_sampleRate > 0)
    {
		track_sec = m_trackSamplesCount / m_sampleRate;
        track_msec = (m_trackSamplesCount - (track_sec * m_sampleRate)) * 1000LL / m_sampleRate;
	}

	QTime t(0, 0, 0, 0);
	t = t.addSecs(track_sec);
	t = t.addMSecs(track_msec);
	QString s_timems = t.toString("HH:mm:ss.zzz");
	ui->trackRelTimeText->setText(s_timems);

    t = t.addMSecs(m_trackTimeStart);
    s_timems = t.toString("HH:mm:ss.zzz");
    ui->fullRelTimeText->setText(s_timems);

	QDateTime dt = QDateTime::fromMSecsSinceEpoch(m_startingTimeStamp);
    dt = dt.addSecs(track_sec);
    dt = dt.addMSecs(track_msec);
	QString s_date = dt.toString("yyyy-MM-dd HH:mm:ss.zzz");
	ui->absTimeText->setText(s_date);

	if (!ui->trackNavTimeSlider->isEnabled())
	{
		float posRatio = (float) m_trackSamplesCount / (float) m_recordLength;
		ui->trackNavTimeSlider->setValue((int) (posRatio * 1000.0));
	}

    if (!ui->fullNavTimeSlider->isEnabled())
    {
        float posRatio = (float) m_samplesCount / (float) m_metaInfo.m_totalSamples;
        ui->fullNavTimeSlider->setValue((int) (posRatio * 1000.0));
    }
}

void SigMFFileInputGUI::tick()
{
	if ((++m_tickCount & 0xf) == 0) {
		SigMFFileInput::MsgConfigureFileInputStreamTiming* message = SigMFFileInput::MsgConfigureFileInputStreamTiming::create();
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}

void SigMFFileInputGUI::setAccelerationCombo()
{
    ui->acceleration->blockSignals(true);
    ui->acceleration->clear();
    ui->acceleration->addItem(QString("1"));

    for (unsigned int i = 0; i <= SigMFFileInputSettings::m_accelerationMaxScale; i++)
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

void SigMFFileInputGUI::setNumberStr(int n, QString& s)
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

void SigMFFileInputGUI::openDeviceSettingsDialog(const QPoint& p)
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
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIDeviceIndex");

        sendSettings();
    }

    resetContextMenuType();
}

void SigMFFileInputGUI::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &SigMFFileInputGUI::on_startStop_toggled);
    QObject::connect(ui->infoDetails, &QPushButton::clicked, this, &SigMFFileInputGUI::on_infoDetails_clicked);
    QObject::connect(ui->captureTable, &QTableWidget::itemSelectionChanged, this, &SigMFFileInputGUI::on_captureTable_itemSelectionChanged);
    QObject::connect(ui->trackNavTimeSlider, &QSlider::valueChanged, this, &SigMFFileInputGUI::on_trackNavTimeSlider_valueChanged);
    QObject::connect(ui->playTrack, &ButtonSwitch::toggled, this, &SigMFFileInputGUI::on_playTrack_toggled);
    QObject::connect(ui->playTrackLoop, &ButtonSwitch::toggled, this, &SigMFFileInputGUI::on_playTrackLoop_toggled);
    QObject::connect(ui->fullNavTimeSlider, &QSlider::valueChanged, this, &SigMFFileInputGUI::on_fullNavTimeSlider_valueChanged);
    QObject::connect(ui->playFull, &ButtonSwitch::toggled, this, &SigMFFileInputGUI::on_playFull_toggled);
    QObject::connect(ui->playFullLoop, &ButtonSwitch::toggled, this, &SigMFFileInputGUI::on_playFullLoop_toggled);
    QObject::connect(ui->showFileDialog, &QPushButton::clicked, this, &SigMFFileInputGUI::on_showFileDialog_clicked);
    QObject::connect(ui->acceleration, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SigMFFileInputGUI::on_acceleration_currentIndexChanged);
}
