///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSignalBlocker>

#include <algorithm>
#include <cmath>

#include "util/csv.h"
#include "gui/audioselectdialog.h"
#include "gui/crightclickenabler.h"
#include "device/deviceuiset.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "audio/audiodevicemanager.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "gui/messagedialog.h"
#include "gui/glspectrum.h"
#include "dsp/spectrumvis.h"
#include "maincore.h"
#include "util/db.h"

#include "selcaldemodgui.h"
#include "ui_selcaldemodgui.h"

SELCALDemodGUI *SELCALDemodGUI::create(PluginAPI *pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    return new SELCALDemodGUI(pluginAPI, deviceUISet, rxChannel);
}

SELCALDemodGUI::SELCALDemodGUI(PluginAPI *, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget *parent) :
    ChannelGUI(parent),
    ui(new Ui::SELCALDemodGUI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_selcalDemod(static_cast<SELCALDemod *>(rxChannel))
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodselcal/readme.md";
    ui->setupUi(getRollupContents());
    setSizePolicy(getRollupContents()->sizePolicy());
    m_selcalDemod->setMessageQueueToGUI(getInputMessageQueue());

    // Spectrum of the audio analysed by the decoder, with the tone frequencies marked.
    m_spectrumVis = m_selcalDemod->getSpectrumVis();
    m_spectrumVis->setGLSpectrum(ui->glSpectrum);
    ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);
    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(SELCALDemodSettings::SELCALDEMOD_SPECTRUM_SAMPLE_RATE);
    ui->glSpectrum->setSsbSpectrum(true);
    ui->glSpectrum->setLsbDisplay(false);
    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
    spectrumSettings.m_ssb = true;
    spectrumSettings.m_fftSize = 1024;
    spectrumSettings.m_fftWindow = FFTWindow::Hanning;
    spectrumSettings.m_averagingMode = SpectrumSettings::AvgModeMoving;
    spectrumSettings.m_averagingValue = 1;
    spectrumSettings.m_displayWaterfall = true;
    spectrumSettings.m_displayMaxHold = false;
    spectrumSettings.m_displayHistogram = false;
    spectrumSettings.m_displayCurrent = true;
    spectrumSettings.m_markersDisplay = SpectrumSettings::MarkersDisplayAnnotations;
    m_spectrumVis->getInputMessageQueue()->push(SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false));

    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);
    auto *audioRightClick = new CRightClickEnabler(ui->audioMute);
    connect(audioRightClick, &CRightClickEnabler::rightClick, this, &SELCALDemodGUI::audioSelect);
    ui->messages->horizontalHeader()->setSectionsMovable(true);
    ui->messages->setColumnWidth(COL_UTC, 175);
    ui->messages->setColumnWidth(COL_CODE, 65);
    ui->messages->setColumnWidth(COL_REGISTRATION, 90);
    ui->messages->setColumnWidth(COL_TYPE, 50);
    ui->messages->setColumnWidth(COL_SIDEVIEW, 90);
    ui->messages->setColumnWidth(COL_COUNTRY, 45);
    ui->messages->setColumnWidth(COL_FREQUENCY, 110);
    ui->messages->setColumnWidth(COL_RSSI, 75);
    ui->messages->setColumnWidth(COL_SNR, 65);
    // Sizes of sideview and flag icons
    ui->messages->setIconSize(QSize(85, 20));
    // Keep rows to one line when a code has several aircraft
    ui->messages->setWordWrap(false);
    ui->messages->setContextMenuPolicy(Qt::CustomContextMenu);
    // Right click in the table header to select the columns shown
    m_columnMenu = new QMenu(ui->messages);
    for (int i = 0; i < SELCALDemodSettings::m_columns; ++i)
    {
        QAction *action = m_columnMenu->addAction(ui->messages->horizontalHeaderItem(i)->text());
        action->setCheckable(true);
        action->setChecked(true);
        action->setData(i);
        connect(action, &QAction::triggered, this, &SELCALDemodGUI::messages_columnSelectMenuChecked);
    }
    ui->messages->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    AircraftInformation::init();
    connect(&m_selcalDB, &SELCALDB::downloadingURL, this, &SELCALDemodGUI::downloadingURL);
    connect(&m_selcalDB, &SELCALDB::downloadError, this, &SELCALDemodGUI::downloadError);
    connect(&m_selcalDB, &SELCALDB::downloadProgress, this, &SELCALDemodGUI::downloadProgress);
    connect(&m_selcalDB, &SELCALDB::downloadSELCALInformationFinished, this, &SELCALDemodGUI::downloadSELCALInformationFinished);
    connect(&m_osnDB, &OsnDB::downloadingURL, this, &SELCALDemodGUI::downloadingURL);
    connect(&m_osnDB, &OsnDB::downloadError, this, &SELCALDemodGUI::downloadError);
    connect(&m_osnDB, &OsnDB::downloadProgress, this, &SELCALDemodGUI::downloadProgress);
    connect(&m_osnDB, &OsnDB::downloadAircraftInformationFinished, this, &SELCALDemodGUI::downloadAircraftInformationFinished);
    for (int i = 0; i < SELCALDecoder::m_toneCount; ++i)
    {
        const auto& tone = SELCALDecoder::tones()[i];
        ui->toneLevels->setItem(i, 0, new QTableWidgetItem(QString(QChar(tone.m_symbol))));
        ui->toneLevels->setItem(i, 1, new QTableWidgetItem(QString::number(tone.m_frequency, 'f', 1)));
        ui->toneLevels->setItem(i, 2, new QTableWidgetItem("-"));
    }
    ui->toneLevels->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);
    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_channelMarker.setVisible(true);
    m_deviceUISet->addChannelMarker(&m_channelMarker);
    connect(&m_channelMarker, &ChannelMarker::changedByCursor, this, &SELCALDemodGUI::channelMarkerChangedByCursor);
    connect(&m_channelMarker, &ChannelMarker::highlightedByCursor, this, &SELCALDemodGUI::channelMarkerHighlightedByCursor);
    connect(getInputMessageQueue(), &MessageQueue::messageEnqueued, this, &SELCALDemodGUI::handleInputMessages);
    connect(&MainCore::instance()->getMasterTimer(), &QTimer::timeout, this, &SELCALDemodGUI::tick);
    connect(this, &SELCALDemodGUI::customContextMenuRequested, this, &SELCALDemodGUI::onMenuDialogCalled);
    // Tone levels start rolled up. The rollup state restores the user's choice.
    ui->diagnosticsContainer->hide();
    connect(getRollupContents(), &RollupContents::widgetRolled, this, &SELCALDemodGUI::onWidgetRolled);
    displaySettings();
    makeUIConnections();
    applySettings({}, true);
    updateToneMarkers(m_settings.m_toneFrequencyOffset, {{-1, -1}});
    loadAircraftInformation();
    getRollupContents()->arrangeRollups();
    m_resizer.enableChildMouseTracking();
}

