///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Copyright (C) 2023 Mohamed <mohamedadlyi@github.com>                          //
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
#include <QMainWindow>
#include <QDebug>
#include <QMessageBox>
#include <QAction>
#include <QClipboard>
#include <QBuffer>

#include "heatmapgui.h"

#include "device/deviceuiset.h"
#include "device/deviceapi.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_heatmapgui.h"
#include "plugin/pluginapi.h"
#include "util/astronomy.h"
#include "util/csv.h"
#include "util/colormap.h"
#include "util/db.h"
#include "util/units.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspengine.h"
#include "maincore.h"

#include "heatmap.h"

#include "SWGMapItem.h"

HeatMapGUI* HeatMapGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    HeatMapGUI* gui = new HeatMapGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void HeatMapGUI::destroy()
{
    delete this;
}

void HeatMapGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray HeatMapGUI::serialize() const
{
    return m_settings.serialize();
}

bool HeatMapGUI::deserialize(const QByteArray& data)
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

bool HeatMapGUI::handleMessage(const Message& message)
{
    if (HeatMap::MsgConfigureHeatMap::match(message))
    {
        qDebug("HeatMapGUI::handleMessage: HeatMap::MsgConfigureHeatMap");
        const HeatMap::MsgConfigureHeatMap& cfg = (HeatMap::MsgConfigureHeatMap&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->scopeGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }

    return false;
}

void HeatMapGUI::handleInputMessages()
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

void HeatMapGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void HeatMapGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void HeatMapGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void HeatMapGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void HeatMapGUI::on_minPower_valueChanged(double value)
{
    m_settings.m_minPower = (float)value;
    plotMap();
    if (m_powerYAxis) {
        m_powerYAxis->setMin(m_settings.m_minPower);
    }
    applySettings();
}

void HeatMapGUI::on_maxPower_valueChanged(double value)
{
    m_settings.m_maxPower = (float)value;
    plotMap();
    if (m_powerYAxis) {
        m_powerYAxis->setMax(m_settings.m_maxPower);
    }
    applySettings();
}

void HeatMapGUI::on_colorMap_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_colorMapName = ui->colorMap->currentText();
        m_colorMap = ColorMap::getColorMap(m_settings.m_colorMapName);
    }
    plotMap();
    applySettings();
}

void HeatMapGUI::on_pulseTH_valueChanged(int value)
{
    m_settings.m_pulseThreshold = (float)value;
    ui->pulseTHText->setText(QString::number(value));
    applySettings();
}

const QStringList HeatMapGUI::m_averagePeriodTexts = {
    "10us", "100us", "1ms", "10ms", "100ms", "1s", "10s"
};

void HeatMapGUI::on_averagePeriod_valueChanged(int value)
{
    m_settings.m_averagePeriodUS = (int)std::pow(10.0f, (float)value);
    ui->averagePeriodText->setText(m_averagePeriodTexts[value-1]);
    applySettings();
}

const QStringList HeatMapGUI::m_sampleRateTexts = {
     "100", "1k", "10k", "100k", "1M", "10M"
};

void HeatMapGUI::on_sampleRate_valueChanged(int value)
{
    m_settings.m_sampleRate = (int)std::pow(10.0f, (float)value);
    ui->sampleRateText->setText(m_sampleRateTexts[value-2]); // value range is [2,7]
    ui->averagePeriod->setMinimum(std::max(1, static_cast<int> (m_averagePeriodTexts.size()) - value));
    ui->rfBW->setMaximum(m_settings.m_sampleRate/100);
    m_scopeVis->setLiveRate(m_settings.m_sampleRate);
    applySettings();
}

void HeatMapGUI::on_mode_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_mode = (HeatMapSettings::Mode)index;
        bool none = m_settings.m_mode == HeatMapSettings::None;
        ui->writeImage->setEnabled(!none);
        ui->writeCSV->setEnabled(!none);
        ui->readCSV->setEnabled(!none);
        ui->colorMapLabel->setEnabled(!none);
        ui->colorMap->setEnabled(!none);
        if (none)
        {
            deleteFromMap();
        }
        else
        {
            if (m_image.isNull()) {
                createImage(m_width, m_height);
            }
            plotMap();
        }
        applySettings();
    }
}

void HeatMapGUI::displayPowerChart()
{
    if (m_settings.m_displayChart)
    {
        ui->chartContainer->setVisible(true);
        plotPowerVsTimeChart();
    }
    else
    {
        ui->chartContainer->setVisible(false);
        QChart *emptyChart = new QChart();
        emptyChart->setTheme(QChart::ChartThemeDark);
        ui->powerChart->setChart(emptyChart); // Can't set to nullptr
        delete m_powerChart;
        m_powerChart = emptyChart;
        m_powerAverageSeries = nullptr; // Deleted when chart deleted?
    }
}

void HeatMapGUI::on_displayChart_clicked(bool checked)
{
    m_settings.m_displayChart = checked;
    displayPowerChart();
    applySettings();
}

void HeatMapGUI::on_clearHeatMap_clicked()
{
    // Clear heat map
    m_heatMap->resetMagLevels();
    clearPower();
    plotMap();
    if (m_powerAverageSeries)
    {
        m_powerAverageSeries->clear();
        m_powerMaxPeakSeries->clear();
        m_powerMinPeakSeries->clear();
        m_powerPulseAverageSeries->clear();
        m_powerPathLossSeries->clear();
    }
}

bool HeatMapGUI::pixelValid(int x, int y) const
{
    return (y >= 0) && (x >= 0) && (y < m_height) && (x < m_width);
}

void HeatMapGUI::coordsToPixel(double latitude, double longitude, int& x, int& y) const
{
    y = m_height - (latitude - m_south) / m_degreesLatPerPixel;
    x = (longitude - m_west) / m_degreesLonPerPixel;
}

void HeatMapGUI::pixelToCoords(int x, int y, double& latitude, double& longitude) const
{
    latitude = m_north - y * m_degreesLatPerPixel;
    longitude = m_west + x * m_degreesLonPerPixel;
}

