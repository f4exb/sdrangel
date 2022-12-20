///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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
#include <QMessageBox>
#include <QAction>
#include <QClipboard>

#include "noisefiguregui.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_noisefiguregui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/crightclickenabler.h"
#include "gui/decimaldelegate.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "noisefigure.h"
#include "noisefiguresink.h"
#include "noisefigurecontroldialog.h"
#include "noisefigureenrdialog.h"

void NoiseFigureGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    int row = ui->results->rowCount();
    ui->results->setRowCount(row + 1);
    ui->results->setItem(row, RESULTS_COL_SETTING, new QTableWidgetItem("2000.000"));
    ui->results->setItem(row, RESULTS_COL_NF, new QTableWidgetItem("10.00"));
    ui->results->setItem(row, RESULTS_COL_TEMP, new QTableWidgetItem("10000"));
    ui->results->setItem(row, RESULTS_COL_Y, new QTableWidgetItem("10.00"));
    ui->results->setItem(row, RESULTS_COL_ENR, new QTableWidgetItem("10.00"));
    ui->results->setItem(row, RESULTS_COL_FLOOR, new QTableWidgetItem("-174.00"));
    ui->results->resizeColumnsToContents();
    ui->results->removeRow(row);
}

void NoiseFigureGUI::measurementReceived(NoiseFigure::MsgNFMeasurement& report)
{
    if (m_settings.m_setting == "centerFrequency") {
        ui->results->horizontalHeaderItem(0)->setText("Freq (MHz)");
    } else {
        ui->results->horizontalHeaderItem(0)->setText(m_settings.m_setting);
    }

    ui->results->setSortingEnabled(false);
    int row = ui->results->rowCount();
    ui->results->setRowCount(row + 1);

    QTableWidgetItem *sweepItem = new QTableWidgetItem();
    QTableWidgetItem *nfItem = new QTableWidgetItem();
    QTableWidgetItem *tempItem = new QTableWidgetItem();
    QTableWidgetItem *yItem = new QTableWidgetItem();
    QTableWidgetItem *enrItem = new QTableWidgetItem();
    QTableWidgetItem *floorItem = new QTableWidgetItem();
    ui->results->setItem(row, RESULTS_COL_SETTING, sweepItem);
    ui->results->setItem(row, RESULTS_COL_NF, nfItem);
    ui->results->setItem(row, RESULTS_COL_TEMP, tempItem);
    ui->results->setItem(row, RESULTS_COL_Y, yItem);
    ui->results->setItem(row, RESULTS_COL_ENR, enrItem);
    ui->results->setItem(row, RESULTS_COL_FLOOR, floorItem);

    sweepItem->setData(Qt::DisplayRole, report.getSweepValue());
    nfItem->setData(Qt::DisplayRole, report.getNF());
    tempItem->setData(Qt::DisplayRole, report.getTemp());
    yItem->setData(Qt::DisplayRole, report.getY());
    enrItem->setData(Qt::DisplayRole, report.getENR());
    floorItem->setData(Qt::DisplayRole, report.getFloor());

    ui->results->setSortingEnabled(true);
    plotChart();
}