SELCALDemodGUI::~SELCALDemodGUI()
{
    // Rollup child hide events are emitted during QWidget destruction, after
    // ChannelGUI has already deleted its title-bar controls.
    QObject::disconnect(getRollupContents(), nullptr, this, nullptr);
    m_selcalDemod->setMessageQueueToGUI(nullptr);
    m_deviceUISet->removeChannelMarker(&m_channelMarker);
    delete ui;
}

void SELCALDemodGUI::destroy()
{
    delete this;
}

void SELCALDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings({}, true);
}

QByteArray SELCALDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool SELCALDemodGUI::deserialize(const QByteArray& data)
{
    bool valid = m_settings.deserialize(data);
    displaySettings();
    applySettings({}, true);
    return valid;
}

void SELCALDemodGUI::applySettings(const QStringList& keys, bool force)
{
    if (!m_doApplySettings) {
        return;
    }
    m_settings.normalize();
    {
        QSignalBlocker blocker(&m_channelMarker);
        m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
        bool usb = m_settings.m_modulation == SELCALDemodSettings::USB;
        m_channelMarker.setBandwidth(usb ? 2 * m_settings.m_rfBandwidth : m_settings.m_rfBandwidth);
        m_channelMarker.setSidebands(usb ? ChannelMarker::usb : ChannelMarker::dsb);
        m_channelMarker.setLowCutoff(usb ? 100 : 0);
    }
    m_selcalDemod->getInputMessageQueue()->push(SELCALDemod::MsgConfigureSELCALDemod::create(keys, m_settings, force));
    updateAbsoluteCenterFrequency();
}