void HeatMapGUI::on_writeCSV_clicked()
{
    m_csvFileDialog.setAcceptMode(QFileDialog::AcceptSave);
    m_csvFileDialog.setNameFilter("*.csv");
    if (m_csvFileDialog.exec())
    {
        QStringList fileNames = m_csvFileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            QFile file(fileNames[0]);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                QMessageBox::critical(this, "Heat Map", QString("Failed to open file %1").arg(fileNames[0]));
                return;
            }
            QTextStream out(&file);
            out.setRealNumberPrecision(9);  // https://bugreports.qt.io/browse/QTBUG-110775 - set to 9, so we get at least 6 fractional digits

            out << "Latitude,Longitude," << ui->mode->currentText() << " Power (dB)\n";
            float *power = getCurrentModePowerData();
            for (int y = 0; y < m_height; y++)
            {
                for (int x = 0; x < m_width; x++)
                {
                    float pow = power[y*m_width+x];
                    if (!std::isnan(pow))
                    {
                        double latitude, longitude;
                        pixelToCoords(x, y, latitude, longitude);
                        out << latitude << "," << longitude << "," << pow << "\n";
                    }
                }
            }
        }
    }
}

void HeatMapGUI::on_readCSV_clicked()
{
    m_csvFileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    m_csvFileDialog.setNameFilter("*.csv");
    if (m_csvFileDialog.exec())
    {
        QStringList fileNames = m_csvFileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            QFile file(fileNames[0]);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                QMessageBox::critical(this, "Heat Map", QString("Failed to open file %1").arg(fileNames[0]));
                return;
            }
            QTextStream in(&file);
            QString powerColName = ui->mode->currentText() + " Power (dB)";
            QString error;
            QStringList colNames = {"Latitude", "Longitude", powerColName};
            QHash<QString, int> colIndexes = CSV::readHeader(in, colNames, error);
            if (error.isEmpty())
            {
                clearPower(getCurrentModePowerData());
                clearImage();
                int latitudeCol = colIndexes.value("Latitude");
                int longitudeCol = colIndexes.value("Longitude");
                int powerCol = colIndexes.value(powerColName);
                QStringList cols;
                while (CSV::readRow(in, &cols))
                {
                    if (cols.size() >= 3)
                    {
                        double latitude = cols[latitudeCol].toDouble();
                        double longitude = cols[longitudeCol].toDouble();
                        float power = cols[powerCol].toFloat();
                        updatePower(latitude, longitude, power);
                    }
                }
            }
            else
            {
                QString actualColNames = colIndexes.keys().join(" ");
                QString expectedColNames = colNames.join(" ");
                QMessageBox::critical(this, "Heat Map", QString("Failed to read expected header in CSV file. %1 != %2").arg(actualColNames).arg(expectedColNames));
                return;
            }
        }
    }
}

void HeatMapGUI::on_writeImage_clicked()
{
    m_imageFileDialog.setAcceptMode(QFileDialog::AcceptSave);
    m_imageFileDialog.setNameFilter("*.png *.jpg *.jpeg *.bmp *.ppm *.xbm *.xpm");
    if (m_imageFileDialog.exec())
    {
        QStringList fileNames = m_imageFileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            if (!m_image.save(fileNames[0]))
            {
                QMessageBox::critical(this, "Heat Map", QString("Failed to save image to %1").arg(fileNames[0]));
                return;
            }
        }
    }
}

void HeatMapGUI::displayTXPosition(bool enabled)
{
    ui->txLatitudeLabel->setEnabled(enabled);
    ui->txLatitude->setEnabled(enabled);
    ui->txLongitude->setEnabled(enabled);
    ui->txLongitudeLabel->setEnabled(enabled);
    ui->txPower->setEnabled(enabled);
    ui->txPowerLabel->setEnabled(enabled);
    ui->txPowerUnits->setEnabled(enabled);
    ui->txPositionSet->setEnabled(enabled);
    ui->rangeLabel->setEnabled(enabled);
    ui->range->setEnabled(enabled);
    ui->rangeUnits->setEnabled(enabled);
    ui->pathLossLabel->setEnabled(enabled);
    ui->pathLoss->setEnabled(enabled);
    ui->pathLossUnits->setEnabled(enabled);
    if (enabled) {
        sendTxToMap();
    } else {
        deleteTxFromMap();
    }
}

void HeatMapGUI::on_txPosition_clicked(bool checked)
{
    m_settings.m_txPosValid = checked;
    displayTXPosition(checked);
    applySettings();
}

void HeatMapGUI::on_txLatitude_editingFinished()
{
    m_settings.m_txLatitude = ui->txLatitude->text().toFloat();
    updateRange();
    sendTxToMap();
    applySettings();
}

void HeatMapGUI::on_txLongitude_editingFinished()
{
    m_settings.m_txLongitude = ui->txLongitude->text().toFloat();
    updateRange();
    sendTxToMap();
    applySettings();
}

void HeatMapGUI::on_txPower_valueChanged(double value)
{
    m_settings.m_txPower = (float)value;
    sendTxToMap();
    applySettings();
}

void HeatMapGUI::on_txPositionSet_clicked(bool checked)
{
    (void) checked;

    ui->txLatitude->setText(QString::number(m_latitude));
    ui->txLongitude->setText(QString::number(m_longitude));
    m_settings.m_txLatitude = m_latitude;
    m_settings.m_txLongitude = m_longitude;
    updateRange();
    sendTxToMap();
    applySettings();
}

void HeatMapGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void HeatMapGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_heatMap->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, true);
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

HeatMapGUI::HeatMapGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::HeatMapGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true),
    m_tickCount(0),
    m_powerChart(nullptr),
    m_powerAverageSeries(nullptr),
    m_powerMaxPeakSeries(nullptr),
    m_powerMinPeakSeries(nullptr),
    m_powerPulseAverageSeries(nullptr),
    m_powerPathLossSeries(nullptr)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/heatmap/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_heatMap = reinterpret_cast<HeatMap*>(rxChannel);
    m_heatMap->setMessageQueueToGUI(getInputMessageQueue());


#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Disable 256MB limit on image size
    QImageReader::setAllocationLimit(0);
