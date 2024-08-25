///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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
#include <ctype.h>

#include <QDebug>
#include <QAction>
#include <QRegularExpression>

#include "dabdemodgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_dabdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "gui/audioselectdialog.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/crightclickenabler.h"
#include "gui/dialogpositioner.h"
#include "channel/channelwebapiutils.h"
#include "feature/featurewebapiutils.h"
#include "maincore.h"

#include "dabdemod.h"

// Table column indexes
#define PROGRAMS_COL_NAME            0
#define PROGRAMS_COL_ID              1
#define PROGRAMS_COL_FREQUENCY       2
#define PROGRAMS_COL_ENSEMBLE        3

void DABDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    int row = ui->programs->rowCount();
    ui->programs->setRowCount(row + 1);
    ui->programs->setItem(row, PROGRAMS_COL_NAME, new QTableWidgetItem("Some Random Radio Station"));
    ui->programs->setItem(row, PROGRAMS_COL_ID, new QTableWidgetItem("123456"));
    ui->programs->setItem(row, PROGRAMS_COL_FREQUENCY, new QTableWidgetItem("200.000"));
    ui->programs->setItem(row, PROGRAMS_COL_ENSEMBLE, new QTableWidgetItem("Some random ensemble"));
    ui->programs->resizeColumnsToContents();
    ui->programs->removeRow(row);
}

// Columns in table reordered
void DABDemodGUI::programs_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_columnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void DABDemodGUI::programs_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_columnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void DABDemodGUI::columnSelectMenu(QPoint pos)
{
    menu->popup(ui->programs->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void DABDemodGUI::columnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->programs->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction *DABDemodGUI::createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(columnSelectMenuChecked()));
    return action;
}

DABDemodGUI* DABDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    DABDemodGUI* gui = new DABDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void DABDemodGUI::destroy()
{
    delete this;
}

void DABDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray DABDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool DABDemodGUI::deserialize(const QByteArray& data)
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

int DABDemodGUI::findProgramRowById(int id)
{
    QString idText = QString::number(id);
    for (int i = 0; i < ui->programs->rowCount(); i++)
    {
        if (ui->programs->item(i, PROGRAMS_COL_ID)->text() == idText) {
            return i;
        }
    }
    return -1;
}

// Add row to table
void DABDemodGUI::addProgramName(const DABDemod::MsgDABProgramName& program)
{
    // Don't add duplicate
    if (findProgramRowById(program.getId()) != -1) {
        return;
    }

    ui->programs->setSortingEnabled(false);
    int row = ui->programs->rowCount();
    ui->programs->setRowCount(row + 1);

    QTableWidgetItem *nameItem = new QTableWidgetItem();
    QTableWidgetItem *idItem = new QTableWidgetItem();
    QTableWidgetItem *frequencyItem = new QTableWidgetItem();
    QTableWidgetItem *ensembleItem = new QTableWidgetItem();
    ui->programs->setItem(row, PROGRAMS_COL_NAME, nameItem);
    ui->programs->setItem(row, PROGRAMS_COL_ID, idItem);
    ui->programs->setItem(row, PROGRAMS_COL_FREQUENCY, frequencyItem);
    ui->programs->setItem(row, PROGRAMS_COL_ENSEMBLE, ensembleItem);
    nameItem->setText(program.getName());
    idItem->setText(QString::number(program.getId()));
    double frequencyInHz;
    if (ChannelWebAPIUtils::getCenterFrequency(m_dabDemod->getDeviceSetIndex(), frequencyInHz))
    {
        double frequencyInMHz = (frequencyInHz+m_settings.m_inputFrequencyOffset)/1e6;
        frequencyItem->setText(QString::number(frequencyInMHz, 'f', 3));
        frequencyItem->setData(Qt::UserRole, frequencyInHz+m_settings.m_inputFrequencyOffset);
    }
    else
    {
        frequencyItem->setData(Qt::UserRole, 0.0);
    }
    ensembleItem->setText(ui->ensemble->text());
    filterRow(row);
    ui->programs->setSortingEnabled(true);
}

// Tune to the selected program
void DABDemodGUI::on_programs_cellDoubleClicked(int row, int column)
{
    (void) column;

    m_settings.m_program = ui->programs->item(row, PROGRAMS_COL_NAME)->text();

    double frequencyInHz = ui->programs->item(row, PROGRAMS_COL_FREQUENCY)->data(Qt::UserRole).toDouble();
    double centreFreq = frequencyInHz-m_settings.m_inputFrequencyOffset;
    ChannelWebAPIUtils::setCenterFrequency(m_dabDemod->getDeviceSetIndex(), centreFreq);
    clearProgram();
    applySettings();
}