void SELCALDemodGUI::displaySettings()
{
    m_doApplySettings = false;
    {
        QSignalBlocker blocker(&m_channelMarker);
        m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
        bool usb = m_settings.m_modulation == SELCALDemodSettings::USB;
        m_channelMarker.setBandwidth(usb ? 2 * m_settings.m_rfBandwidth : m_settings.m_rfBandwidth);
        m_channelMarker.setSidebands(usb ? ChannelMarker::usb : ChannelMarker::dsb);
        m_channelMarker.setLowCutoff(usb ? 100 : 0);
        m_channelMarker.setTitle(m_settings.m_title);
        m_channelMarker.setColor(m_settings.m_rgbColor);
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
    }
    setTitle(m_settings.m_title);
    setWindowTitle(m_settings.m_title);
    setTitleColor(m_settings.m_rgbColor);
    // Handlers must not change the settings being displayed. For example, the
    // modulation handler selects a default bandwidth, and limiting the
    // bandwidth slider's range can change its value.
    QSignalBlocker deltaFrequencyBlocker(ui->deltaFrequency);
    QSignalBlocker modulationBlocker(ui->modulation);
    QSignalBlocker rfBandwidthBlocker(ui->rfBandwidth);
    QSignalBlocker toneFrequencyOffsetBlocker(ui->toneFrequencyOffset);
    QSignalBlocker autoToneFrequencyOffsetBlocker(ui->autoToneFrequencyOffset);
    QSignalBlocker thresholdBlocker(ui->threshold);
    QSignalBlocker volumeBlocker(ui->volume);
    QSignalBlocker audioMuteBlocker(ui->audioMute);
    QSignalBlocker squelchBlocker(ui->squelch);
    QSignalBlocker filterBlocker(ui->filter);
    QSignalBlocker udpEnabledBlocker(ui->udpEnabled);
    QSignalBlocker udpPortBlocker(ui->udpPort);
    QSignalBlocker logEnableBlocker(ui->logEnable);
    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    ui->modulation->setCurrentIndex(m_settings.m_modulation);
    // Bandwidth in 100 Hz steps
    ui->rfBandwidth->setMaximum(m_settings.m_modulation == SELCALDemodSettings::USB ? 36 : 60);
    ui->rfBandwidth->setValue(std::lround(m_settings.m_rfBandwidth / 100.0));
    ui->rfBandwidthText->setText(QString("%1 kHz").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->toneFrequencyOffset->setValue(m_settings.m_toneFrequencyOffset);
    ui->autoToneFrequencyOffset->setChecked(m_settings.m_autoToneFrequencyOffset);
    ui->autoToneFrequencyOffset->setEnabled(m_settings.m_modulation == SELCALDemodSettings::USB);
    SELCALMessages::Status status;
    status.m_toneFrequencyOffset = m_settings.m_toneFrequencyOffset;
    updateCarrierStatus(status);
    ui->threshold->setValue(m_settings.m_threshold);
    ui->volume->setValue(std::lround(m_settings.m_volume * 10.0));
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 0, 'f', 1));
    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->squelch->setValue(m_settings.m_squelch);
    ui->squelchText->setText(QString("%1 dB").arg(m_settings.m_squelch));
    updateAudioMute();
    ui->filter->setText(m_settings.m_filter);
    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setValue(m_settings.m_udpPort);
    ui->logEnable->setChecked(m_settings.m_logEnabled);
    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    getRollupContents()->restoreState(m_rollupState);
    auto *header = ui->messages->horizontalHeader();
    // Place columns in visual order: moving in logical order can displace a
    // column restored earlier when the saved permutation contains cycles.
    for (int visual = 0; visual < SELCALDemodSettings::m_columns; ++visual) {
        for (int logical = 0; logical < SELCALDemodSettings::m_columns; ++logical) {
            if (m_settings.m_columnIndexes[logical] == visual)
            {
                header->moveSection(header->visualIndex(logical), visual);
                break;
            }
        }
    }
    // Hidden columns have zero size
    for (int i = 0; i < SELCALDemodSettings::m_columns; ++i)
    {
        bool hidden = m_settings.m_columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        m_columnMenu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_columnSizes[i] > 0) {
            header->resizeSection(i, m_settings.m_columnSizes[i]);
        }
    }
    filter();
    updateIndexLabel();
    updateAbsoluteCenterFrequency();
    m_doApplySettings = true;
}

void SELCALDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &SELCALDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->modulation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SELCALDemodGUI::on_modulation_currentIndexChanged);
    QObject::connect(ui->rfBandwidth, &QSlider::valueChanged, this, &SELCALDemodGUI::on_rfBandwidth_valueChanged);
    QObject::connect(ui->toneFrequencyOffset, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SELCALDemodGUI::on_toneFrequencyOffset_valueChanged);
    QObject::connect(ui->autoToneFrequencyOffset, &ButtonSwitch::toggled, this, &SELCALDemodGUI::on_autoToneFrequencyOffset_toggled);
    QObject::connect(ui->threshold, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SELCALDemodGUI::on_threshold_valueChanged);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &SELCALDemodGUI::on_volume_valueChanged);
    QObject::connect(ui->audioMute, &ButtonSwitch::toggled, this, &SELCALDemodGUI::on_audioMute_toggled);
    QObject::connect(ui->squelch, &QDial::valueChanged, this, &SELCALDemodGUI::on_squelch_valueChanged);
    QObject::connect(ui->filter, &QLineEdit::textChanged, this, &SELCALDemodGUI::on_filter_textChanged);
    QObject::connect(ui->clearTable, &QPushButton::clicked, this, &SELCALDemodGUI::on_clearTable_clicked);
    QObject::connect(ui->copy, &QToolButton::clicked, this, &SELCALDemodGUI::copySelection);
    QObject::connect(ui->exportCsv, &QToolButton::clicked, this, &SELCALDemodGUI::exportCsv);
    QObject::connect(ui->getSELCALDB, &QToolButton::clicked, this, &SELCALDemodGUI::on_getSELCALDB_clicked);
    QObject::connect(ui->logEnable, &ButtonSwitch::toggled, this, &SELCALDemodGUI::on_logEnable_toggled);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &SELCALDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &SELCALDemodGUI::on_logOpen_clicked);
    QObject::connect(ui->udpEnabled, &ButtonSwitch::toggled, this, &SELCALDemodGUI::on_udpEnabled_toggled);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &SELCALDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, QOverload<int>::of(&QSpinBox::valueChanged), this, &SELCALDemodGUI::on_udpPort_valueChanged);
    QObject::connect(ui->messages->horizontalHeader(), &QHeaderView::sectionMoved, this, &SELCALDemodGUI::messages_sectionMoved);
    QObject::connect(ui->messages->horizontalHeader(), &QHeaderView::sectionResized, this, &SELCALDemodGUI::messages_sectionResized);
    QObject::connect(ui->messages, &QTableWidget::customContextMenuRequested, this, &SELCALDemodGUI::messages_customContextMenuRequested);
    QObject::connect(ui->messages->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &SELCALDemodGUI::messages_columnSelectMenu);
}

void SELCALDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_settings.m_inputFrequencyOffset = value;
    applySettings({"inputFrequencyOffset"});
}

void SELCALDemodGUI::on_modulation_currentIndexChanged(int index)
{
    m_settings.m_modulation = index;
    m_settings.m_rfBandwidth = index == SELCALDemodSettings::AM ? 6000 : 3000;
    displaySettings();
    applySettings({"modulation", "rfBandwidth"});
}

void SELCALDemodGUI::on_rfBandwidth_valueChanged(int value)
{
    m_settings.m_rfBandwidth = value * 100;
    ui->rfBandwidthText->setText(QString("%1 kHz").arg(value / 10.0, 0, 'f', 1));
    applySettings({"rfBandwidth"});
}

void SELCALDemodGUI::on_toneFrequencyOffset_valueChanged(double value)
{
    m_settings.m_toneFrequencyOffset = value;
    applySettings({"toneFrequencyOffset"});
}

void SELCALDemodGUI::on_autoToneFrequencyOffset_toggled(bool checked)
{
    m_settings.m_autoToneFrequencyOffset = checked;
    displaySettings();
    applySettings({"autoToneFrequencyOffset"});
}

void SELCALDemodGUI::on_threshold_valueChanged(double value)
{
    m_settings.m_threshold = value;
    applySettings({"threshold"});
}

void SELCALDemodGUI::on_volume_valueChanged(int value)
{
    m_settings.m_volume = value / 10.0;
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    applySettings({"volume"});
}

void SELCALDemodGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
    applySettings({"audioMute"});
}

void SELCALDemodGUI::on_squelch_valueChanged(int value)
{
    m_settings.m_squelch = value;
    ui->squelchText->setText(QString("%1 dB").arg(value));
    applySettings({"squelch"});
}

void SELCALDemodGUI::on_filter_textChanged(const QString& text)
{
    m_settings.m_filter = text;
    filter();
    applySettings({"filter"});
}

void SELCALDemodGUI::on_clearTable_clicked()
{
    ui->messages->setRowCount(0);
}

void SELCALDemodGUI::on_logEnable_toggled(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySettings({"logEnabled"});
}

void SELCALDemodGUI::on_logFilename_clicked()
{
    QFileDialog dialog(this, "Select file to log received calls to", m_settings.m_logFilename, "CSV files (*.csv)");
    dialog.setDefaultSuffix("csv");
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    if (dialog.exec() && !dialog.selectedFiles().isEmpty())
    {
        m_settings.m_logFilename = dialog.selectedFiles().first();
        ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
        applySettings({"logFilename"});
    }
}

void SELCALDemodGUI::on_logOpen_clicked()
{
    QString filename = QFileDialog::getOpenFileName(this, "Select CSV log to read", m_settings.m_logFilename, "CSV files (*.csv)");
    if (!filename.isEmpty()) {
        readLog(filename);
    }
}

void SELCALDemodGUI::on_udpEnabled_toggled(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings({"udpEnabled"});
}

void SELCALDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings({"udpAddress"});
}

void SELCALDemodGUI::on_udpPort_valueChanged(int value)
{
    m_settings.m_udpPort = value;
    applySettings({"udpPort"});
}

void SELCALDemodGUI::messages_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) logicalIndex;
    (void) oldVisualIndex;
    (void) newVisualIndex;

    if (!m_doApplySettings) {
        return;
    }
    for (int i = 0; i < SELCALDemodSettings::m_columns; ++i) {
        m_settings.m_columnIndexes[i] = ui->messages->horizontalHeader()->visualIndex(i);
    }
}