#endif

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    m_scopeVis = m_heatMap->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

    // Scope settings to display magdB
    ui->scopeGUI->setPreTrigger(1);
    GLScopeSettings::TraceData traceDataI, traceDataQ;
    traceDataI.m_projectionType = Projector::ProjectionMagDB;
    traceDataI.m_amp = 1.0;      // for 0 -100 dB
    traceDataI.m_ofs = 0.0;
    ui->scopeGUI->changeTrace(0, traceDataI);
    ui->scopeGUI->setDisplayMode(GLScopeSettings::DisplayX);
    ui->scopeGUI->focusOnTrace(0); // re-focus to take changes into account in the GUI

    GLScopeSettings::TriggerData triggerData;
    triggerData.m_triggerLevel = 0.1;
    triggerData.m_triggerLevelCoarse = 10;
    triggerData.m_triggerPositiveEdge = true;
    ui->scopeGUI->changeTrigger(0, triggerData);
    ui->scopeGUI->focusOnTrigger(0); // re-focus to take changes into account in the GUI

    m_scopeVis->setLiveRate(m_settings.m_sampleRate);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("Heat Map");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setScopeGUI(ui->scopeGUI);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    m_latitude = MainCore::instance()->getSettings().getLatitude();
    m_longitude = MainCore::instance()->getSettings().getLongitude();
    m_altitude = MainCore::instance()->getSettings().getAltitude();
    ui->latitude->setText(QString::number(m_latitude));
    ui->longitude->setText(QString::number(m_longitude));

    QStringList colorMapNames = ColorMap::getColorMapNames();
    for (const auto& color : colorMapNames) {
        ui->colorMap->addItem(color);
    }
    m_colorMap = ColorMap::getColorMap(m_settings.m_colorMapName);

    m_pen.setColor(Qt::black);
    m_painter.setPen(m_pen);

    createMap();

    // Get updated when position changes
    connect(&MainCore::instance()->getSettings(), &MainSettings::preferenceChanged, this, &HeatMapGUI::preferenceChanged);

    ui->scopeContainer->setVisible(false);

    displaySettings();
    makeUIConnections();
    applySettings(true);
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();

    plotPowerVsTimeChart();
}

HeatMapGUI::~HeatMapGUI()
{
    disconnect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));
    deleteFromMap();
    deleteTxFromMap();
    deleteMap();
    delete ui;
}

void HeatMapGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void HeatMapGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        HeatMap::MsgConfigureHeatMap* message = HeatMap::MsgConfigureHeatMap::create(m_settings, force);
        m_heatMap->getInputMessageQueue()->push(message);
    }
}

