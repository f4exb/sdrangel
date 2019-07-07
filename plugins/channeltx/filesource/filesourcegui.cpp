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

#include "filesourcegui.h"

#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "dsp/hbfilterchainconverter.h"
#include "gui/basicchannelsettingsdialog.h"
#include "mainwindow.h"

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

void FileSourceGUI::setName(const QString& name)
{
    setObjectName(name);
}

QString FileSourceGUI::getName() const
{
    return objectName();
}

qint64 FileSourceGUI::getCenterFrequency() const {
    return 0;
}

void FileSourceGUI::setCenterFrequency(qint64 centerFrequency)
{
    (void) centerFrequency;
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
    if (FileSource::MsgSampleRateNotification::match(message))
    {
        FileSource::MsgSampleRateNotification& notif = (FileSource::MsgSampleRateNotification&) message;
        m_sampleRate = notif.getSampleRate();
        displayRateAndShift();
        return true;
    }
    else if (FileSource::MsgConfigureFileSource::match(message))
    {
        const FileSource::MsgConfigureFileSource& cfg = (FileSource::MsgConfigureFileSource&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else
    {
        return false;
    }
}

FileSourceGUI::FileSourceGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
        RollupWidget(parent),
        ui(new Ui::FileSourceGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_tickCount(0)
{
    (void) channelTx;
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_fileSource = (FileSource*) channelTx;
    m_fileSource->setMessageQueueToGUI(getInputMessageQueue());

    connect(&(m_deviceUISet->m_deviceAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Remote source");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);

    m_deviceUISet->registerTxChannelInstance(FileSource::m_channelIdURI, this);
    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    m_time.start();

    displaySettings();
    applySettings(true);
}

FileSourceGUI::~FileSourceGUI()
{
    m_deviceUISet->removeTxChannelInstance(this);
    delete m_fileSource;
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

void FileSourceGUI::applyChannelSettings()
{
    if (m_doApplySettings)
    {
        FileSource::MsgConfigureChannelizer *msgChan = FileSource::MsgConfigureChannelizer::create(
                m_settings.m_log2Interp,
                m_settings.m_filterChainHash);
        m_fileSource->getInputMessageQueue()->push(msgChan);
    }
}

void FileSourceGUI::configureFileName()
{
	qDebug() << "FileSourceGui::configureFileName: " << m_fileName.toStdString().c_str();
	FileSource::MsgConfigureFileSourceName* message = FileSource::MsgConfigureFileSourceName::create(m_fileName);
	m_fileSource->getInputMessageQueue()->push(message);
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

    blockApplySettings(true);
    ui->interpolationFactor->setCurrentIndex(m_settings.m_log2Interp);
    applyInterpolation();
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

void FileSourceGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void FileSourceGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
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

void FileSourceGUI::on_showFileDialog_clicked(bool checked)
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

    displayRateAndShift();
    applyChannelSettings();
}

void FileSourceGUI::tick()
{
    if (++m_tickCount == 20) // once per second
    {
        m_tickCount = 0;
    }
}

void FileSourceGUI::channelMarkerChangedByCursor()
{
}
