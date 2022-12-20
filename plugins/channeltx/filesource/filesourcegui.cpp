///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2019 Edouard Griffiths, F4EXB                              //
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

#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/dspcommands.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/dialogpositioner.h"
#include "util/db.h"

#include "mainwindow.h"

#include "filesourcereport.h"
#include "filesourcegui.h"
#include "filesource.h"
#include "ui_filesourcegui.h"

FileSourceGUI* FileSourceGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    FileSourceGUI* gui = new FileSourceGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void FileSourceGUI::destroy()
{
    delete this;
}

void FileSourceGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray FileSourceGUI::serialize() const
{
    return m_settings.serialize();
}

bool FileSourceGUI::deserialize(const QByteArray& data)
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

bool FileSourceGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_sampleRate = notif.getSampleRate();
        updateAbsoluteCenterFrequency();
        displayRateAndShift();
        return true;
    }
    else if (FileSource::MsgConfigureFileSource::match(message)) // API settings feedback
    {
        const FileSource::MsgConfigureFileSource& cfg = (FileSource::MsgConfigureFileSource&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (FileSource::MsgReportFileSourceAcquisition::match(message))
	{
		m_acquisition = ((FileSource::MsgReportFileSourceAcquisition&)message).getAcquisition();
		updateWithAcquisition();
		return true;
	}
	else if (FileSourceReport::MsgReportFileSourceStreamData::match(message))
	{
		m_fileSampleRate = ((FileSourceReport::MsgReportFileSourceStreamData&)message).getSampleRate();
		m_fileSampleSize = ((FileSourceReport::MsgReportFileSourceStreamData&)message).getSampleSize();
		m_startingTimeStamp = ((FileSourceReport::MsgReportFileSourceStreamData&)message).getStartingTimeStamp();
		m_recordLengthMuSec = ((FileSourceReport::MsgReportFileSourceStreamData&)message).getRecordLengthMuSec();
		updateWithStreamData();
		return true;
	}
	else if (FileSourceReport::MsgReportFileSourceStreamTiming::match(message))
	{
		m_samplesCount = ((FileSourceReport::MsgReportFileSourceStreamTiming&)message).getSamplesCount();
		updateWithStreamTime();
		return true;
	}
	else if (FileSourceReport::MsgPlayPause::match(message))
	{
	    FileSourceReport::MsgPlayPause& notif = (FileSourceReport::MsgPlayPause&) message;
	    bool checked = notif.getPlayPause();
	    ui->play->setChecked(checked);
	    ui->navTime->setEnabled(!checked);
	    m_enableNavTime = !checked;

	    return true;
	}
	else if (FileSourceReport::MsgReportHeaderCRC::match(message))
	{
		FileSourceReport::MsgReportHeaderCRC& notif = (FileSourceReport::MsgReportHeaderCRC&) message;

        if (notif.isOK()) {
			ui->crcLabel->setStyleSheet("QLabel { background-color : green; }");
		} else {
			ui->crcLabel->setStyleSheet("QLabel { background-color : red; }");
		}

		return true;
	}
    else if (FileSource::MsgConfigureFileSourceWork::match(message)) // API action "play" feedback
    {
        const FileSource::MsgConfigureFileSourceWork& notif = (const FileSource::MsgConfigureFileSourceWork&) message;
        bool play = notif.isWorking();
        ui->play->blockSignals(true);
        ui->navTime->blockSignals(true);
        ui->play->setChecked(play);
        ui->navTime->setEnabled(!play);
        m_enableNavTime = !play;
        ui->play->blockSignals(false);
        ui->navTime->blockSignals(false);

        return true;
    }
    else if (FileSource::MsgConfigureFileSourceSeek::match(message)) // API action "seekms" feedback
    {
        const FileSource::MsgConfigureFileSourceSeek& notif = (FileSource::MsgConfigureFileSourceSeek&) message;
        int seekMillis = notif.getMillis();
        ui->navTime->blockSignals(true);
        ui->navTime->setValue(seekMillis);
        ui->navTime->blockSignals(false);

        return true;
    }
    else
    {
        return false;
    }
}

FileSourceGUI::FileSourceGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
        ChannelGUI(parent),
        ui(new Ui::FileSourceGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_deviceCenterFrequency(0),
        m_sampleRate(0),
        m_shiftFrequencyFactor(0.0),
        m_fileSampleRate(0),
        m_fileSampleSize(0),
        m_recordLengthMuSec(0),
        m_startingTimeStamp(0),
        m_samplesCount(0),
        m_acquisition(false),
        m_enableNavTime(false),
        m_doApplySettings(true),
        m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/filesource/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_fileSource = (FileSource*) channelTx;
    m_fileSource->setMessageQueueToGUI(getInputMessageQueue());

    connect(&(m_deviceUISet->m_deviceAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("File source");
    m_channelMarker.setMovable(false); // do not let user move the center arbitrarily
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    displaySettings();
    makeUIConnections();
    applySettings(true);
}

FileSourceGUI::~FileSourceGUI()
{
    delete ui;
}

void FileSourceGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void FileSourceGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        FileSource::MsgConfigureFileSource* message = FileSource::MsgConfigureFileSource::create(m_settings, force);
        m_fileSource->getInputMessageQueue()->push(message);
    }
}

void FileSourceGUI::configureFileName()
{
	qDebug() << "FileSourceGui::configureFileName: " << m_settings.m_fileName.toStdString().c_str();
    applySettings();
}

void FileSourceGUI::updateWithAcquisition()
{
    ui->play->blockSignals(true);
	ui->play->setChecked(m_acquisition);
    ui->play->blockSignals(false);
	ui->showFileDialog->setEnabled(!m_acquisition);
}

void FileSourceGUI::updateWithStreamData()
{
	ui->sampleRateText->setText(tr("%1k").arg((float) m_fileSampleRate / 1000));
	ui->sampleSizeText->setText(tr("%1b").arg(m_fileSampleSize));
	QTime recordLength(0, 0, 0, 0);
	recordLength = recordLength.addMSecs(m_recordLengthMuSec/1000UL);
	QString s_time = recordLength.toString("HH:mm:ss.zzz");
	ui->recordLengthText->setText(s_time);
	updateWithStreamTime();
}

void FileSourceGUI::updateWithStreamTime()
{
    qint64 t_sec = 0;
    qint64 t_msec = 0;

	if (m_fileSampleRate > 0)
    {
		t_sec = m_samplesCount / m_fileSampleRate;
        t_msec = (m_samplesCount - (t_sec * m_fileSampleRate)) * 1000LL / m_fileSampleRate;
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
		float posRatio = (float) (t_sec*1000000L + t_msec*1000L) / (float) m_recordLengthMuSec;
		ui->navTime->setValue((int) (posRatio * 1000.0));
	}
}

void FileSourceGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_sampleRate);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    updateIndexLabel();

    blockApplySettings(true);
    ui->fileNameText->setText(m_settings.m_fileName);
    ui->gain->setValue(m_settings.m_gainDB);
    ui->gainText->setText(tr("%1 dB").arg(m_settings.m_gainDB));
    ui->interpolationFactor->setCurrentIndex(m_settings.m_log2Interp);
    applyInterpolation();
    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void FileSourceGUI::displayRateAndShift()
{
    int shift = m_shiftFrequencyFactor * m_sampleRate;
    double channelSampleRate = ((double) m_sampleRate) / (1<<m_settings.m_log2Interp);
    QLocale loc;
    ui->offsetFrequencyText->setText(tr("%1 Hz").arg(loc.toString(shift)));
    ui->channelRateText->setText(tr("%1k").arg(QString::number(channelSampleRate / 1000.0, 'g', 5)));
    m_channelMarker.setCenterFrequency(shift);
    m_channelMarker.setBandwidth(channelSampleRate);
}

void FileSourceGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void FileSourceGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void FileSourceGUI::handleSourceMessages()
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

void FileSourceGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void FileSourceGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.setDefaultTitle(m_displayedName);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_fileSource->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, false);
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

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings();
    }

    resetContextMenuType();
}

void FileSourceGUI::on_interpolationFactor_currentIndexChanged(int index)
{
    m_settings.m_log2Interp = index;
    applyInterpolation();
}

void FileSourceGUI::on_position_valueChanged(int value)
{
    m_settings.m_filterChainHash = value;
    applyPosition();
}

void FileSourceGUI::on_gain_valueChanged(int value)
{
    ui->gainText->setText(tr("%1 dB").arg(value));
    m_settings.m_gainDB = value;
    applySettings();
}

void FileSourceGUI::on_showFileDialog_clicked(bool checked)
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

void FileSourceGUI::on_playLoop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        m_settings.m_loop = checked;
        FileSource::MsgConfigureFileSource *message = FileSource::MsgConfigureFileSource::create(m_settings, false);
        m_fileSource->getInputMessageQueue()->push(message);
    }
}

void FileSourceGUI::on_play_toggled(bool checked)
{
	FileSource::MsgConfigureFileSourceWork* message = FileSource::MsgConfigureFileSourceWork::create(checked);
	m_fileSource->getInputMessageQueue()->push(message);
	ui->navTime->setEnabled(!checked);
	m_enableNavTime = !checked;
}