void SELCALDemodGUI::messages_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    if (m_doApplySettings) {
        m_settings.m_columnSizes[logicalIndex] = newSize;
    }
}

void SELCALDemodGUI::messageReceived(const SELCALMessages::Call& call, bool update)
{
    ui->lastCode->setText(call.m_code);
    ui->lastCode->setToolTip(call.m_dateTime.toString(Qt::ISODateWithMs));
    int row = ui->messages->rowCount();
    ui->messages->insertRow(row);
    for (int col = 0; col < SELCALDemodSettings::m_columns; ++col) {
        ui->messages->setItem(row, col, new QTableWidgetItem());
    }
    // Values are displayed in kHz, but exported in Hz.
    auto set = [this, row](int col, const QString& text, const QString& exported) {
        ui->messages->item(row, col)->setText(text);
        ui->messages->item(row, col)->setData(Qt::UserRole, exported);
    };
    QString utc = call.m_dateTime.toString(Qt::ISODateWithMs);
    QString rssi = QString::number(call.m_rssi, 'f', 1);
    QString snr = QString::number(call.m_snr, 'f', 1);
    set(COL_UTC, utc, utc);
    set(COL_CODE, call.m_code, call.m_code);
    set(COL_FREQUENCY, QString::number(call.m_frequency / 1000.0, 'f', 3), QString::number(call.m_frequency));
    set(COL_RSSI, rssi, rssi);
    set(COL_SNR, snr, snr);
    lookupAircraft(row);
    if (update)
    {
        filter();
        ui->messages->scrollToBottom();
    }
}

void SELCALDemodGUI::filter()
{
    QString query = m_settings.m_filter.trimmed().toUpper().remove('-');
    for (int row = 0; row < ui->messages->rowCount(); ++row)
    {
        QString code = ui->messages->item(row, COL_CODE)->text().remove('-');
        ui->messages->setRowHidden(row, !code.contains(query));
    }
}

void SELCALDemodGUI::handleInputMessages()
{
    while (auto *message = m_inputMessageQueue.pop())
    {
        if (SELCALDemod::MsgConfigureSELCALDemod::match(*message))
        {
            const auto& config = static_cast<const SELCALDemod::MsgConfigureSELCALDemod&>(*message);
            if (config.getForce())
            {
                // The layout of this window isn't sent to the channel when changed
                SELCALDemodSettings settings = config.getSettings();
                settings.m_columnIndexes = m_settings.m_columnIndexes;
                settings.m_columnSizes = m_settings.m_columnSizes;
                settings.m_workspaceIndex = m_settings.m_workspaceIndex;
                settings.m_geometryBytes = m_settings.m_geometryBytes;
                settings.m_hidden = m_settings.m_hidden;
                m_settings = settings;
            }
            else
            {
                m_settings.applySettings(config.getSettingsKeys(), config.getSettings());
            }
            // Serializable pointers always belong to this GUI.
            m_settings.setChannelMarker(&m_channelMarker);
            m_settings.setRollupState(&m_rollupState);
            m_settings.setSpectrumGUI(ui->spectrumGUI);
            displaySettings();
        }
        else if (DSPSignalNotification::match(*message))
        {
            const auto& notification = static_cast<const DSPSignalNotification&>(*message);
            m_deviceCenterFrequency = notification.getCenterFrequency();
            m_basebandSampleRate = notification.getSampleRate();
            ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate / 2, m_basebandSampleRate / 2);
            updateAbsoluteCenterFrequency();
        }
        else if (SELCALMessages::MsgCall::match(*message)) {
            messageReceived(static_cast<const SELCALMessages::MsgCall&>(*message).getMessage());
        }
        else if (SELCALMessages::MsgStatus::match(*message))
        {
            const auto& status = static_cast<const SELCALMessages::MsgStatus&>(*message).getStatus();
            m_squelchOpen = status.m_squelchOpen;
            updateAudioMute();
            updateCarrierStatus(status);
            updateToneMarkers(status.m_toneFrequencyOffset, status.m_pair);
            if (!ui->diagnosticsContainer->isHidden())
            {
                for (int i = 0; i < SELCALDecoder::m_toneCount; ++i) {
                    ui->toneLevels->item(i, 2)->setText(QString::number(status.m_levels[i], 'f', 1));
                }
            }
        }
        delete message;
    }
}

QString SELCALDemodGUI::signedHz(double value)
{
    return QString("%1%2 Hz").arg(value >= 0.05 ? "+" : "").arg(value, 0, 'f', 1);
}

