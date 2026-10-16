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

#include <algorithm>
#include <cmath>
#include <limits>
#include <ctype.h>

#include <QDebug>
#include <QAction>
#include <QDateTime>
#include <QFontMetrics>
#include <QHeaderView>
#include <QRegularExpression>
#include <QScrollBar>
#include <QUrl>

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
#include "modemdab/dabtables.h"
#include "gui/journalinebrowser.h"
#include "gui/programmeguide.h"
#include "gui/websitebrowser.h"

// Table column indexes
#define PROGRAMS_COL_NAME            0
#define PROGRAMS_COL_ID              1
#define PROGRAMS_COL_FREQUENCY       2
#define PROGRAMS_COL_ENSEMBLE        3

// Announcement types followed when the button is on: all but alarms, which always are
static const int AllAnnouncementTypes = 0x07fe;

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
    applySettings(QStringList(), true);
}

QByteArray DABDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool DABDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applySettings(QStringList(), true);
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
    ensembleItem->setText(info(InfoEnsemble));
    filterRow(row);
    ui->programs->setSortingEnabled(true);
}

// Tune to the selected program
void DABDemodGUI::on_programs_cellDoubleClicked(int row, int column)
{
    (void) column;

    const QString program = ui->programs->item(row, PROGRAMS_COL_NAME)->text();
    double frequencyInHz = ui->programs->item(row, PROGRAMS_COL_FREQUENCY)->data(Qt::UserRole).toDouble();
    double centreFreq = frequencyInHz-m_settings.m_inputFrequencyOffset;

    // The program playing: its details would be cleared, and with nothing changed, not sent again
    if ((program == m_settings.m_program) && (std::llround(centreFreq) == m_deviceCenterFrequency)) {
        return;
    }

    m_settings.m_program = program;
    ChannelWebAPIUtils::setCenterFrequency(m_dabDemod->getDeviceSetIndex(), centreFreq);
    clearProgram();
    applySettings(QStringList({"program"}));
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
        // Another program, from the WebAPI or MCP: the details shown are the previous one's
        if (cfg.getSettings().m_program != m_settings.m_program) {
            clearProgram();
        }
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
        setInfo(InfoEnsemble, report.getName());
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
        setInfo(InfoProgram, m_settings.m_program);
        setInfo(InfoBitrate, QString("%1kbps").arg(report.getBitrate()));
        setInfo(InfoAudio, report.getAudio());
        setInfo(InfoLanguage, report.getLanguage());
        setInfo(InfoProgramType, report.getProgramType());
        return true;
    }
    else if (DABDemod::MsgDABSystemData::match(message))
    {
        DABDemod::MsgDABSystemData& report = (DABDemod::MsgDABSystemData&) message;
        if (report.getSync())
            setInfo(InfoSync, "Yes");
        else
            setInfo(InfoSync, "No");
        setInfo(InfoSNR, QString("%1 dB").arg(report.getSNR(), 0, 'f', 1));
        setInfo(InfoFrequencyOffset, QString("%1 Hz").arg(report.getFrequencyOffset()));
        return true;
    }
    else if (DABDemod::MsgDABProgramQuality::match(message))
    {
        DABDemod::MsgDABProgramQuality& report = (DABDemod::MsgDABProgramQuality&) message;
        // RS and AAC are -1 for DAB (MP2) programs
        const auto percent = [](int value) { return value < 0 ? QString("-") : QString("%1%").arg(value); };
        setInfo(InfoFrames, percent(report.getFrames()));
        setInfo(InfoRS, percent(report.getRS()));
        setInfo(InfoAAC, percent(report.getAAC()));
        return true;
    }
    else if (DABDemod::MsgDABSignalQuality::match(message))
    {
        const DABDemod::SignalQuality& quality = ((DABDemod::MsgDABSignalQuality&) message).getQuality();
        if (quality.measured)
        {
            setInfo(InfoCN, QString("%1 dB").arg(quality.cn, 0, 'f', 1));
            setInfo(InfoNullDepth, QString("%1 dB").arg(quality.nullDepth, 0, 'f', 1));
            setInfo(InfoMSCBER, QString("%1%").arg(100.0 * quality.mscBER, 0, 'f', 2));
            setInfo(InfoFICBER, QString("%1%").arg(100.0 * quality.ficBER, 0, 'f', 2));
            setInfo(InfoRSCorrected, QString("%1 B/s").arg(quality.rsCorrectedRate, 0, 'f', 0));
            setInfo(InfoLost, QString("%1 / %2").arg(quality.framesLost).arg(quality.reacquisitions));
        }
        setInfo(InfoClipping, QString("%1%").arg(quality.clipping, 0, 'f', 2));
        setInfo(InfoGain, quality.gainAdvice);
        switch (quality.gainStatus)
        {
        case DABDemod::GainReduce:
            setInfoColor(InfoGain, QColor(255, 64, 64));
            break;
        case DABDemod::GainIncrease:
        case DABDemod::GainCheck:
            setInfoColor(InfoGain, QColor(255, 165, 0));
            break;
        default:
            setInfoColor(InfoGain, QColor());
            break;
        }
        return true;
    }
    else if (DABDemod::MsgDABCarrierCN::match(message))
    {
        const DABDemod::MsgDABCarrierCN& report = (const DABDemod::MsgDABCarrierCN&) message;
        ui->carrierPlot->setCarrierCN(report.getCarrierCN(), report.getMedian());
        return true;
    }
    else if (DABDemod::MsgDABFIBQuality::match(message))
    {
        DABDemod::MsgDABFIBQuality& report = (DABDemod::MsgDABFIBQuality&) message;
        setInfo(InfoFIB, QString("%1%").arg(report.getPercent()));
        return true;
    }
    else if (DABDemod::MsgDABSampleRate::match(message))
    {
        DABDemod::MsgDABSampleRate& report = (DABDemod::MsgDABSampleRate&) message;
        setInfo(InfoSampleRate, QString("%1k").arg(report.getSampleRate()/1000.0, 0, 'f', 0));
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
        // MOT image content subtypes: 1 JFIF (JPEG), 3 PNG
        const int subType = report.getContentSubType();
        if (filename.endsWith(".png") || filename.endsWith(".PNG") || filename.endsWith(".jpg") || filename.endsWith(".JPG")
            || (subType == 1) || (subType == 3))
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
    else if (DABDemod::MsgDABDLPlus::match(message))
    {
        showDLPlus((DABDemod::MsgDABDLPlus&) message);
        return true;
    }
    else if (DABDemod::MsgDABWebsite::match(message))
    {
        DABDemod::MsgDABWebsite& report = (DABDemod::MsgDABWebsite&) message;
        const auto website = report.getWebsite();
        if (website && !website->files.empty()) {
            ui->websiteStatus->setText(tr("%1 of %2 files").arg(website->files.size()).arg(website->directoryObjects));
        } else if (report.getPresent()) {
            ui->websiteStatus->setText(tr("Waiting for the website..."));
        } else {
            ui->websiteStatus->setText(tr("No Broadcast Website"));
        }
        ui->website->setWebsite(website);
        return true;
    }
    else if (DABDemod::MsgDABJournaline::match(message))
    {
        DABDemod::MsgDABJournaline& report = (DABDemod::MsgDABJournaline&) message;
        ui->journaline->setObjects(report.getObjects());
        return true;
    }
    else if (DABDemod::MsgDABServiceInfo::match(message))
    {
        DABDemod::MsgDABServiceInfo& report = (DABDemod::MsgDABServiceInfo&) message;
        m_serviceInfo = report.getInfo();
        showServiceInfo();
        return true;
    }
    else if (DABDemod::MsgDABSPI::match(message))
    {
        DABDemod::MsgDABSPI& report = (DABDemod::MsgDABSPI&) message;
        m_spi = report.getData();
        showSPI();
        updateProgramLogos();
        return true;
    }
    else if (DABDemod::MsgDABTII::match(message))
    {
        DABDemod::MsgDABTII& report = (DABDemod::MsgDABTII&) message;
        int tii = report.getTII();
        if (tii == DABDemod::TIINone)
        {
            // Synchronized for a few seconds, and the ensemble sends no TII
            setInfo(InfoTIIMainId, "None");
            setInfo(InfoTIISubId, "None");
        }
        else
        {
            setInfo(InfoTIIMainId, QStringLiteral("%1").arg((tii >> 8) & 0xff, 2, 16, QLatin1Char('0')).toUpper());
            setInfo(InfoTIISubId, QStringLiteral("%1").arg(tii & 0xff, 2, 16, QLatin1Char('0')).toUpper());
        }
        return true;
    }
    return false;
}

void DABDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }
        delete message;
    }
}

void DABDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings(QStringList({"inputFrequencyOffset"}));
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
    applySettings(QStringList({"inputFrequencyOffset"}));
}

void DABDemodGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
    applySettings(QStringList({"audioMute"}));
}

void DABDemodGUI::on_volume_valueChanged(int value)
{
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_volume = value / 10.0;
    applySettings(QStringList({"volume"}));
}

void DABDemodGUI::on_coherent_toggled(bool checked)
{
    m_settings.m_coherent = checked;
    applySettings(QStringList({"coherent"}));
}

void DABDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings(QStringList({"rfBandwidth"}));
}

void DABDemodGUI::on_filter_editingFinished()
{
    m_settings.m_filter = ui->filter->text();
    filter();
    applySettings(QStringList({"filter"}));
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
    applySettings(QStringList());
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

        applySettings(QStringList({"rgbColor", "title", "useReverseAPI", "reverseAPIAddress",
                                   "reverseAPIPort", "reverseAPIDeviceIndex", "reverseAPIChannelIndex",
                                   "streamIndex"}));
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
    m_audioPlaying(false),
    m_channelFreq(0.0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demoddab/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setupInfoTables();
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_dabDemod = reinterpret_cast<DABDemod*>(rxChannel);
    m_dabDemod->setMessageQueueToGUI(getInputMessageQueue());

    ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->warning->setVisible(false);
    ui->warning->setStyleSheet("QLabel { background-color: red; }");

    ui->links->horizontalHeader()->setStretchLastSection(true);
    ui->links->verticalHeader()->setVisible(false);

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
    applySettings(QStringList(), true);
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

void DABDemodGUI::applySettings(const QStringList& channelSettingsKeys, bool force)
{
    if (m_doApplySettings)
    {
        DABDemod::MsgConfigureDABDemod* message = DABDemod::MsgConfigureDABDemod::create(channelSettingsKeys, m_settings, force);
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

    ui->coherent->setChecked(m_settings.m_coherent);
    // Without its toggled slot, which would choose every type
    ui->announcements->blockSignals(true);
    ui->announcements->setChecked(m_settings.m_announcementTypes != 0);
    ui->announcements->blockSignals(false);

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
    setInfo(InfoProgram, "-");
    setInfo(InfoProgramType, "-");
    setInfo(InfoLanguage, "-");
    setInfo(InfoAudio, "-");
    setInfo(InfoBitrate, "-");
    setInfo(InfoSampleRate, "-");
    setInfo(InfoFrames, "-");
    setInfo(InfoRS, "-");
    setInfo(InfoAAC, "-");
    ui->data->setText("");
    ui->motImage->setPixmap(QPixmap());
    ui->motImage->setVisible(false);
    setInfo(InfoNowPlaying, "-");
    setInfo(InfoAnnouncement, "-");
    setInfoBold(InfoAnnouncement, false);
    setInfo(InfoOnNow, "-");
    ui->programLogo->clear();
    ui->programLogo->setVisible(false);
    m_serviceInfo = DABDemod::ServiceInfo();
    showLinks();
    ui->programmeGuideView->setService(0);
    ui->journaline->setObjects(nullptr, true);
    ui->website->setWebsite(nullptr);
    ui->websiteStatus->setText(tr("No Broadcast Website"));
    getRollupContents()->arrangeRollups();
}

void DABDemodGUI::clearSignalQuality()
{
    setInfo(InfoCN, "-");
    setInfo(InfoNullDepth, "-");
    setInfo(InfoMSCBER, "-");
    setInfo(InfoFICBER, "-");
    setInfo(InfoRSCorrected, "-");
    setInfo(InfoLost, "-");
    setInfo(InfoClipping, "-");
    setInfo(InfoGain, "-");
    setInfoColor(InfoGain, QColor());
    ui->carrierPlot->clear();
}

void DABDemodGUI::resetService()
{
    setInfo(InfoEnsemble, "-");
    setInfo(InfoTIIMainId, "-");
    setInfo(InfoTIISubId, "-");
    m_spi.reset();
    ui->programmeGuideView->setData(nullptr);
    clearProgram();
    clearSignalQuality();
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
        applySettings(QStringList({"audioDeviceName"}));
    }
}

// The rows of the Current Program, Statistics and Transmitter tables: a name and a value, rows
// alternating in colour, as the DRM Demodulator's status
namespace
{
struct InfoRow
{
    int table;          // 0 Current Program, 1 Statistics, 2 Transmitter
    const char *name;
    const char *toolTip;
};

const InfoRow infoRows[] = {
    {0, "Program", "The program playing"},
    {0, "Ensemble", "The ensemble (multiplex) tuned to"},
    {0, "Program type", "Program type (e.g. News, Pop)"},
    {0, "Language", "Language of the program"},
    {0, "Audio", "DAB (MPEG Layer II) or DAB+ (HE-AAC)"},
    {0, "Bitrate", "Audio bitrate"},
    {0, "Sample rate", "Audio sample rate (in Sa/s)"},
    {0, "Announcement", "Announcement (traffic, news, alarm...) in progress for this service, and the service it comes from"},
    {0, "Now playing", "Artist and title, from the DL Plus tags of the dynamic label"},
    {0, "On now", "The programme on now, from the Service and Programme Information (SPI)"},
    {1, "Sync", "Time synchronized"},
    {1, "Freq offset", "Frequency offset in Hz"},
    {1, "SNR", "Signal to noise ratio of the demodulated symbols (their MER) in dB. It includes every impairment: noise, interference, "
        "multipath, receiver overload and phase noise."},
    {1, "C/N", "Carrier to noise ratio of the median carrier, from the null symbol. It sees noise and interference, but not overload "
        "of the receiver by the ensemble itself, nor multipath. SNR well below C/N points to those."},
    {1, "Null", "Power in the null symbol relative to the rest of the frame"},
    {1, "Clipping", "Percentage of input samples at or near full scale"},
    {1, "FIB", "Percent of FIB packages with valid CRC"},
    {1, "FIC BER", "Bit error rate of the FIC (service information) before error correction"},
    {1, "Frames", "Percentage of valid audio frames: for DAB+, superframes whose Fire code checks; for DAB, frames whose CRC checks"},
    {1, "MSC BER", "Bit error rate of the selected service before error correction. Audio errors start at about 10%: the lower, the more margin."},
    {1, "RS", "For DAB+, percentage of Reed Solomon codewords corrected"},
    {1, "RS fixed", "For DAB+, bytes per second corrected by Reed Solomon: rising before audio errors start"},
    {1, "AAC", "For DAB+, percentage of AAC access units whose CRC checks"},
    {1, "Lost", "Frames lost / times synchronization was acquired again, since tuning"},
    {1, "Gain", "Advice on the receiver gain, from the input level, clipping, and SNR against C/N"},
    {2, "TII main ID", "Transmitter Identification Information: main identifier"},
    {2, "TII sub ID", "Transmitter Identification Information: sub identifier"},
};

// Name and value pairs side by side in each row of each table: the statistics are short
const int infoPairs[] = {1, 2, 1};

// The position of a field among its table's
int infoIndex(int field)
{
    int index = 0;
    for (int i = 0; i < field; i++)
    {
        if (infoRows[i].table == infoRows[field].table) {
            index++;
        }
    }
    return index;
}
}

void DABDemodGUI::setupInfoTables()
{
    static_assert(sizeof(infoRows) / sizeof(infoRows[0]) == InfoFieldCount, "a row for each InfoField");
    QTableWidget *tables[] = {ui->programInfo, ui->statisticsInfo, ui->transmitterInfo};

    for (int t = 0; t < 3; t++)
    {
        QTableWidget *table = tables[t];
        const QFontMetrics metrics(table->font());
        const int rowHeight = metrics.height() + 4;
        int rows = 0;
        int nameWidth = 0;

        for (const InfoRow& row : infoRows)
        {
            if (row.table == t)
            {
                rows++;
                nameWidth = std::max(nameWidth, metrics.horizontalAdvance(row.name));
            }
        }
        const int pairs = infoPairs[t];
        const int fields = rows;
        rows = (fields + pairs - 1) / pairs;
        table->setColumnCount(2 * pairs);
        table->setRowCount(rows);
        table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        table->verticalHeader()->setDefaultSectionSize(rowHeight);
        // Names their width, values sharing the rest
        for (int pair = 0; pair < pairs; pair++)
        {
            table->horizontalHeader()->setSectionResizeMode(2 * pair, QHeaderView::Fixed);
            table->setColumnWidth(2 * pair, nameWidth + 12);
            table->horizontalHeader()->setSectionResizeMode(2 * pair + 1, QHeaderView::Stretch);
        }
        // A last field on its own: its value across the row (the gain advice is long)
        if (fields % pairs) {
            table->setSpan(rows - 1, 2 * (fields % pairs) - 1, 1, 2 * (pairs - fields % pairs) + 1);
        }
        // Every row shown without scrolling
        table->setMinimumHeight(rows * rowHeight + 2 * table->frameWidth());
        table->setMaximumHeight(rows * rowHeight + 2 * table->frameWidth());
    }

    for (int field = 0; field < InfoFieldCount; field++)
    {
        QTableWidget *table = tables[infoRows[field].table];
        const int pairs = infoPairs[infoRows[field].table];
        const int index = infoIndex(field);
        table->setItem(index / pairs, 2 * (index % pairs), new QTableWidgetItem(infoRows[field].name));
        table->setItem(index / pairs, 2 * (index % pairs) + 1, new QTableWidgetItem("-"));
        setInfoToolTip((InfoField) field, infoRows[field].toolTip);
    }
}

QTableWidgetItem *DABDemodGUI::infoItem(InfoField field, int column) const
{
    QTableWidget *tables[] = {ui->programInfo, ui->statisticsInfo, ui->transmitterInfo};
    const int pairs = infoPairs[infoRows[field].table];
    const int index = infoIndex(field);
    return tables[infoRows[field].table]->item(index / pairs, 2 * (index % pairs) + column);
}

void DABDemodGUI::setInfo(InfoField field, const QString& value)
{
    QTableWidgetItem *item = infoItem(field);
    if (item->text() != value) {
        item->setText(value);
    }
}

QString DABDemodGUI::info(InfoField field) const
{
    return infoItem(field)->text();
}

void DABDemodGUI::setInfoToolTip(InfoField field, const QString& toolTip)
{
    for (int column = 0; column < 2; column++) {
        infoItem(field, column)->setToolTip(toolTip);
    }
}

void DABDemodGUI::setInfoColor(InfoField field, const QColor& color)
{
    if (color.isValid()) {
        infoItem(field)->setForeground(color);
    } else {
        infoItem(field)->setData(Qt::ForegroundRole, QVariant());
    }
}

void DABDemodGUI::setInfoBold(InfoField field, bool bold)
{
    QFont font = infoItem(field)->font();
    font.setBold(bold);
    infoItem(field)->setFont(font);
}

// Artist and title from the DL Plus tags, with all the tags in the tooltip
void DABDemodGUI::showDLPlus(const DABDemod::MsgDABDLPlus& report)
{
    QString artist;
    QString title;
    QStringList tags;

    for (const auto& tag : report.getTags())
    {
        if (tag.first == "ITEM.ARTIST") {
            artist = tag.second;
        } else if (tag.first == "ITEM.TITLE") {
            title = tag.second;
        }
        tags.append(QString("%1: %2").arg(tag.first, tag.second));
    }

    QString nowPlaying;
    if (report.getItemRunning())
    {
        if (!artist.isEmpty() && !title.isEmpty()) {
            nowPlaying = QString("%1 - %2").arg(artist, title);
        } else {
            nowPlaying = artist.isEmpty() ? title : artist;
        }
    }
    setInfo(InfoNowPlaying, nowPlaying.isEmpty() ? "-" : nowPlaying);
    setInfoToolTip(InfoNowPlaying, tags.isEmpty() ? "Artist and title, from the DL Plus tags of the dynamic label" : tags.join("\n"));
}

void DABDemodGUI::showServiceInfo()
{
    const DABDemod::ServiceInfo& info = m_serviceInfo;

    if (info.announcement.isEmpty())
    {
        setInfo(InfoAnnouncement, "-");
        setInfoBold(InfoAnnouncement, false);
    }
    else
    {
        QString text = info.announcement;
        if (info.announcementSwitched && !info.announcementService.isEmpty()) {
            text += QString(" (%1)").arg(info.announcementService);
        }
        setInfo(InfoAnnouncement, text);
        setInfoBold(InfoAnnouncement, true);
    }

    setInfo(InfoOnNow, info.programmeNow.isEmpty() ? "-" : info.programmeNow);
    setInfoToolTip(InfoOnNow, info.programmeNowDescription.isEmpty()
        ? "The programme on now, from the Service and Programme Information (SPI)" : info.programmeNowDescription);

    showLinks();
    showSPI();
}

void DABDemodGUI::showLinks()
{
    const auto hex = [](quint32 id, int digits) { return QString("%1").arg(id, digits, 16, QLatin1Char('0')).toUpper(); };

    ui->links->setSortingEnabled(false);
    ui->links->setRowCount(0);

    for (const modemdab::DABServiceLink& link : m_serviceInfo.links)
    {
        // Only where it can be found: a frequency, an ensemble, or a service of this one
        if (link.frequencies.empty() && link.eids.empty() && link.label.empty()) {
            continue;
        }

        QString type;
        QString service;
        QStringList frequencies;

        switch (link.kind)
        {
        case modemdab::DABServiceLink::SameService:
        case modemdab::DABServiceLink::LinkedDAB:
            type = "DAB";
            service = link.label.empty() ? hex(link.id, link.id > 0xffff ? 8 : 4) : QString::fromStdString(link.label);
            for (int64_t f : link.frequencies) {
                frequencies.append(QString("%1 MHz").arg(f / 1e6, 0, 'f', 3));
            }
            break;
        case modemdab::DABServiceLink::LinkedFM:
            type = "FM";
            service = QString("PI %1").arg(hex(link.id, 4));
            for (int64_t f : link.frequencies) {
                frequencies.append(QString("%1 MHz").arg(f / 1e6, 0, 'f', 1));
            }
            break;
        case modemdab::DABServiceLink::LinkedDRM:
            type = "DRM";
            service = hex(link.id, 6);
            for (int64_t f : link.frequencies) {
                frequencies.append(QString("%1 kHz").arg(f / 1e3, 0, 'f', 0));
            }
            break;
        default:
            continue;
        }

        QStringList ensembles;
        for (int eid : link.eids) {
            ensembles.append(hex(eid, 4));
        }

        QString kind = link.kind == modemdab::DABServiceLink::SameService ? "Same service" : (link.hard ? "Same programme" : "Related");
        if (!link.active) {
            kind += " (inactive)";
        }

        const int row = ui->links->rowCount();
        ui->links->setRowCount(row + 1);
        ui->links->setItem(row, 0, new QTableWidgetItem(type));
        ui->links->setItem(row, 1, new QTableWidgetItem(service));
        ui->links->setItem(row, 2, new QTableWidgetItem(ensembles.isEmpty() ? (type == "DAB" ? "This" : "-") : ensembles.join(", ")));
        QTableWidgetItem *frequencyItem = new QTableWidgetItem(frequencies.isEmpty() ? "-" : frequencies.join(", "));
        // DAB: the first frequency, to tune to
        frequencyItem->setData(Qt::UserRole, (type == "DAB") && !link.frequencies.empty() ? (double) link.frequencies[0] : 0.0);
        ui->links->setItem(row, 3, frequencyItem);
        ui->links->setItem(row, 4, new QTableWidgetItem(kind));
    }
    ui->links->resizeColumnsToContents();
}

// Tune to another ensemble carrying the program
void DABDemodGUI::on_links_cellDoubleClicked(int row, int column)
{
    (void) column;

    const double frequencyInHz = ui->links->item(row, 3)->data(Qt::UserRole).toDouble();
    if (frequencyInHz <= 0.0) {
        return;
    }
    // Already tuned there: the details shown are the program's
    if (std::llround(frequencyInHz - m_settings.m_inputFrequencyOffset) == m_deviceCenterFrequency) {
        return;
    }
    ChannelWebAPIUtils::setCenterFrequency(m_dabDemod->getDeviceSetIndex(), frequencyInHz - m_settings.m_inputFrequencyOffset);
    clearProgram();
    applySettings(QStringList({"program"}));
}

// The selected program's logo, description and schedule
void DABDemodGUI::showSPI()
{
    const quint32 sid = m_serviceInfo.sid;
    const QPixmap pixmap = ProgrammeGuide::logo(m_spi.get(), sid, 128);

    if (pixmap.isNull()) {
        ui->programLogo->clear();
    } else {
        ui->programLogo->setPixmap(pixmap.scaled(ui->programLogo->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    ui->programLogo->setVisible(!pixmap.isNull());
    ui->programmeGuideView->setData(m_spi);
    ui->programmeGuideView->setService(sid);
}

// Logos, from the SPI, beside the programs of the ensemble tuned to
void DABDemodGUI::updateProgramLogos()
{
    if (!m_spi) {
        return;
    }
    const double frequencyInHz = m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset;

    for (int row = 0; row < ui->programs->rowCount(); row++)
    {
        QTableWidgetItem *nameItem = ui->programs->item(row, PROGRAMS_COL_NAME);
        if (ui->programs->item(row, PROGRAMS_COL_FREQUENCY)->data(Qt::UserRole).toDouble() != frequencyInHz) {
            continue;
        }
        const QPixmap pixmap = ProgrammeGuide::logo(m_spi.get(), ui->programs->item(row, PROGRAMS_COL_ID)->text().toUInt(), 32);
        if (!pixmap.isNull()) {
            nameItem->setIcon(QIcon(pixmap));
        }
    }
}

void DABDemodGUI::updateJournalineButtons()
{
    ui->journalineBack->setEnabled(ui->journaline->canGoBack());
    ui->journalineHome->setEnabled(!ui->journaline->atHome());
}

void DABDemodGUI::on_announcements_toggled(bool checked)
{
    // All types, unless some were chosen
    m_settings.m_announcementTypes = checked ? AllAnnouncementTypes : 0;
    applySettings(QStringList({"announcementTypes"}));
}

// Choose the announcement types to follow
void DABDemodGUI::announcementsMenu(const QPoint& p)
{
    QMenu menu(this);
    for (int type = 1; type < modemdab::DABAnnouncementTypes; type++)
    {
        QAction *action = menu.addAction(QString::fromStdString(modemdab::DABTables::announcementTypeName(type)));
        action->setCheckable(true);
        action->setChecked((m_settings.m_announcementTypes >> type) & 1);
        action->setData(type);
    }
    QAction *chosen = menu.exec(ui->announcements->mapToGlobal(p));
    if (!chosen) {
        return;
    }
    m_settings.m_announcementTypes ^= 1 << chosen->data().toInt();
    ui->announcements->blockSignals(true);
    ui->announcements->setChecked(m_settings.m_announcementTypes != 0);
    ui->announcements->blockSignals(false);
    applySettings(QStringList({"announcementTypes"}));
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

    // The mute button is green while audio is received and heard
    const bool playing = m_dabDemod->isAudioActive() && !m_settings.m_audioMute;
    if (playing != m_audioPlaying)
    {
        ui->audioMute->setStyleSheet(playing
            ? "QToolButton { background-color : green; }"
            : "QToolButton { background:rgb(79,79,79); }");
        m_audioPlaying = playing;
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
    QObject::connect(ui->volume, &QDial::valueChanged, this, &DABDemodGUI::on_volume_valueChanged);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &DABDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->coherent, &ButtonSwitch::toggled, this, &DABDemodGUI::on_coherent_toggled);
    QObject::connect(ui->filter, &QLineEdit::editingFinished, this, &DABDemodGUI::on_filter_editingFinished);
    QObject::connect(ui->clearTable, &QToolButton::clicked, this, &DABDemodGUI::on_clearTable_clicked);
    QObject::connect(ui->programs, &QTableWidget::cellDoubleClicked, this, &DABDemodGUI::on_programs_cellDoubleClicked);
    QObject::connect(ui->channel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DABDemodGUI::on_channel_currentIndexChanged);
    QObject::connect(ui->findOnMap, &QToolButton::clicked, this, &DABDemodGUI::on_findOnMap_clicked);
    QObject::connect(ui->announcements, &ButtonSwitch::toggled, this, &DABDemodGUI::on_announcements_toggled);
    QObject::connect(ui->announcements, &QWidget::customContextMenuRequested, this, &DABDemodGUI::announcementsMenu);
    QObject::connect(ui->links, &QTableWidget::cellDoubleClicked, this, &DABDemodGUI::on_links_cellDoubleClicked);
    QObject::connect(ui->journaline, &JournalineBrowser::navigationChanged, this, &DABDemodGUI::updateJournalineButtons);
    QObject::connect(ui->journalineHome, &QToolButton::clicked, ui->journaline, &JournalineBrowser::goHome);
    QObject::connect(ui->journalineBack, &QToolButton::clicked, ui->journaline, &JournalineBrowser::goBack);
    QObject::connect(ui->websiteHome, &QToolButton::clicked, ui->website, &WebsiteBrowser::showStartPage);
    QObject::connect(ui->websiteBack, &QToolButton::clicked, ui->website, &WebsiteBrowser::backward);
    QObject::connect(ui->website, &WebsiteBrowser::backwardAvailable, ui->websiteBack, &QToolButton::setEnabled);
    ui->websiteBack->setEnabled(false);
    updateJournalineButtons();
}

void DABDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

void DABDemodGUI::on_findOnMap_clicked()
{
    QString mainID = info(InfoTIIMainId);
    if (mainID.isEmpty() || (mainID == "-") || (mainID == "None")) {
        return;
    }
    QString subID = info(InfoTIISubId);
    if (subID.isEmpty() || (subID == "-")) {
        return;
    }
    QString ensemble = info(InfoEnsemble).trimmed();
    if (ensemble.isEmpty() || (ensemble == "-")) {
        return;
    }
    QString id = ensemble + " " + mainID + subID;
    qDebug() << "Finding " << id;
    FeatureWebAPIUtils::mapFind(id);
}