void HeatMapGUI::displaySettings()
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

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);

    ui->minPower->setValue(m_settings.m_minPower);
    ui->maxPower->setValue(m_settings.m_maxPower);
    ui->colorMap->setCurrentText(m_settings.m_colorMapName);
    m_colorMap = ColorMap::getColorMap(m_settings.m_colorMapName);

    ui->mode->setCurrentIndex((int)m_settings.m_mode);
    ui->pulseTH->setValue(m_settings.m_pulseThreshold);
    ui->pulseTHText->setText(QString::number((int)m_settings.m_pulseThreshold));

    int value = (int)std::log10(m_settings.m_averagePeriodUS);
    ui->averagePeriod->setValue(value);
    ui->averagePeriodText->setText(m_averagePeriodTexts[value-1]);

    value = (int)std::log10(m_settings.m_sampleRate);
    ui->sampleRate->setValue(value);
    int idx = std::min(std::max(0, value-2), (int)m_sampleRateTexts.size() - 1);
    ui->sampleRateText->setText(m_sampleRateTexts[idx]);
    ui->averagePeriod->setMinimum(std::max(1, static_cast<int> (m_averagePeriodTexts.size()) - value));

    ui->txPosition->setChecked(m_settings.m_txPosValid);
    displayTXPosition(m_settings.m_txPosValid);
    ui->txLatitude->setText(QString::number(m_settings.m_txLatitude));
    ui->txLongitude->setText(QString::number(m_settings.m_txLongitude));
    ui->txPower->setValue(m_settings.m_txPower);

    ui->displayChart->setChecked(m_settings.m_displayChart);
    displayPowerChart();
    ui->displayAverage->setChecked(m_settings.m_displayAverage);
    ui->displayMax->setChecked(m_settings.m_displayMax);
    ui->displayMin->setChecked(m_settings.m_displayMin);
    ui->displayPulseAverage->setChecked(m_settings.m_displayPulseAverage);
    ui->displayPathLoss->setChecked(m_settings.m_displayPathLoss);
    ui->displayMins->setValue(m_settings.m_displayMins);
    ui->recordAverage->setChecked(m_settings.m_recordAverage);
    ui->recordMax->setChecked(m_settings.m_recordMax);
    ui->recordMin->setChecked(m_settings.m_recordMin);
    ui->recordPulseAverage->setChecked(m_settings.m_recordPulseAverage);
    ui->recordPathLoss->setChecked(m_settings.m_recordPathLoss);

    m_scopeVis->setLiveRate(m_settings.m_sampleRate);

    updateIndexLabel();
    updateRange();
    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void HeatMapGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void HeatMapGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void HeatMapGUI::tick()
{
    // Update power meter widget
    double magsqAvg, magsqpeak;
    int nbMagsqSamples;
    m_heatMap->getMagSqLevels(magsqAvg, magsqpeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbMaxPeak = CalcDb::dbPower(magsqpeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbMaxPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    // Get power measurements and plot on map
    if ((m_y >= 0) && (m_x >= 0) && (m_y < m_height) && (m_x < m_width))
    {
        double magAvg, magPulseAvg, magMaxPeak, magMinPeak;
        m_heatMap->getMagLevels(magAvg, magPulseAvg, magMaxPeak, magMinPeak);

        double powDbPulseAvg, powDbMinPeak, powDbPathLoss;
        powDbAvg = std::numeric_limits<double>::quiet_NaN();
        powDbPulseAvg = std::numeric_limits<double>::quiet_NaN();
        powDbMaxPeak = std::numeric_limits<double>::quiet_NaN();
        powDbMinPeak = std::numeric_limits<double>::quiet_NaN();
        powDbPathLoss = std::numeric_limits<double>::quiet_NaN();

        int idx = m_y * m_width + m_x;
        if (!std::isnan(magAvg))
        {
            powDbAvg = CalcDb::dbPower(magAvg * magAvg);
            if (m_powerAverage) {
                m_powerAverage[idx] = powDbAvg;
            }
            if (m_tickCount % 4 == 0) {
                ui->average->setText(QString::number(powDbAvg, 'f', 1));
            }
        }
        else
        {
            ui->average->setText("");
        }
        if (!std::isnan(magPulseAvg))
        {
            powDbPulseAvg = CalcDb::dbPower(magPulseAvg * magPulseAvg);
            if (m_powerPulseAverage) {
                m_powerPulseAverage[idx] = powDbPulseAvg;
            }
            if (m_tickCount % 4 == 0) {
                ui->pulseAverage->setText(QString::number(powDbPulseAvg, 'f', 1));
            }
        }
        else
        {
            ui->pulseAverage->setText("");
        }
        if (magMaxPeak != -std::numeric_limits<double>::max())
        {
            powDbMaxPeak = CalcDb::dbPower(magMaxPeak * magMaxPeak);
            if (m_powerMaxPeak) {
                m_powerMaxPeak[idx] = powDbMaxPeak;
            }
            if (m_tickCount % 4 == 0) {
                ui->maxPeak->setText(QString::number(powDbMaxPeak, 'f', 1));
            }
        }
        else
        {
            ui->maxPeak->setText("");
        }
        if (magMinPeak != std::numeric_limits<double>::max())
        {
            powDbMinPeak = CalcDb::dbPower(magMinPeak * magMinPeak);
            if (m_powerMinPeak) {
                m_powerMinPeak[idx] = powDbMinPeak;
            }
            if (m_tickCount % 4 == 0) {
                ui->minPeak->setText(QString::number(powDbMinPeak, 'f', 1));
            }
        }
        else
        {
            ui->minPeak->setText("");
        }

        double range = calcRange(m_latitude, m_longitude);
        double frequency = m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset;
        powDbPathLoss = m_settings.m_txPower - calcFreeSpacePathLoss(range, frequency);
        if (m_powerPathLoss) {
            m_powerPathLoss[idx] = powDbPathLoss;
        }

        if (m_heatMap->getDeviceAPI()->state(0) == DeviceAPI::StRunning)
        {
            addToPowerSeries(QDateTime::currentDateTime(), powDbAvg, powDbPulseAvg, powDbMaxPeak, powDbMinPeak, powDbPathLoss);
            if (m_settings.m_mode != HeatMapSettings::None)
            {
                // Plot newest measurement on map
                float *power = getCurrentModePowerData();
                if (power)
                {
                    double powDb = power[idx];
                    if (!std::isnan(powDb)) {
                        plotPixel(m_x, m_y, powDb);
                    }
                }
            }

            if (m_tickCount % 15 == 0)
            {
                trimPowerSeries(QDateTime::currentDateTime().addSecs(-10*60));
                // Updating axis range causes chart to be redrawn, so don't call for every sample
                updateAxis();
            }

        }
    }

    if (m_tickCount % 25 == 0) {
        sendToMap();
    }

    m_tickCount++;
}

float *HeatMapGUI::getCurrentModePowerData()
{
    switch (m_settings.m_mode)
    {
    case HeatMapSettings::None:
    case HeatMapSettings::Average:
        return m_powerAverage;
    case HeatMapSettings::PulseAverage:
        return m_powerPulseAverage;
    case HeatMapSettings::Max:
        return m_powerMaxPeak;
    case HeatMapSettings::Min:
        return m_powerMinPeak;
    case HeatMapSettings::PathLoss:
        return m_powerPathLoss;
    default:
        return nullptr;
    }
}

void HeatMapGUI::updatePower(double latitude, double longitude, float power)
{
    int x, y;
    coordsToPixel(latitude, longitude, x, y);
    if (!pixelValid(x, y))
    {
        resizeMap(x, y);
        coordsToPixel(latitude, longitude, x, y);
    }
    float *powerArray = getCurrentModePowerData();
    powerArray[y*m_width+x] = power;
    plotPixel(x, y, power);
}

void HeatMapGUI::plotMap()
{
    if ((m_settings.m_mode != HeatMapSettings::None) && !m_image.isNull())
    {
        clearImage();
        float *data = getCurrentModePowerData();
        if (data) {
            plotMap(data);
        }
        sendToMap();
    }
}

void HeatMapGUI::clearPower()
{
    clearPower(m_powerAverage);
    clearPower(m_powerPulseAverage);
    clearPower(m_powerMaxPeak);
    clearPower(m_powerMinPeak);
    clearPower(m_powerPathLoss);
}

void HeatMapGUI::clearPower(float *power)
{
    clearPower(power, m_width * m_height);
}

void HeatMapGUI::clearPower(float *power, int size)
{
    if (power) {
        std::fill_n(power, size, std::numeric_limits<float>::quiet_NaN());
    }
}

void HeatMapGUI::createImage(int width, int height)
{
    if (!m_image.isNull()) {
        m_painter.end();
    }

    try
    {
        if (m_settings.m_mode != HeatMapSettings::None)
        {
            qDebug() << "HeatMapGUI::createImage" << width << "*" << height;
            m_image = QImage(width, height, QImage::Format_ARGB32);
            m_painter.begin(&m_image);
        }
        else
        {
            m_image = QImage();
        }
    }
    catch (std::bad_alloc&)
    {
        m_image = QImage();
        QMessageBox::critical(this, "Heat Map", QString("Failed to allocate memory (width=%1 height=%2)").arg(m_width).arg(m_height));
    }
}

void HeatMapGUI::clearImage()
{
    m_image.fill(Qt::transparent);
    //m_pen.setColor(Qt::black);
    //m_painter.drawEllipse(QPoint(m_width/2, m_height/2), m_width/2-1, m_height/2-1);
}

void HeatMapGUI::plotMap(float *power)
{
    for (int y = 0; y < m_height; y++)
    {
        for (int x = 0; x < m_width; x++)
        {
            float pow = power[y * m_width + x];
            if (!std::isnan(pow)) {
                plotPixel(x, y, pow);
            }
        }
    }
}

void HeatMapGUI::plotPixel(int x, int y, double power)
{
    if (m_image.isNull()) {
        return;
    }

    // Normalise to [0,1]
    float powNorm = (power - m_settings.m_minPower) / (m_settings.m_maxPower - m_settings.m_minPower);
    if (powNorm < 0) {
        return; // Don't plot below min, so we can easily see where we're below the min limit
    }
    powNorm = std::max(0.0f, powNorm);
    powNorm = std::min(1.0f, powNorm);

    int index = std::round(powNorm * 255);

    QColor color = QColor::fromRgbF(m_colorMap[index*3], m_colorMap[index*3+1], m_colorMap[index*3+2]);
    m_pen.setColor(color);
    m_painter.setPen(m_pen);

    m_painter.drawPoint(QPoint(x, y));
}

void HeatMapGUI::updateRange()
{
    if (m_settings.m_txPosValid)
    {
        // Calculate range
        qreal range = calcRange(m_latitude, m_longitude);
        if (range < 1000)
        {
            ui->range->setText(QString::number(std::round(range)));
            ui->rangeUnits->setText("m");
        }
        else
        {
            ui->range->setText(QString::number(range / 1000.0, 'f', 1));
            ui->rangeUnits->setText("km");
        }
        double frequency = m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset;
        double loss = calcFreeSpacePathLoss(range, frequency);
        ui->pathLoss->setText(QString::number(loss, 'f', 1));
    }
    else
    {
        ui->range->setText("");
        ui->pathLoss->setText("");
    }
}

qreal HeatMapGUI::calcRange(double latitude, double longitude)
{
    QGeoCoordinate pos(latitude, longitude);
    QGeoCoordinate tx(m_settings.m_txLatitude, m_settings.m_txLongitude);
    return tx.distanceTo(pos);
}

double HeatMapGUI::calcFreeSpacePathLoss(double range, double frequency)
{
    // In dB
    if ((range == 0.0) || (frequency == 0.0)) {
        return 0.0;
    } else {
        return 20.0 * log10(range) + 20 * log10(frequency) + 20 * log10(4.0 * M_PI / Astronomy::m_speedOfLight);
    }
}

void HeatMapGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &HeatMapGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &HeatMapGUI::on_rfBW_valueChanged);
    QObject::connect(ui->minPower, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &HeatMapGUI::on_minPower_valueChanged);
    QObject::connect(ui->maxPower, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &HeatMapGUI::on_maxPower_valueChanged);
    QObject::connect(ui->colorMap, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HeatMapGUI::on_colorMap_currentIndexChanged);
    QObject::connect(ui->pulseTH, QOverload<int>::of(&QDial::valueChanged), this, &HeatMapGUI::on_pulseTH_valueChanged);
    QObject::connect(ui->averagePeriod, QOverload<int>::of(&QDial::valueChanged), this, &HeatMapGUI::on_averagePeriod_valueChanged);
    QObject::connect(ui->sampleRate, QOverload<int>::of(&QDial::valueChanged), this, &HeatMapGUI::on_sampleRate_valueChanged);
    QObject::connect(ui->mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HeatMapGUI::on_mode_currentIndexChanged);
    QObject::connect(ui->displayChart, &QPushButton::clicked, this, &HeatMapGUI::on_displayChart_clicked);
    QObject::connect(ui->clearHeatMap, &QPushButton::clicked, this, &HeatMapGUI::on_clearHeatMap_clicked);
    QObject::connect(ui->writeCSV, &QPushButton::clicked, this, &HeatMapGUI::on_writeCSV_clicked);
    QObject::connect(ui->readCSV, &QPushButton::clicked, this, &HeatMapGUI::on_readCSV_clicked);
    QObject::connect(ui->writeImage, &QPushButton::clicked, this, &HeatMapGUI::on_writeImage_clicked);
    QObject::connect(ui->txPosition, &QPushButton::clicked, this, &HeatMapGUI::on_txPosition_clicked);
    QObject::connect(ui->txLatitude, &QLineEdit::editingFinished, this, &HeatMapGUI::on_txLatitude_editingFinished);
    QObject::connect(ui->txLongitude, &QLineEdit::editingFinished, this, &HeatMapGUI::on_txLongitude_editingFinished);
    QObject::connect(ui->txPower, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &HeatMapGUI::on_txPower_valueChanged);
    QObject::connect(ui->txPositionSet, &QPushButton::clicked, this, &HeatMapGUI::on_txPositionSet_clicked);
    QObject::connect(ui->displayAverage, &QCheckBox::clicked, this, &HeatMapGUI::on_displayAverage_clicked);
    QObject::connect(ui->displayMax, &QCheckBox::clicked, this, &HeatMapGUI::on_displayMax_clicked);
    QObject::connect(ui->displayMin, &QCheckBox::clicked, this, &HeatMapGUI::on_displayMin_clicked);
    QObject::connect(ui->displayPulseAverage, &QCheckBox::clicked, this, &HeatMapGUI::on_displayPulseAverage_clicked);
    QObject::connect(ui->displayPathLoss, &QCheckBox::clicked, this, &HeatMapGUI::on_displayPathLoss_clicked);
    QObject::connect(ui->displayMins, QOverload<int>::of(&QSpinBox::valueChanged), this, &HeatMapGUI::on_displayMins_valueChanged);
    QObject::connect(ui->recordAverage, &QCheckBox::clicked, this, &HeatMapGUI::on_recordAverage_clicked);
    QObject::connect(ui->recordMax, &QCheckBox::clicked, this, &HeatMapGUI::on_recordMax_clicked);
    QObject::connect(ui->recordMin, &QCheckBox::clicked, this, &HeatMapGUI::on_recordMin_clicked);
    QObject::connect(ui->recordPulseAverage, &QCheckBox::clicked, this, &HeatMapGUI::on_recordPulseAverage_clicked);
    QObject::connect(ui->recordPathLoss, &QCheckBox::clicked, this, &HeatMapGUI::on_recordPathLoss_clicked);
}

void HeatMapGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

void HeatMapGUI::sendToMap()
{
    if (m_settings.m_mode != HeatMapSettings::None)
    {
        // Send to Map feature
        QList<ObjectPipe*> mapPipes;
        MainCore::instance()->getMessagePipes().getMessagePipes(m_heatMap, "mapitems", mapPipes);

        if (mapPipes.size() > 0)
        {
            // Encode image as base64 PNG
            QByteArray ba;
            QBuffer buffer(&ba);
            buffer.open(QIODevice::WriteOnly);
            m_image.save(&buffer, "PNG");
            QByteArray data = ba.toBase64();

            for (const auto& pipe : mapPipes)
            {
                MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
                swgMapItem->setName(new QString("Heat Map"));
                swgMapItem->setImage(new QString(data));
                swgMapItem->setAltitude(0);
                swgMapItem->setType(1);
                swgMapItem->setImageTileEast(m_east);
                swgMapItem->setImageTileWest(m_west);
                swgMapItem->setImageTileNorth(m_north);
                swgMapItem->setImageTileSouth(m_south);
                swgMapItem->setImageZoomLevel((float)m_zoomLevel);

                MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_heatMap, swgMapItem);
                messageQueue->push(msg);
            }
        }
    }
}

void HeatMapGUI::deleteFromMap()
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_heatMap, "mapitems", mapPipes);

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString("Heat Map"));
        swgMapItem->setImage(new QString());  // Set image to "" to delete it
        swgMapItem->setType(1);

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_heatMap, swgMapItem);
        messageQueue->push(msg);
    }
}