void SELCALDemodGUI::updateCarrierStatus(const SELCALMessages::Status& status)
{
    QString tones = tr("tones %1").arg(signedHz(status.m_toneFrequencyOffset));
    if (m_settings.m_modulation == SELCALDemodSettings::AM)
    {
        ui->carrierStatus->setText(status.m_carrierLocked ? tr("Coherent AM, carrier %1").arg(signedHz(status.m_carrierFrequencyOffset)) :
                                                            tr("Envelope AM, no carrier lock"));
    }
    else if (!m_settings.m_autoToneFrequencyOffset) {
        ui->carrierStatus->setText(tr("Manual, %1").arg(tones));
    }
    else if (status.m_carrierLocked) {
        ui->carrierStatus->setText(tr("Carrier %1, %2").arg(signedHz(status.m_carrierFrequencyOffset)).arg(tones));
    }
    else {
        ui->carrierStatus->setText(tr("No carrier, %1").arg(tones));
    }
}

void SELCALDemodGUI::updateToneMarkers(double offset, const SELCALDecoder::Pair& pair)
{
    // GLSpectrumGUI replaces the markers when its settings change, including
    // with markers saved in a preset, so restore them.
    const auto& shown = ui->glSpectrum->getAnnotationMarkers();
    bool replaced = shown.size() != m_markers.size();
    for (int i = 0; !replaced && i < shown.size(); ++i)
    {
        replaced = shown[i].m_startFrequency != m_markers[i].m_startFrequency || shown[i].m_markerColor != m_markers[i].m_markerColor ||
                   shown[i].m_show != m_markers[i].m_show || shown[i].m_text != m_markers[i].m_text;
    }
    if (!replaced && std::abs(offset - m_markerOffset) < 0.5 && pair == m_markerPair) {
        return;
    }
    m_markerOffset = offset;
    m_markerPair = pair;
    QList<SpectrumAnnotationMarker> markers;
    for (int i = 0; i < SELCALDecoder::m_toneCount; ++i)
    {
        const auto& tone = SELCALDecoder::tones()[i];
        bool detected = i == pair[0] || i == pair[1];
        markers.append(SpectrumAnnotationMarker(std::llround(tone.m_frequency + offset), 0, detected ? QColor(0, 255, 0) : QColor(160, 160, 160),
                                                detected ? SpectrumAnnotationMarker::ShowFull : SpectrumAnnotationMarker::ShowTop,
                                                QString(QChar(tone.m_symbol))));
    }
    ui->glSpectrum->setAnnotationMarkers(markers);
    m_markers = markers;
}

void SELCALDemodGUI::tick()
{
    double avg, peak;
    int count;
    m_selcalDemod->getMagSqLevels(avg, peak, count);
    double power = CalcDb::dbPower(avg);
    ui->channelPower->setText(QString::number(power, 'f', 1) + " dB");
    ui->channelPowerMeter->levelChanged((100 + power) / 100, (100 + CalcDb::dbPower(peak)) / 100, count);
}

void SELCALDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

void SELCALDemodGUI::channelMarkerChangedByCursor()
{
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    applySettings({"inputFrequencyOffset"});
}

void SELCALDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void SELCALDemodGUI::onWidgetRolled(QWidget *widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    // Rollups change while the saved state is being restored
    if (!m_doApplySettings) {
        return;
    }

    getRollupContents()->saveState(m_rollupState);
    applySettings({"rollupState"});
}

void SELCALDemodGUI::audioSelect(const QPoint& point)
{
    AudioSelectDialog dialog(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    dialog.move(point);
    new DialogPositioner(&dialog, false);
    dialog.exec();
    if (dialog.m_selected)
    {
        m_settings.m_audioDeviceName = dialog.m_audioDeviceName;
        updateAudioMute();
        applySettings({"audioDeviceName"});
    }
}

void SELCALDemodGUI::updateAudioMute()
{
    ui->audioMute->setToolTip(tr("Mute/unmute audio. Right click to select audio device: %1").arg(m_settings.m_audioDeviceName));
    if (m_squelchOpen) {
        ui->audioMute->setColor(QColor(0, 128, 0));
    }
    else {
        ui->audioMute->resetColor();
    }
}

void SELCALDemodGUI::readLog(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "SELCAL log", file.errorString());
        return;
    }
    QTextStream stream(&file);
    QString error;
    auto columns = CSV::readHeader(stream, {"UTC", "Code", "FrequencyHz", "RSSI", "SNR"}, error);
    if (!error.isEmpty())
    {
        QMessageBox::warning(this, "SELCAL log", error);
        return;
    }
    int lastColumn = std::max({columns["UTC"], columns["Code"], columns["FrequencyHz"], columns["RSSI"], columns["SNR"]});
    QStringList values;
    ui->messages->setUpdatesEnabled(false);
    while (CSV::readRow(stream, &values))
    {
        if (values.size() <= lastColumn) {
            continue;
        }
        SELCALMessages::Call call;
        call.m_dateTime = QDateTime::fromString(values[columns["UTC"]], Qt::ISODateWithMs);
        call.m_code = values[columns["Code"]];
        bool frequencyOK, rssiOK, snrOK;
        call.m_frequency = values[columns["FrequencyHz"]].toLongLong(&frequencyOK);
        call.m_rssi = values[columns["RSSI"]].toFloat(&rssiOK);
        call.m_snr = values[columns["SNR"]].toFloat(&snrOK);
        if (call.m_dateTime.isValid() && !call.m_code.isEmpty() && frequencyOK && rssiOK && snrOK &&
            std::isfinite(call.m_rssi) && std::isfinite(call.m_snr)) {
            messageReceived(call, false);
        }
    }
    filter();
    ui->messages->scrollToBottom();
    ui->messages->setUpdatesEnabled(true);
}