void FileSourceGUI::on_navTime_valueChanged(int value)
{
	if (m_enableNavTime && ((value >= 0) && (value <= 1000)))
	{
		FileSource::MsgConfigureFileSourceSeek* message = FileSource::MsgConfigureFileSourceSeek::create(value);
		m_fileSource->getInputMessageQueue()->push(message);
	}
}

void FileSourceGUI::applyInterpolation()
{
    uint32_t maxHash = 1;

    for (uint32_t i = 0; i < m_settings.m_log2Interp; i++) {
        maxHash *= 3;
    }

    ui->position->setMaximum(maxHash-1);
    ui->position->setValue(m_settings.m_filterChainHash);
    m_settings.m_filterChainHash = ui->position->value();
    applyPosition();
}

void FileSourceGUI::applyPosition()
{
    ui->filterChainIndex->setText(tr("%1").arg(m_settings.m_filterChainHash));
    QString s;
    m_shiftFrequencyFactor = HBFilterChainConverter::convertToString(m_settings.m_log2Interp, m_settings.m_filterChainHash, s);
    ui->filterChainText->setText(s);

    updateAbsoluteCenterFrequency();
    displayRateAndShift();
    applySettings();
}

void FileSourceGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_fileSource->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    if (++m_tickCount == 20) // once per second
    {
		FileSource::MsgConfigureFileSourceStreamTiming* message = FileSource::MsgConfigureFileSourceStreamTiming::create();
		m_fileSource->getInputMessageQueue()->push(message);
        m_tickCount = 0;
    }
}

void FileSourceGUI::channelMarkerChangedByCursor()
{
}

void FileSourceGUI::makeUIConnections()
{
    QObject::connect(ui->interpolationFactor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FileSourceGUI::on_interpolationFactor_currentIndexChanged);
    QObject::connect(ui->position, &QSlider::valueChanged, this, &FileSourceGUI::on_position_valueChanged);
    QObject::connect(ui->gain, &QSlider::valueChanged, this, &FileSourceGUI::on_gain_valueChanged);
    QObject::connect(ui->showFileDialog, &QPushButton::clicked, this, &FileSourceGUI::on_showFileDialog_clicked);
    QObject::connect(ui->playLoop, &ButtonSwitch::toggled, this, &FileSourceGUI::on_playLoop_toggled);
    QObject::connect(ui->play, &ButtonSwitch::toggled, this, &FileSourceGUI::on_play_toggled);
    QObject::connect(ui->navTime, &QSlider::valueChanged, this, &FileSourceGUI::on_navTime_valueChanged);
}

void FileSourceGUI::updateAbsoluteCenterFrequency()
{
    int shift = m_shiftFrequencyFactor * m_sampleRate;
    setStatusFrequency(m_deviceCenterFrequency + shift);
}