void HeatMapGUI::sendTxToMap()
{
    // Send to Map feature
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_heatMap, "mapitems", mapPipes);

    if (mapPipes.size() > 0)
    {
        QString text = QString("Heat Map Transmitter\nPower: %1 dB").arg(m_settings.m_txPower);
        for (const auto& pipe : mapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
            swgMapItem->setName(new QString("TX"));
            swgMapItem->setLatitude(m_settings.m_txLatitude);
            swgMapItem->setLongitude(m_settings.m_txLongitude);
            swgMapItem->setAltitude(0);
            swgMapItem->setImage(new QString("antenna.png"));
            swgMapItem->setText(new QString(text));
            swgMapItem->setModel(new QString("antenna.glb"));
            swgMapItem->setFixedPosition(true);
            swgMapItem->setLabel(new QString("TX"));
            swgMapItem->setLabelAltitudeOffset(4.5);
            swgMapItem->setAltitudeReference(1);
            swgMapItem->setType(0);

            MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_heatMap, swgMapItem);
            messageQueue->push(msg);
        }
    }
}

void HeatMapGUI::deleteTxFromMap()
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_heatMap, "mapitems", mapPipes);

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString("TX"));
        swgMapItem->setImage(new QString());  // Set image to "" to delete it
        swgMapItem->setType(0);

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_heatMap, swgMapItem);
        messageQueue->push(msg);
    }
}

void HeatMapGUI::preferenceChanged(int elementType)
{
    Preferences::ElementType pref = (Preferences::ElementType)elementType;
    if ((pref == Preferences::Latitude) || (pref == Preferences::Longitude) || (pref == Preferences::Altitude))
    {
        // Get new position
        m_latitude = MainCore::instance()->getSettings().getLatitude();
        m_longitude = MainCore::instance()->getSettings().getLongitude();
        m_altitude = MainCore::instance()->getSettings().getAltitude();

        // Display new position in GUI
        ui->latitude->setText(QString::number(m_latitude));
        ui->longitude->setText(QString::number(m_longitude));
        updateRange();

        // Map position to pixel
        int x, y;
        coordsToPixel(m_latitude, m_longitude, x, y);
        if ((x != m_x) || (y != m_y))
        {
            m_x = x;
            m_y = y;
            m_heatMap->resetMagLevels();
            if (!pixelValid(x, y)) {
                resizeMap(x, y);
            }
        }
    }
}

void HeatMapGUI::createMap()
{
    // https://wiki.openstreetmap.org/wiki/Zoom_levels
    double earthCircumference = 40075016.686;
    double scale = cos(Units::degreesToRadians(m_latitude));
    m_resolution = earthCircumference * scale / pow(2.0, m_zoomLevel+8.0); // metres per pixel
    m_width = m_blockSize;
    m_height = m_blockSize;
    m_degreesLonPerPixel = m_resolution / scale / (earthCircumference / 360.0);
    m_degreesLatPerPixel = m_resolution  / (earthCircumference / 360.0);
    int size = m_width * m_height;
    try
    {
        if (m_settings.m_recordAverage) {
            m_powerAverage = new float[size];
        } else {
            m_powerAverage = nullptr;
        }
        if (m_settings.m_recordPulseAverage) {
            m_powerPulseAverage = new float[size];
        } else {
            m_powerPulseAverage = nullptr;
        }
        if (m_settings.m_recordMax) {
            m_powerMaxPeak = new float[size];
        } else {
            m_powerMaxPeak = nullptr;
        }
        if (m_settings.m_recordMin) {
            m_powerMinPeak = new float[size];
        } else {
            m_powerMinPeak = nullptr;
        }
        if (m_settings.m_recordPathLoss) {
            m_powerPathLoss = new float[size];
        } else {
            m_powerPathLoss = nullptr;
        }

        m_north = m_latitude + m_degreesLatPerPixel * m_height / 2;
        m_south = m_latitude - m_degreesLatPerPixel * m_height / 2;
        m_east = m_longitude + m_degreesLonPerPixel * m_width / 2;
        m_west = m_longitude - m_degreesLonPerPixel * m_width / 2;
        m_x = m_width / 2;
        m_y = m_height / 2;

        createImage(m_width, m_height);
    }
    catch (std::bad_alloc&)
    {
        deleteMap();
        QMessageBox::critical(this, "Heat Map", QString("Failed to allocate memory (width=%1 height=%2)").arg(m_width).arg(m_height));
    }

    on_clearHeatMap_clicked();
}

void HeatMapGUI::deleteMap()
{
    deleteFromMap();
    delete[] m_powerAverage;
    m_powerAverage = nullptr;
    delete[] m_powerPulseAverage;
    m_powerPulseAverage = nullptr;
    delete[] m_powerMaxPeak;
    m_powerMaxPeak = nullptr;
    delete[] m_powerMinPeak;
    m_powerMinPeak = nullptr;
    delete[] m_powerPathLoss;
    m_powerPathLoss = nullptr;
    if (!m_image.isNull()) {
        m_painter.end();
    }
}