void SELCALDemodGUI::messages_customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item = ui->messages->itemAt(pos);
    if (!item) {
        return;
    }
    // Image cells have no text, so copy what their tool tip describes
    m_cellText = item->text().isEmpty() ? item->toolTip() : item->text();
    if (m_cellText.isEmpty()) {
        return;
    }
    QMenu *menu = new QMenu(ui->messages);
    connect(menu, &QMenu::aboutToHide, menu, &QMenu::deleteLater);
    QAction *copyAction = menu->addAction("Copy");
    connect(copyAction, &QAction::triggered, this, &SELCALDemodGUI::copyCell);
    menu->popup(ui->messages->viewport()->mapToGlobal(pos));
}

void SELCALDemodGUI::copyCell()
{
    QGuiApplication::clipboard()->setText(m_cellText);
}

void SELCALDemodGUI::messages_columnSelectMenu(QPoint pos)
{
    m_columnMenu->popup(ui->messages->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hiding a column resizes it to zero, which is saved in the settings.
void SELCALDemodGUI::messages_columnSelectMenuChecked(bool checked)
{
    auto *action = qobject_cast<QAction *>(sender());
    if (action) {
        ui->messages->setColumnHidden(action->data().toInt(), !checked);
    }
}

void SELCALDemodGUI::copySelection()
{
    QStringList lines;
    auto rows = ui->messages->selectionModel()->selectedRows();
    std::sort(rows.begin(), rows.end(), [](const QModelIndex& a, const QModelIndex& b) {
        return a.row() < b.row();
    });
    // Columns shown, in the order shown
    QList<int> columns;
    auto *header = ui->messages->horizontalHeader();
    for (int visual = 0; visual < SELCALDemodSettings::m_columns; ++visual)
    {
        int col = header->logicalIndex(visual);
        // Icons have no text
        if (!header->isSectionHidden(col) && (col != COL_SIDEVIEW) && (col != COL_COUNTRY)) {
            columns.append(col);
        }
    }
    for (const auto& index : rows)
    {
        QStringList values;
        for (int col : columns) {
            values << ui->messages->item(index.row(), col)->text();
        }
        lines << values.join('\t');
    }
    QApplication::clipboard()->setText(lines.isEmpty() ? ui->lastCode->text() : lines.join('\n'));
}

void SELCALDemodGUI::exportCsv()
{
    QString filename = QFileDialog::getSaveFileName(this, "Export visible SELCAL calls", "selcal.csv", "CSV files (*.csv)");
    if (filename.isEmpty()) {
        return;
    }
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "SELCAL export", file.errorString());
        return;
    }
    QTextStream stream(&file);
    stream << "UTC,Code,FrequencyHz,RSSI,SNR\n";
    for (int row = 0; row < ui->messages->rowCount(); ++row)
    {
        if (ui->messages->isRowHidden(row)) {
            continue;
        }
        // Same format as the CSV log
        QStringList values;
        for (int col : {COL_UTC, COL_CODE, COL_FREQUENCY, COL_RSSI, COL_SNR}) {
            values << ui->messages->item(row, col)->data(Qt::UserRole).toString();
        }
        stream << values.join(',') << '\n';
    }
}

// OsnDB caches the aircraft database, and reads it again if it has since been
// downloaded, e.g. by the ADS-B demodulator. It's large, so it's first read
// when the channel is created, rather than when a call is received.
void SELCALDemodGUI::loadAircraftInformation()
{
    m_aircraftInfoByReg = OsnDB::getAircraftInformationByReg();
}