// Ensemble name can sometimes be decoded after program name, so we
// need to update entries in the table where ensemble name is "-"
void DABDemodGUI::updateEnsembleName(const QString& ensemble)
{
    double frequencyInHz = m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset;
    for (int i = 0; i < ui->programs->rowCount(); i++)
    {
        if (ui->programs->item(i, PROGRAMS_COL_ENSEMBLE)->text() == "-")
        {
            if (ui->programs->item(i, PROGRAMS_COL_FREQUENCY)->data(Qt::UserRole).toDouble() == frequencyInHz)
            {
                ui->programs->item(i, PROGRAMS_COL_ENSEMBLE)->setText(ensemble);
            }
        }
    }
}

bool DABDemodGUI::handleMessage(const Message& message)
{
    if (DABDemod::MsgConfigureDABDemod::match(message))
    {
        qDebug("DABDemodGUI::handleMessage: DABDemod::MsgConfigureDABDemod");
        const DABDemod::MsgConfigureDABDemod& cfg = (DABDemod::MsgConfigureDABDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        if (m_deviceCenterFrequency != notif.getCenterFrequency())
        {
            // Reset on frequency change, to get new ensemble name
            resetService();
        }
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        bool srTooLow = m_basebandSampleRate < 2048000;
        ui->warning->setVisible(srTooLow);

        if (srTooLow) {
            ui->warning->setText("Sample rate must be >= 2048000");
        } else {
            ui->warning->setText("");
        }

        getRollupContents()->arrangeRollups();
        updateAbsoluteCenterFrequency();

        return true;
    }
    else if (DABDemod::MsgDABEnsembleName::match(message))
    {
        DABDemod::MsgDABEnsembleName& report = (DABDemod::MsgDABEnsembleName&) message;
        ui->ensemble->setText(report.getName());
        updateEnsembleName(report.getName());
        return true;
    }
    else if (DABDemod::MsgDABProgramName::match(message))
    {
        DABDemod::MsgDABProgramName& report = (DABDemod::MsgDABProgramName&) message;
        addProgramName(report);
        return true;
    }
    else if (DABDemod::MsgDABProgramData::match(message))
    {
        DABDemod::MsgDABProgramData& report = (DABDemod::MsgDABProgramData&) message;
        ui->program->setText(m_settings.m_program);
        ui->bitrate->setText(QString("%1kbps").arg(report.getBitrate()));
        ui->audio->setText(report.getAudio());
        ui->language->setText(report.getLanguage());
        ui->programType->setText(report.getProgramType());
        return true;
    }
    else if (DABDemod::MsgDABSystemData::match(message))
    {
        DABDemod::MsgDABSystemData& report = (DABDemod::MsgDABSystemData&) message;
        if (report.getSync())
            ui->sync->setText("Yes");
        else
            ui->sync->setText("No");
        ui->snr->setText(QString("%1").arg(report.getSNR()));
        ui->freqOffset->setText(QString("%1").arg(report.getFrequencyOffset()));
        return true;
    }
    else if (DABDemod::MsgDABProgramQuality::match(message))
    {
        DABDemod::MsgDABProgramQuality& report = (DABDemod::MsgDABProgramQuality&) message;
        ui->frames->setText(QString("%1%").arg(report.getFrames()));
        ui->rs->setText(QString("%1%").arg(report.getRS()));
        ui->aac->setText(QString("%1%").arg(report.getAAC()));
        return true;
    }
    else if (DABDemod::MsgDABFIBQuality::match(message))
    {
        DABDemod::MsgDABFIBQuality& report = (DABDemod::MsgDABFIBQuality&) message;
        ui->fib->setText(QString("%1%").arg(report.getPercent()));
        return true;
    }
    else if (DABDemod::MsgDABSampleRate::match(message))
    {
        DABDemod::MsgDABSampleRate& report = (DABDemod::MsgDABSampleRate&) message;
        ui->sampleRate->setText(QString("%1k").arg(report.getSampleRate()/1000.0, 0, 'f', 0));
        return true;
    }
    else if (DABDemod::MsgDABData::match(message))
    {
        DABDemod::MsgDABData& report = (DABDemod::MsgDABData&) message;
        ui->data->setText(report.getData());
        return true;
    }
    else if (DABDemod::MsgDABMOTData::match(message))
    {
        DABDemod::MsgDABMOTData& report = (DABDemod::MsgDABMOTData&) message;
        QString filename = report.getFilename();
        if (filename.endsWith(".png") || filename.endsWith(".PNG") || filename.endsWith(".jpg") || filename.endsWith(".JPG"))
        {
            QPixmap pixmap;
            pixmap.loadFromData(report.getData());
            ui->motImage->resize(ui->motImage->width(), pixmap.height());
            ui->motImage->setVisible(true);
            ui->motImage->setPixmap(pixmap, pixmap.size());
            getRollupContents()->arrangeRollups();
        }
        return true;
    }
    else if (DABDemod::MsgDABTII::match(message))
    {
        DABDemod::MsgDABTII& report = (DABDemod::MsgDABTII&) message;
        int tii = report.getTII();
        ui->tiiMainId->setText(QStringLiteral("%1").arg((tii >> 8) & 0xff, 2, 16, QLatin1Char('0')).toUpper());
        ui->tiiSubId->setText(QStringLiteral("%1").arg(tii & 0xff, 2, 16, QLatin1Char('0')).toUpper());
        return true;
    }
    return false;
}

void DABDemodGUI::handleInputMessages()
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

void DABDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void DABDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void DABDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void DABDemodGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
    applySettings();
}