void NoiseFigureGUI::plotChart()
{
    QChart *oldChart = m_chart;

    m_chart = new QChart();

    m_chart->layout()->setContentsMargins(0, 0, 0, 0);
    m_chart->setMargins(QMargins(1, 1, 1, 1));
    m_chart->setTheme(QChart::ChartThemeDark);

    // Create reference data series
    QLineSeries *ref = nullptr;
    if ((m_refData.size() > 0) && (ui->chartSelect->currentIndex() < m_refCols-1)) {
        ref = new QLineSeries();
        for (int i = 0; i < m_refData.size() / m_refCols; i++) {
            ref->append(m_refData[i*m_refCols], m_refData[i*m_refCols+ui->chartSelect->currentIndex()+1]);
        }
        QFileInfo fi(m_refFilename);
        ref->setName(fi.completeBaseName());
    } else {
        m_chart->legend()->hide();
    }

    // Create measurement data series
    QLineSeries *series = new QLineSeries();
    series->setName("Measurement");
    for (int i = 0; i < ui->results->rowCount(); i++)
    {
        double sweepValue = ui->results->item(i, RESULTS_COL_SETTING)->data(Qt::DisplayRole).toDouble();
        double val = ui->results->item(i, ui->chartSelect->currentIndex() + RESULTS_COL_NF)->data(Qt::DisplayRole).toDouble();

        series->append(sweepValue, val);
    }

    QValueAxis *xAxis = new QValueAxis();
    QValueAxis *yAxis = new QValueAxis();

    m_chart->addAxis(xAxis, Qt::AlignBottom);
    m_chart->addAxis(yAxis, Qt::AlignLeft);

    if (m_settings.m_setting == "centerFrequency") {
        xAxis->setTitleText("Frequency (MHz)");
    } else {
        xAxis->setTitleText(m_settings.m_setting);
    }
    yAxis->setTitleText(ui->chartSelect->currentText());

    m_chart->addSeries(series);
    series->attachAxis(xAxis);
    series->attachAxis(yAxis);

    if (ref)
    {
        m_chart->addSeries(ref);
        ref->attachAxis(xAxis);
        ref->attachAxis(yAxis);
    }

    ui->chart->setChart(m_chart);

    delete oldChart;
}

void NoiseFigureGUI::on_chartSelect_currentIndexChanged(int index)
{
    (void) index;
    plotChart();
}

// Columns in table reordered
void NoiseFigureGUI::results_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_resultsColumnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void NoiseFigureGUI::results_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_resultsColumnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void NoiseFigureGUI::resultsColumnSelectMenu(QPoint pos)
{
    resultsMenu->popup(ui->results->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void NoiseFigureGUI::resultsColumnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->results->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction *NoiseFigureGUI::createCheckableItem(QString &text, int idx, bool checked, const char *slot)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, slot);
    return action;
}

NoiseFigureGUI* NoiseFigureGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    NoiseFigureGUI* gui = new NoiseFigureGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void NoiseFigureGUI::destroy()
{
    delete this;
}

void NoiseFigureGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray NoiseFigureGUI::serialize() const
{
    return m_settings.serialize();
}

bool NoiseFigureGUI::deserialize(const QByteArray& data)
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