void HeatMapGUI::resizeMap(int x, int y)
{
    if ((x <= -m_blockSize) || (x >= m_width+m_blockSize) || (y <= -m_blockSize) || (y >= m_height+m_blockSize))
    {
        // Position has moved a long way - restart from scratch to avoid map being too big
        qDebug() << "HeatMapGUI::resizeMap: Position has moved significantly. Recreating map";
        deleteMap();
        createMap();
    }
    else
    {
        // Expand map
        int newWidth = m_width;
        int newHeight = m_height;
        int xOffset = 0;
        int yOffset = 0;
        if (x < 0)
        {
            newWidth += m_blockSize;
            xOffset = m_blockSize;
            m_west -= m_blockSize * m_degreesLonPerPixel;
        }
        if (x >= m_width)
        {
            newWidth += m_blockSize;
            m_east += m_blockSize * m_degreesLonPerPixel;
        }
        if (y < 0)
        {
            newHeight += m_blockSize;
            yOffset = m_blockSize * newWidth;
            m_north += m_blockSize * m_degreesLatPerPixel;
        }
        if (y >= m_height)
        {
            newHeight += m_blockSize;
            m_south -= m_blockSize * m_degreesLatPerPixel;
        }

        float *powerAverage = nullptr;
        float *powerPulseAverage = nullptr;
        float *powerMaxPeak = nullptr;
        float *powerMinPeak = nullptr;
        float *powerPathLoss = nullptr;

        int newSize = newWidth * newHeight;
        qDebug() << "HeatMapGUI::resizeMap:" << m_width << "*" << m_height << "to" << newWidth << "*" << newHeight;

        try
        {
            // Allocate new memory
            if (m_settings.m_recordAverage) {
                powerAverage = new float[newSize];
            }
            if (m_settings.m_recordPulseAverage) {
                powerPulseAverage = new float[newSize];
            }
            if (m_settings.m_recordMax) {
                powerMaxPeak = new float[newSize];
            }
            if (m_settings.m_recordMin) {
                powerMinPeak = new float[newSize];
            }
            if (m_settings.m_recordPathLoss) {
                powerPathLoss = new float[newSize];
            }

            clearPower(powerAverage, newSize);
            clearPower(powerPulseAverage, newSize);
            clearPower(powerMaxPeak, newSize);
            clearPower(powerMinPeak, newSize);
            clearPower(powerPathLoss, newSize);

            // Copy across old data
            for (int j = 0; j < m_height; j++)
            {
                int srcStart = j * m_width;
                int srcEnd = (j + 1) * m_width;
                int destStart = j * newWidth + yOffset + xOffset;
                //qDebug() << srcStart << srcEnd << destStart;
                if (powerAverage && m_powerAverage) {
                    std::copy(m_powerAverage + srcStart, m_powerAverage + srcEnd, powerAverage + destStart);
                }
                if (powerPulseAverage && m_powerPulseAverage) {
                    std::copy(m_powerPulseAverage + srcStart, m_powerPulseAverage + srcEnd, powerPulseAverage + destStart);
                }
                if (powerMaxPeak && m_powerMaxPeak) {
                    std::copy(m_powerMaxPeak + srcStart, m_powerMaxPeak + srcEnd, powerMaxPeak + destStart);
                }
                if (powerMinPeak && m_powerMinPeak) {
                    std::copy(m_powerMinPeak + srcStart, m_powerMinPeak + srcEnd, powerMinPeak + destStart);
                }
                if (powerPathLoss && m_powerPathLoss) {
                    std::copy(m_powerPathLoss + srcStart, m_powerPathLoss + srcEnd, powerPathLoss + destStart);
                }
            }

            createImage(newWidth, newHeight);

            m_width = newWidth;
            m_height = newHeight;

            // Delete old memory
            delete[] m_powerAverage;
            delete[] m_powerPulseAverage;
            delete[] m_powerMaxPeak;
            delete[] m_powerMinPeak;
            m_powerAverage = powerAverage;
            m_powerPulseAverage = powerPulseAverage;
            m_powerMaxPeak = powerMaxPeak;
            m_powerMinPeak = powerMinPeak;
            m_powerPathLoss = powerPathLoss;

            plotMap();
        }
        catch (std::bad_alloc&)
        {
            // Delete partially allocated memory
            delete[] powerAverage;
            delete[] powerPulseAverage;
            delete[] powerMaxPeak;
            delete[] powerMinPeak;
            delete[] powerPathLoss;
            QMessageBox::critical(this, "Heat Map", QString("Failed to allocate memory (width=%1 height=%2)").arg(newWidth).arg(newHeight));
        }
    }
}

void HeatMapGUI::plotPowerVsTimeChart()
{
    QChart *oldChart = m_powerChart;

    m_powerChart = new QChart();

    m_powerChart->layout()->setContentsMargins(0, 0, 0, 0);
    m_powerChart->setMargins(QMargins(1, 1, 1, 1));
    m_powerChart->setTheme(QChart::ChartThemeDark);

    m_powerChart->legend()->setAlignment(Qt::AlignBottom);
    m_powerChart->legend()->setVisible(true);

    // Create measurement data series
    m_powerAverageSeries = new QLineSeries();
    m_powerAverageSeries->setVisible(m_settings.m_displayAverage);
    m_powerAverageSeries->setName("Average");
    m_powerMaxPeakSeries = new QLineSeries();
    m_powerMaxPeakSeries->setVisible(m_settings.m_displayMax);
    m_powerMaxPeakSeries->setName("Max");
    m_powerMinPeakSeries = new QLineSeries();
    m_powerMinPeakSeries->setVisible(m_settings.m_displayMin);
    m_powerMinPeakSeries->setName("Min");
    m_powerPulseAverageSeries = new QLineSeries();
    m_powerPulseAverageSeries->setVisible(m_settings.m_displayPulseAverage);
    m_powerPulseAverageSeries->setName("Pulse Average");
    m_powerPathLossSeries = new QLineSeries();
    m_powerPathLossSeries->setVisible(m_settings.m_displayPathLoss);
    m_powerPathLossSeries->setName("Path Loss");

    // Create X axis
    m_powerXAxis = new QDateTimeAxis();
    QString dateTimeFormat = "hh:mm:ss";
    m_powerXAxis->setFormat(dateTimeFormat);
    m_powerXAxis->setTitleText("Time");

    // Create Y axis
    m_powerYAxis = new QValueAxis();
    m_powerYAxis->setRange(m_settings.m_minPower, m_settings.m_maxPower);
    m_powerYAxis->setTitleText("Power (dB)");

    m_powerChart->addAxis(m_powerXAxis, Qt::AlignBottom);
    m_powerChart->addAxis(m_powerYAxis, Qt::AlignLeft);

    m_powerChart->addSeries(m_powerAverageSeries);
    m_powerAverageSeries->attachAxis(m_powerXAxis);
    m_powerAverageSeries->attachAxis(m_powerYAxis);
    m_powerChart->addSeries(m_powerMaxPeakSeries);
    m_powerMaxPeakSeries->attachAxis(m_powerXAxis);
    m_powerMaxPeakSeries->attachAxis(m_powerYAxis);
    m_powerChart->addSeries(m_powerMinPeakSeries);
    m_powerMinPeakSeries->attachAxis(m_powerXAxis);
    m_powerMinPeakSeries->attachAxis(m_powerYAxis);
    m_powerChart->addSeries(m_powerPulseAverageSeries);
    m_powerPulseAverageSeries->attachAxis(m_powerXAxis);
    m_powerPulseAverageSeries->attachAxis(m_powerYAxis);
    m_powerChart->addSeries(m_powerPathLossSeries);
    m_powerPathLossSeries->attachAxis(m_powerXAxis);
    m_powerPathLossSeries->attachAxis(m_powerYAxis);

    ui->powerChart->setChart(m_powerChart);

    delete oldChart;
}