void SELCALDemodGUI::lookupAircraft(int row)
{
    loadAircraftInformation();

    // Codes are not unique, so there can be several aircraft
    QStringList registrations = SELCALDB::getRegistrations(ui->messages->item(row, COL_CODE)->text());
    QStringList types;
    QStringList details;
    const AircraftInformation *first = nullptr;
    bool known = false;

    for (const auto& registration : registrations)
    {
        const AircraftInformation *info = m_aircraftInfoByReg ? m_aircraftInfoByReg->value(registration) : nullptr;
        if (info && !info->m_type.isEmpty())
        {
            types.append(info->m_type);
            known = true;
        }
        else
        {
            types.append("?");
        }
        if (info)
        {
            details.append(QString("%1: %2 %3, %4").arg(registration).arg(info->m_manufacturerName).arg(info->m_model).arg(info->m_operator));
            if (!first) {
                first = info;
            }
        }
        else
        {
            details.append(registration);
        }
    }

    QTableWidgetItem *registrationItem = ui->messages->item(row, COL_REGISTRATION);
    QTableWidgetItem *typeItem = ui->messages->item(row, COL_TYPE);
    QTableWidgetItem *sideviewItem = ui->messages->item(row, COL_SIDEVIEW);
    QTableWidgetItem *countryItem = ui->messages->item(row, COL_COUNTRY);
    QString tooltip = details.join("\n");
    registrationItem->setText(registrations.join(", "));
    registrationItem->setToolTip(tooltip);
    typeItem->setText(known ? types.join(", ") : QString());
    typeItem->setToolTip(tooltip);
    sideviewItem->setIcon(QIcon());
    sideviewItem->setToolTip(QString());
    countryItem->setIcon(QIcon());
    countryItem->setToolTip(QString());

    if (registrations.isEmpty()) {
        return;
    }

    // Icons are for the first of the aircraft that is in the aircraft database
    if (first)
    {
        QIcon *icon = AircraftInformation::getSideviewIcon(first->m_registration, first->m_operatorICAO, first->m_type);
        if (icon)
        {
            sideviewItem->setSizeHint(QSize(85, 20));
            sideviewItem->setIcon(*icon);
            sideviewItem->setToolTip(first->m_registration);
        }
    }

    // A flag can be found from the registration prefix alone
    AircraftInformation flagInfo;
    if (first) {
        flagInfo = *first;
    } else {
        flagInfo.m_registration = registrations.first();
    }
    QString flag = flagInfo.getFlag();
    if (!flag.isEmpty())
    {
        QIcon *icon = AircraftInformation::getFlagIcon(flag);
        if (icon)
        {
            countryItem->setSizeHint(QSize(40, 20));
            countryItem->setIcon(*icon);
            countryItem->setToolTip(flag);
        }
    }
}

void SELCALDemodGUI::updateAircraft()
{
    for (int row = 0; row < ui->messages->rowCount(); ++row) {
        lookupAircraft(row);
    }
}

void SELCALDemodGUI::on_getSELCALDB_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog) {
        return;
    }

    auto download = [this](bool aircraftDB) {
        m_downloadAircraftDB = aircraftDB;
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_selcalDB.downloadSELCALInformation();
    };

    // Aircraft types and icons also need the aircraft database
    loadAircraftInformation();
    if (m_aircraftInfoByReg)
    {
        download(false);
    }
    else
    {
        MessageDialog::question(this, "SELCAL",
            "Also download the aircraft database (45MB), to display aircraft types and images?",
            QMessageBox::Yes | QMessageBox::No, [download](QMessageBox::StandardButton button) {
                download(button == QMessageBox::Yes);
            });
    }
}

void SELCALDemodGUI::downloadingURL(const QString& url)
{
    if (m_progressDialog)
    {
        m_progressDialog->setLabelText(QString("Downloading %1.").arg(url));
        m_progressDialog->setValue(m_progressDialog->value() + 1);
    }
}

void SELCALDemodGUI::downloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    if (m_progressDialog)
    {
        m_progressDialog->setMaximum(totalBytes);
        m_progressDialog->setValue(bytesRead);
    }
}

void SELCALDemodGUI::downloadError(const QString& error)
{
    MessageDialog::critical(this, "SELCAL", error);
    finishDownload();
}

void SELCALDemodGUI::downloadSELCALInformationFinished()
{
    if (m_downloadAircraftDB) {
        m_osnDB.downloadAircraftInformation();
    }
    else
    {
        finishDownload();
        updateAircraft();
    }
}

void SELCALDemodGUI::downloadAircraftInformationFinished()
{
    if (m_progressDialog)
    {
        m_progressDialog->setLabelText("Reading aircraft information.");
        QApplication::processEvents();
    }
    loadAircraftInformation();
    finishDownload();
    updateAircraft();
}

void SELCALDemodGUI::finishDownload()
{
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void SELCALDemodGUI::onMenuDialogCalled(const QPoint& point)
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
            dialog.setNumberOfStreams(m_selcalDemod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }
        dialog.move(point);
        new DialogPositioner(&dialog, false);
        dialog.exec();
        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();
        if (m_deviceUISet->m_deviceMIMOEngine) {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
        }
        displaySettings();
        applySettings({"rgbColor", "title", "useReverseAPI", "reverseAPIAddress", "reverseAPIPort", "reverseAPIDeviceIndex",
                       "reverseAPIChannelIndex", "streamIndex"});
    }
    resetContextMenuType();
}