// Call when sample rate or FFT size changes
void NoiseFigureGUI::updateBW()
{
    // We use a single bin for measurement
    double bw = m_basebandSampleRate / (double)m_settings.m_fftSize;
    ui->rfBWText->setText(QString("%1k").arg(bw / 1000.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    // TODO: Could display total measurement time
}

bool NoiseFigureGUI::handleMessage(const Message& message)
{
    if (NoiseFigure::MsgConfigureNoiseFigure::match(message))
    {
        qDebug("NoiseFigureGUI::handleMessage: NoiseFigure::MsgConfigureNoiseFigure");
        const NoiseFigure::MsgConfigureNoiseFigure& cfg = (NoiseFigure::MsgConfigureNoiseFigure&) message;
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
        m_deviceCenterFrequency = notif.getSampleRate();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        updateBW();
        return true;
    }
    else if (NoiseFigure::MsgNFMeasurement::match(message))
    {
        NoiseFigure::MsgNFMeasurement& report = (NoiseFigure::MsgNFMeasurement&) message;
        measurementReceived(report);
        return true;
    }
    else if (NoiseFigure::MsgFinished::match(message))
    {
        NoiseFigure::MsgFinished& report = (NoiseFigure::MsgFinished&) message;
        ui->startStop->setChecked(false);
        m_runningTest = false;
        QString errorMessage = report.getErrorMessage();
        if (!errorMessage.isEmpty()) {
            QMessageBox::critical(this, "Noise Figure", errorMessage);
        }
        return true;
    }

    return false;
}

void NoiseFigureGUI::handleInputMessages()
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

void NoiseFigureGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void NoiseFigureGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void NoiseFigureGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void NoiseFigureGUI::on_fftCount_valueChanged(int value)
{
    m_settings.m_fftCount = 10000 * value;
    ui->fftCountText->setText(QString("%1k").arg(m_settings.m_fftCount / 1000));
    applySettings();
}

void NoiseFigureGUI::on_setting_currentTextChanged(const QString& text)
{
    m_settings.m_setting = text;
    applySettings();
}

void NoiseFigureGUI::updateFreqWidgets()
{
    bool range = m_settings.m_sweepSpec == NoiseFigureSettings::RANGE;
    bool step = m_settings.m_sweepSpec == NoiseFigureSettings::STEP;
    bool list = m_settings.m_sweepSpec == NoiseFigureSettings::LIST;
    ui->startLabel->setVisible(range || step);
    ui->start->setVisible(range || step);
    ui->stopLabel->setVisible(range || step);
    ui->stop->setVisible(range || step);
    ui->stepsLabel->setVisible(range);
    ui->steps->setVisible(range);
    ui->stepLabel->setVisible(step);
    ui->step->setVisible(step);
    ui->list->setVisible(list);
}

void NoiseFigureGUI::on_frequencySpec_currentIndexChanged(int index)
{
    m_settings.m_sweepSpec = (NoiseFigureSettings::SweepSpec)index;
    updateFreqWidgets();
    applySettings();
}

void NoiseFigureGUI::on_start_valueChanged(double value)
{
    m_settings.m_startValue = value;
    applySettings();
}

void NoiseFigureGUI::on_stop_valueChanged(double value)
{
    m_settings.m_stopValue = value;
    applySettings();
}

void NoiseFigureGUI::on_steps_valueChanged(int value)
{
    m_settings.m_steps = value;
    applySettings();
}

void NoiseFigureGUI::on_step_valueChanged(double value)
{
    m_settings.m_step = value;
    applySettings();
}

void NoiseFigureGUI::on_list_editingFinished()
{
    m_settings.m_sweepList = ui->list->text().trimmed();
    applySettings();
}

void NoiseFigureGUI::on_fftSize_currentIndexChanged(int index)
{
    m_settings.m_fftSize = 1 << (index + 6);
    updateBW();
    applySettings();
}

void NoiseFigureGUI::on_startStop_clicked()
{
    // Check we have at least on ENR value
    if (m_settings.m_enr.size() < 1)
    {
        QMessageBox::critical(this, "Noise Figure", "You must enter the ENR of the noise source for at least one frequency");
        return;
    }
    // Clear current results if starting a test
    if (!m_runningTest)
    {
        on_clearResults_clicked();
        m_runningTest = true;
    }
    // Send message to start/stop test
    NoiseFigure::MsgStartStop* message = NoiseFigure::MsgStartStop::create();
    m_noiseFigure->getInputMessageQueue()->push(message);
}

// Save results in table to a CSV file
void NoiseFigureGUI::on_saveResults_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select file to save results to", "", "*.csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            qDebug() << "NoiseFigureGUI: Saving results to " << fileNames;
            QFile file(fileNames[0]);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox::critical(this, "Noise Figure", QString("Failed to open file %1").arg(fileNames[0]));
                return;
            }
            QTextStream out(&file);

            // Create a CSV file from the values in the table
            out << ui->results->horizontalHeaderItem(0)->text() << ",NF (dB),Noise Temp (K),Y (dB),ENR (dB)\n";
            for (int i = 0; i < ui->results->rowCount(); i++)
            {
                for (int j = 0; j < NOISEFIGURE_COLUMNS; j++)
                {
                    double val = ui->results->item(i,j)->data(Qt::DisplayRole).toDouble();
                    out << val << ",";
                }
                out << "\n";
            }
        }
    }
}