void HeatMapGUI::addToPowerSeries(QDateTime dateTime, double average, double pulseAverage, double max, double min, double pathLoss)
{
    if (m_powerAverageSeries)
    {
        try
        {
            qint64 msecs = dateTime.toMSecsSinceEpoch();
            if (!std::isnan(average)) {
                m_powerAverageSeries->append(msecs, average);
            }
            if (!std::isnan(pulseAverage)) {
                m_powerPulseAverageSeries->append(msecs, pulseAverage);
            }
            if (!std::isnan(max)) {
                m_powerMaxPeakSeries->append(msecs, max);
            }
            if (!std::isnan(min)) {
                m_powerMinPeakSeries->append(msecs, min);
            }
            if (!std::isnan(pathLoss)) {
                m_powerPathLossSeries->append(msecs, pathLoss);
            }
        }
        catch (std::bad_alloc&)
        {
           QMessageBox::critical(this, "Heat Map", QString("Failed to allocate memory for chart series"));
           ui->displayChart->setChecked(false);
        }
    }
}

void HeatMapGUI::updateAxis()
{
    if (!m_powerAverageSeries || !m_powerPathLossSeries || (m_powerPathLossSeries->count() <= 1)) {
        return;
    }
    QDateTime current = QDateTime::currentDateTime();
    QDateTime first = QDateTime::fromMSecsSinceEpoch(m_powerPathLossSeries->at(0).x());
    QDateTime min = current.addSecs(-60 * m_settings.m_displayMins);
    if (first > min) {
        min = first;
    }
    m_powerXAxis->setRange(min, current);
}

void HeatMapGUI::trimPowerSeries(QLineSeries *series, QDateTime dateTime)
{
    qint64 msecs = dateTime.toMSecsSinceEpoch();
    for (int i = 0; i < series->count(); i++)
    {
        const QPointF& p = series->at(i);
        if (p.x() >= msecs)
        {
            if (i > 0) {
                series->removePoints(0, i);
            }
            break;
        }
    }
}

// Remove all points in series before the specified date and time
void HeatMapGUI::trimPowerSeries(QDateTime dateTime)
{
    if (m_powerAverageSeries)
    {
        trimPowerSeries(m_powerAverageSeries, dateTime);
        trimPowerSeries(m_powerMaxPeakSeries, dateTime);
        trimPowerSeries(m_powerMinPeakSeries, dateTime);
        trimPowerSeries(m_powerPulseAverageSeries, dateTime);
        trimPowerSeries(m_powerPathLossSeries, dateTime);
    }
}

void HeatMapGUI::on_displayAverage_clicked(bool checked)
{
    m_settings.m_displayAverage = checked;
    m_powerAverageSeries->setVisible(checked);
    applySettings();
}

void HeatMapGUI::on_displayMax_clicked(bool checked)
{
    m_settings.m_displayMax = checked;
    m_powerMaxPeakSeries->setVisible(checked);
    applySettings();
}

void HeatMapGUI::on_displayMin_clicked(bool checked)
{
    m_settings.m_displayMin = checked;
    m_powerMinPeakSeries->setVisible(checked);
    applySettings();
}

void HeatMapGUI::on_displayPulseAverage_clicked(bool checked)
{
    m_settings.m_displayPulseAverage = checked;
    m_powerPulseAverageSeries->setVisible(checked);
    applySettings();
}

void HeatMapGUI::on_displayPathLoss_clicked(bool checked)
{
    m_settings.m_displayPathLoss = checked;
    m_powerPathLossSeries->setVisible(checked);
    applySettings();
}

void HeatMapGUI::on_displayMins_valueChanged(int value)
{
    m_settings.m_displayMins = value;
    updateAxis();
    applySettings();
}

void HeatMapGUI::on_recordAverage_clicked(bool checked)
{
    m_settings.m_recordAverage = checked;
    resizeMap(0, 0);
    applySettings();
}

void HeatMapGUI::on_recordMax_clicked(bool checked)
{
    m_settings.m_recordMax = checked;
    resizeMap(0, 0);
    applySettings();
}

void HeatMapGUI::on_recordMin_clicked(bool checked)
{
    m_settings.m_recordMin = checked;
    resizeMap(0, 0);
    applySettings();
}

void HeatMapGUI::on_recordPulseAverage_clicked(bool checked)
{
    m_settings.m_recordPulseAverage = checked;
    resizeMap(0, 0);
    applySettings();
}

void HeatMapGUI::on_recordPathLoss_clicked(bool checked)
{
    m_settings.m_recordPathLoss = checked;
    resizeMap(0, 0);
    applySettings();
}