void DABDemodGUI::on_volume_valueChanged(int value)
{
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_volume = value / 10.0;
    applySettings();
}

void DABDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void DABDemodGUI::on_filter_editingFinished()
{
    m_settings.m_filter = ui->filter->text();
    filter();
    applySettings();
}

void DABDemodGUI::on_clearTable_clicked()
{
    ui->programs->setRowCount(0);
    // Reset the DAB library, so it re-outputs program names
    DABDemod::MsgDABReset* message = DABDemod::MsgDABReset::create();
    m_dabDemod->getInputMessageQueue()->push(message);
}

void DABDemodGUI::filterRow(int row)
{
    bool hidden = false;
    if (m_settings.m_filter != "")
    {
        QRegularExpression re(m_settings.m_filter);
        QTableWidgetItem *fromItem = ui->programs->item(row, PROGRAMS_COL_NAME);
        QRegularExpressionMatch match = re.match(fromItem->text());
        if (!match.hasMatch())
            hidden = true;
    }
    ui->programs->setRowHidden(row, hidden);
}

void DABDemodGUI::filter()
{
    for (int i = 0; i < ui->programs->rowCount(); i++)
    {
        filterRow(i);
    }
}

void DABDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void DABDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
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
            dialog.setNumberOfStreams(m_dabDemod->getNumberOfDeviceStreams());
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
        setTitle(m_channelMarker.getTitle());
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

DABDemodGUI::DABDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::DABDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_doApplySettings(true),
    m_tickCount(0),
    m_channelFreq(0.0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demoddab/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_dabDemod = reinterpret_cast<DABDemod*>(rxChannel);
    m_dabDemod->setMessageQueueToGUI(getInputMessageQueue());

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->warning->setVisible(false);
    ui->warning->setStyleSheet("QLabel { background-color: red; }");

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("Packet Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    ui->motImage->setVisible(false);

    // Resize the table using dummy data
    resizeTable();
    // Allow user to reorder columns
    ui->programs->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->programs->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    menu = new QMenu(ui->programs);
    for (int i = 0; i < ui->programs->horizontalHeader()->count(); i++)
    {
        QString text = ui->programs->horizontalHeaderItem(i)->text();
        menu->addAction(createCheckableItem(text, i, true));
    }
    ui->programs->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->programs->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(columnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->programs->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(programs_sectionMoved(int, int, int)));
    connect(ui->programs->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(programs_sectionResized(int, int, int)));

    displaySettings();
    makeUIConnections();
    applySettings(true);
    m_resizer.enableChildMouseTracking();
}

DABDemodGUI::~DABDemodGUI()
{
    delete ui;
}

void DABDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void DABDemodGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        DABDemod::MsgConfigureDABDemod* message = DABDemod::MsgConfigureDABDemod::create( m_settings, force);
        m_dabDemod->getInputMessageQueue()->push(message);
    }
}

void DABDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->audioMute->setChecked(m_settings.m_audioMute);

    ui->volume->setValue(m_settings.m_volume * 10.0);
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 0, 'f', 1));

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);

    updateIndexLabel();

    ui->filter->setText(m_settings.m_filter);

    // Order and size columns
    QHeaderView *header = ui->programs->horizontalHeader();
    for (int i = 0; i < DABDEMOD_COLUMNS; i++)
    {
        bool hidden = m_settings.m_columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        menu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_columnSizes[i] > 0)
            ui->programs->setColumnWidth(i, m_settings.m_columnSizes[i]);
        header->moveSection(header->visualIndex(i), m_settings.m_columnIndexes[i]);
    }

    filter();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void DABDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void DABDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void DABDemodGUI::clearProgram()
{
    // Clear current program
    ui->program->setText("-");
    ui->programType->setText("-");
    ui->language->setText("-");
    ui->audio->setText("-");
    ui->bitrate->setText("-");
    ui->sampleRate->setText("-");
    ui->data->setText("");
    ui->motImage->setPixmap(QPixmap());
    ui->motImage->setVisible(false);
    getRollupContents()->arrangeRollups();
}

void DABDemodGUI::resetService()
{
    ui->ensemble->setText("-");
    ui->tiiMainId->setText("-");
    ui->tiiSubId->setText("-");
    clearProgram();
    // Reset DAB audio service, so we get new ensemble name
    DABDemod::MsgDABReset* message = DABDemod::MsgDABReset::create();
    m_dabDemod->getInputMessageQueue()->push(message);
}

void DABDemodGUI::on_channel_currentIndexChanged(int index)
{
    (void) index;

    QString text = ui->channel->currentText();
    if (!text.isEmpty())
    {
        // Tune to requested channel
        QString freq = text.split(" ")[2];
        m_channelFreq = freq.toDouble() * 1e6;
        ChannelWebAPIUtils::setCenterFrequency(m_dabDemod->getDeviceSetIndex(), m_channelFreq - m_settings.m_inputFrequencyOffset);
    }
}

void DABDemodGUI::audioSelect(const QPoint& p)
{
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void DABDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_dabDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    // Update channel combobox according to current frequency
    double frequencyInHz;
    if (ChannelWebAPIUtils::getCenterFrequency(m_dabDemod->getDeviceSetIndex(), frequencyInHz))
    {
        frequencyInHz += m_settings.m_inputFrequencyOffset;
        double frequencyInkHz = std::round(frequencyInHz / 1e3);
        double frequencyInMHz = frequencyInkHz / 1000.0;

        if (m_channelFreq != frequencyInMHz * 1e6)
        {
            QString freqText = QString::number(frequencyInMHz, 'f', 3);
            int i;
            for (i = 0; i < ui->channel->count(); i++)
            {
                if (ui->channel->itemText(i).contains(freqText))
                {
                    ui->channel->blockSignals(true);
                    ui->channel->setCurrentIndex(i);
                    ui->channel->blockSignals(false);
                    break;
                }
            }
            if (i == ui->channel->count())
            {
                ui->channel->setCurrentIndex(-1);
                m_channelFreq = -1.0;
            }
        }
    }

    m_tickCount++;
}

void DABDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &DABDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &DABDemodGUI::on_audioMute_toggled);
    QObject::connect(ui->volume, &QSlider::valueChanged, this, &DABDemodGUI::on_volume_valueChanged);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &DABDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->filter, &QLineEdit::editingFinished, this, &DABDemodGUI::on_filter_editingFinished);
    QObject::connect(ui->clearTable, &QPushButton::clicked, this, &DABDemodGUI::on_clearTable_clicked);
    QObject::connect(ui->programs, &QTableWidget::cellDoubleClicked, this, &DABDemodGUI::on_programs_cellDoubleClicked);
    QObject::connect(ui->channel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DABDemodGUI::on_channel_currentIndexChanged);
    QObject::connect(ui->findOnMap, &QToolButton::clicked, this, &DABDemodGUI::on_findOnMap_clicked);
}

void DABDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

void DABDemodGUI::on_findOnMap_clicked()
{
    QString mainID = ui->tiiMainId->text();
    if (mainID.isEmpty() || (mainID == "-")) {
        return;
    }
    QString subID = ui->tiiSubId->text();
    if (subID.isEmpty() || (subID == "-")) {
        return;
    }
    QString ensemble = ui->ensemble->text().trimmed();
    if (ensemble.isEmpty() || (ensemble == "-")) {
        return;
    }
    QString id = ensemble + " " + mainID + subID;
    qDebug() << "Finding " << id;
    FeatureWebAPIUtils::mapFind(id);
}