// Open .csv containing reference data to plot
void NoiseFigureGUI::on_openReference_clicked()
{
    QFileDialog fileDialog(nullptr, "Open file to plot", "", "*.csv");
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            m_refFilename = fileNames[0];
            QFile file(m_refFilename);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QMessageBox::critical(this, "Noise Figure", QString("Failed to open file %1").arg(m_refFilename));
                return;
            }
            QTextStream in(&file);

            // Parse CSV file
            QString headerLine = in.readLine();
            m_refCols = headerLine.split(",").size();
            m_refData.clear();
            QString line;
            while (!(line = in.readLine()).isNull())
            {
                QStringList cols = line.split(",");
                for (int i = 0; i < m_refCols; i++)
                {
                    if (i < cols.size()) {
                        bool ok;
                        m_refData.append(cols[i].toDouble(&ok));
                        if (!ok) {
                            qDebug() << "NoiseFigureGUI::on_openReference_clicked: Error parsing " << cols[i] << " as a double";
                        }
                    } else {
                        m_refData.append(0.0);
                    }
                }
            }
            plotChart();
        }
    }
}

void NoiseFigureGUI::on_clearReference_clicked()
{
    m_refFilename = "";
    m_refData.clear();
    m_refCols = 0;
    plotChart();
}

void NoiseFigureGUI::on_clearResults_clicked()
{
    ui->results->setRowCount(0);
    plotChart();
}

void NoiseFigureGUI::on_enr_clicked()
{
    NoiseFigureENRDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted)
    {
        applySettings();
    }
}

void NoiseFigureGUI::on_control_clicked()
{
    NoiseFigureControlDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted)
    {
        applySettings();
    }
}

void NoiseFigureGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void NoiseFigureGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_noiseFigure->getNumberOfDeviceStreams());
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

NoiseFigureGUI::NoiseFigureGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::NoiseFigureGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_doApplySettings(true),
    m_basebandSampleRate(1000000),
    m_tickCount(0),
    m_runningTest(false),
    m_chart(nullptr)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/noisefigure/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_noiseFigure = reinterpret_cast<NoiseFigure*>(rxChannel);
    m_noiseFigure->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_basebandSampleRate / m_settings.m_fftSize);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("Noise Figure");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    // Resize the table using dummy data
    resizeTable();
    // Allow user to reorder columns
    ui->results->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->results->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    resultsMenu = new QMenu(ui->results);
    for (int i = 0; i < ui->results->horizontalHeader()->count(); i++)
    {
        QString text = ui->results->horizontalHeaderItem(i)->text();
        resultsMenu->addAction(createCheckableItem(text, i, true, SLOT(resultsColumnSelectMenuChecked())));
    }
    ui->results->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->results->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(resultsColumnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->results->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(results_sectionMoved(int, int, int)));
    connect(ui->results->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(results_sectionResized(int, int, int)));
    ui->results->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->results, SIGNAL(customContextMenuRequested(QPoint)), SLOT(customContextMenuRequested(QPoint)));

    ui->results->setItemDelegateForColumn(RESULTS_COL_NF, new DecimalDelegate(2));
    ui->results->setItemDelegateForColumn(RESULTS_COL_TEMP, new DecimalDelegate(0));
    ui->results->setItemDelegateForColumn(RESULTS_COL_Y, new DecimalDelegate(2));
    ui->results->setItemDelegateForColumn(RESULTS_COL_ENR, new DecimalDelegate(2));
    ui->results->setItemDelegateForColumn(RESULTS_COL_FLOOR, new DecimalDelegate(1));

    displaySettings();
    makeUIConnections();
    applySettings(true);
}

void NoiseFigureGUI::customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item =  ui->results->itemAt(pos);
    if (item)
    {
        QMenu* tableContextMenu = new QMenu(ui->results);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);
        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
        });
        tableContextMenu->addAction(copyAction);
        tableContextMenu->popup(ui->results->viewport()->mapToGlobal(pos));
    }
}

NoiseFigureGUI::~NoiseFigureGUI()
{
    delete ui;
}

void NoiseFigureGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void NoiseFigureGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        NoiseFigure::MsgConfigureNoiseFigure* message = NoiseFigure::MsgConfigureNoiseFigure::create( m_settings, force);
        m_noiseFigure->getInputMessageQueue()->push(message);
    }
}

void NoiseFigureGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->fftCountText->setText(QString("%1k").arg(m_settings.m_fftCount / 1000));
    ui->fftCount->setValue(m_settings.m_fftCount / 10000);

    ui->setting->setCurrentText(m_settings.m_setting);

    ui->frequencySpec->setCurrentIndex((int)m_settings.m_sweepSpec);
    updateFreqWidgets();
    ui->start->setValue(m_settings.m_startValue);
    ui->stop->setValue(m_settings.m_stopValue);
    ui->steps->setValue(m_settings.m_steps);
    ui->step->setValue(m_settings.m_step);
    ui->list->setText(m_settings.m_sweepList);

    ui->fftSize->setCurrentIndex(log2(m_settings.m_fftSize) - 6);
    updateBW();

    updateIndexLabel();

    // Order and size columns
    QHeaderView *header = ui->results->horizontalHeader();
    for (int i = 0; i < NOISEFIGURE_COLUMNS; i++)
    {
        bool hidden = m_settings.m_resultsColumnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        resultsMenu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_resultsColumnSizes[i] > 0) {
            ui->results->setColumnWidth(i, m_settings.m_resultsColumnSizes[i]);
        }
        header->moveSection(header->visualIndex(i), m_settings.m_resultsColumnIndexes[i]);
    }

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void NoiseFigureGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void NoiseFigureGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void NoiseFigureGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_noiseFigure->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    m_tickCount++;
}

void NoiseFigureGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &NoiseFigureGUI::on_deltaFrequency_changed);
    QObject::connect(ui->fftCount, &QSlider::valueChanged, this, &NoiseFigureGUI::on_fftCount_valueChanged);
    QObject::connect(ui->setting, &QComboBox::currentTextChanged, this, &NoiseFigureGUI::on_setting_currentTextChanged);
    QObject::connect(ui->frequencySpec, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NoiseFigureGUI::on_frequencySpec_currentIndexChanged);
    QObject::connect(ui->start, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NoiseFigureGUI::on_start_valueChanged);
    QObject::connect(ui->stop, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NoiseFigureGUI::on_stop_valueChanged);
    QObject::connect(ui->steps, QOverload<int>::of(&QSpinBox::valueChanged), this, &NoiseFigureGUI::on_steps_valueChanged);
    QObject::connect(ui->step, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NoiseFigureGUI::on_step_valueChanged);
    QObject::connect(ui->list, &QLineEdit::editingFinished, this, &NoiseFigureGUI::on_list_editingFinished);
    QObject::connect(ui->fftSize, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NoiseFigureGUI::on_fftSize_currentIndexChanged);
    QObject::connect(ui->startStop, &ButtonSwitch::clicked, this, &NoiseFigureGUI::on_startStop_clicked);
    QObject::connect(ui->saveResults, &QToolButton::clicked, this, &NoiseFigureGUI::on_saveResults_clicked);
    QObject::connect(ui->clearResults, &QToolButton::clicked, this, &NoiseFigureGUI::on_clearResults_clicked);
    QObject::connect(ui->enr, &QToolButton::clicked, this, &NoiseFigureGUI::on_enr_clicked);
    QObject::connect(ui->control, &QToolButton::clicked, this, &NoiseFigureGUI::on_control_clicked);
    QObject::connect(ui->chartSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NoiseFigureGUI::on_chartSelect_currentIndexChanged);
    QObject::connect(ui->openReference, &QToolButton::clicked, this, &NoiseFigureGUI::on_openReference_clicked);
    QObject::connect(ui->clearReference, &QToolButton::clicked, this, &NoiseFigureGUI::on_clearReference_clicked);
}

void NoiseFigureGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
