///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QPointer>
#include <QDialog>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QScreen>

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "dsp/dspengine.h"
#include "util/db.h"
#include "maincore.h"

#include "ui_denoisergui.h"
#include "denoiser.h"
#include "denoisergui.h"

DenoiserGUI* DenoiserGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
	DenoiserGUI* gui = new DenoiserGUI(pluginAPI, featureUISet, feature);
	return gui;
}

void DenoiserGUI::destroy()
{
	delete this;
}

void DenoiserGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	updateChannelList(true);
	applySettings(true);
}

QByteArray DenoiserGUI::serialize() const
{
    return m_settings.serialize();
}

bool DenoiserGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        displaySettings();
        updateChannelList(true);
        applySettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool DenoiserGUI::handleMessage(const Message& message)
{
    if (Denoiser::MsgConfigureDenoiser::match(message))
    {
        qDebug("DenoiserGUI::handleMessage: Denoiser::MsgConfigureDenoiser");
        const Denoiser::MsgConfigureDenoiser& cfg = (Denoiser::MsgConfigureDenoiser&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        const auto requestedType = m_settings.m_denoiserType;
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        if (cfg.getForce() || cfg.getSettingsKeys().contains("selectedSource")) {
            updateChannelList(true);
        }
        if (m_settings.m_denoiserType != requestedType)
        {
            m_settingsKeys.append("denoiserType");
            applySettings();
        }

        return true;
    }
    else if (Denoiser::MsgReportChannels::match(message))
    {
        qDebug("DenoiserGUI::handleMessage: Denoiser::MsgReportChannels");
        Denoiser::MsgReportChannels& report = (Denoiser::MsgReportChannels&) message;
        m_availableChannels = report.getAvailableChannels();
        m_selectedChannel = report.getSelectedChannel();
        for (int i = 0; i < report.getRenameFrom().size() && i < report.getRenameTo().size(); ++i)
        {
            if (m_settings.m_selectedSource == report.getRenameFrom().at(i)) {
                m_settings.m_selectedSource = report.getRenameTo().at(i);
            }
        }
        updateChannelList(report.getAutoSelect());

        return true;
    }
    else if (Denoiser::MsgReportSampleRate::match(message))
    {
        Denoiser::MsgReportSampleRate& report = (Denoiser::MsgReportSampleRate&) message;
        int sampleRate = report.getSampleRate();
        qDebug("DenoiserGUI::handleMessage: Denoiser::MsgReportSampleRate: %d", sampleRate);
        displaySampleRate(sampleRate);
        m_sampleRate = sampleRate;

        return true;
    }

	return false;
}

void DenoiserGUI::handleInputMessages()
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

void DenoiserGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    RollupContents *rollupContents = getRollupContents();

    rollupContents->saveState(m_rollupState);
    applySettings();
}

DenoiserGUI::DenoiserGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
	FeatureGUI(parent),
	ui(new Ui::DenoiserGUI),
	m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_sampleRate(0),
	m_doApplySettings(true),
    m_selectedChannel(nullptr)
{
    m_feature = feature;
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/denoiser/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    using DenoiserType = DenoiserSettings::DenoiserType;
    ui->denoiserType->setItemData(0, static_cast<int>(DenoiserType::DenoiserType_None));
    ui->denoiserType->setItemData(1, static_cast<int>(DenoiserType::DenoiserType_RNnoise));
    ui->denoiserType->setItemData(2, static_cast<int>(DenoiserType::DenoiserType_Nvidia));
    ui->denoiserType->setItemData(3, static_cast<int>(DenoiserType::DenoiserType_Vst3));
#if !defined(Q_OS_WIN) && !defined(Q_OS_LINUX)
    ui->denoiserType->removeItem(static_cast<int>(DenoiserSettings::DenoiserType::DenoiserType_Nvidia));
#endif
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_denoiser = reinterpret_cast<Denoiser*>(feature);
    m_denoiser->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
	connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

	connect(m_denoiser, &Feature::stateChanged, this, &DenoiserGUI::updateFeatureState);
	updateFeatureState();

    displaySampleRate(m_sampleRate);

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_settings.setRollupState(&m_rollupState);

    displaySettings();
	applySettings(true);
    makeUIConnections();
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
    m_denoiser->getAvailableChannelsReport();
    m_denoiser->setLevelMeter(ui->volumeMeter);
}

DenoiserGUI::~DenoiserGUI()
{
	if (m_vst3ScanProcess) 
    {
        QProcess *process = m_vst3ScanProcess;
        m_vst3ScanProcess = nullptr;
        QObject::disconnect(process, nullptr, this, nullptr);
        process->kill();
        process->waitForFinished(1000);
    }
	delete ui;
}

void DenoiserGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void DenoiserGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void DenoiserGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->record->setChecked(m_settings.m_recordToFile);
    ui->fileNameText->setText(m_settings.m_fileRecordName);
    ui->showFileDialog->setEnabled(!m_settings.m_recordToFile);
    int typeIndex = ui->denoiserType->findData(static_cast<int>(m_settings.m_denoiserType));
    if (typeIndex < 0)
    {
        m_settings.m_denoiserType = DenoiserSettings::DenoiserType::DenoiserType_RNnoise;
        typeIndex = ui->denoiserType->findData(static_cast<int>(m_settings.m_denoiserType));
    }
    ui->denoiserType->setCurrentIndex(typeIndex);
    updateControls();
    displayVst3Selection();
    ui->nvidiaIntensity->setValue(m_settings.m_nvidiaIntensity);
    ui->nvidiaIntensityText->setText(QStringLiteral("%1%").arg(m_settings.m_nvidiaIntensity));
    ui->nvidiaVad->setChecked(m_settings.m_nvidiaVad);
    ui->enable->setChecked(m_settings.m_enableDenoiser);
    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->volume->setValue(m_settings.m_volumeTenths);
    ui->volumeText->setText(QString::number(m_settings.m_volumeTenths / 10.0, 'f', 1));
    displayNRenabled();
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void DenoiserGUI::displaySampleRate(int sampleRate)
{
	if (sampleRate <= 0) 
    {
		ui->sinkSampleRateText->setText(tr("-- kS/s"));
		ui->sinkSampleRateText->setToolTip(tr("Waiting for the selected channel's sample rate"));
		return;
	}
	QString s = QString::number(sampleRate/1000.0, 'f', 1);
	ui->sinkSampleRateText->setText(tr("%1 kS/s").arg(s));
	ui->sinkSampleRateText->setToolTip(QString());
}

void DenoiserGUI::updateChannelList(bool autoSelect)
{
    {
        const QSignalBlocker blocker(ui->channels);
        ui->channels->clear();

        for (const auto& source : m_availableChannels) {
            ui->channels->addItem(source.getLongId());
        }

        int selectedItem = -1;
        if (autoSelect)
        {
            if (!m_settings.m_selectedSource.isEmpty()) 
            {
                selectedItem = m_availableChannels.indexOfLongId(m_settings.m_selectedSource);
            } 
            else 
            {
                selectedItem = m_availableChannels.indexOfObject(m_selectedChannel);
                if (selectedItem < 0 && !m_availableChannels.isEmpty()) {
                    selectedItem = 0;
                }
            }
        }
        else
        {
            selectedItem = m_availableChannels.indexOfObject(m_selectedChannel);
            if (selectedItem < 0) {
                m_settings.m_selectedSource.clear();
            }
        }
        ui->channels->setCurrentIndex(selectedItem);
    }

    // Adding the first item selects it while signals are blocked. Send the
    // selection explicitly so the feature registers its data and rate pipes.
    if (ui->channels->currentIndex() >= 0) {
        on_channels_currentIndexChanged(ui->channels->currentIndex());
    }
}

void DenoiserGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
    {
        BasicFeatureSettingsDialog dialog(this);
        dialog.setTitle(m_settings.m_title);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIFeatureSetIndex(m_settings.m_reverseAPIFeatureSetIndex);
        dialog.setReverseAPIFeatureIndex(m_settings.m_reverseAPIFeatureIndex);
        dialog.setDefaultTitle(m_displayedName);

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        m_settingsKeys.append("title");
        m_settingsKeys.append("rgbColor");
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIFeatureSetIndex");
        m_settingsKeys.append("reverseAPIFeatureIndex");

        applySettings();
    }

    resetContextMenuType();
}

void DenoiserGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        if (checked && !m_vst3ScanAttempted) {
            scanVst3Plugins({}, true);
        }
        Denoiser::MsgStartStop *message = Denoiser::MsgStartStop::create(checked);
        m_denoiser->getInputMessageQueue()->push(message);

        if (checked && (ui->channels->count() > 0)) {
            on_channels_currentIndexChanged(ui->channels->currentIndex());
        }
    }
}

void DenoiserGUI::on_channels_currentIndexChanged(int index)
{
    if ((index >= 0) && (index < m_availableChannels.size()))
    {
        m_selectedChannel = m_availableChannels[index].m_object;
        m_settings.m_selectedSource = m_availableChannels[index].getLongId();
        Denoiser::MsgSelectChannel *msg = Denoiser::MsgSelectChannel::create(m_selectedChannel);
        m_denoiser->getInputMessageQueue()->push(msg);
    }
}

void DenoiserGUI::on_channelApply_clicked()
{
    if (ui->channels->count() > 0) {
        on_channels_currentIndexChanged(ui->channels->currentIndex());
    }
}

void DenoiserGUI::on_record_toggled(bool checked)
{
    ui->showFileDialog->setEnabled(!checked);
    m_settings.m_recordToFile = checked;
    m_settingsKeys.append("recordToFile");
    applySettings();
}

void DenoiserGUI::on_showFileDialog_clicked(bool checked)
{
    (void) checked;
    QFileDialog fileDialog(
        this,
        tr("Save record file"),
        m_settings.m_fileRecordName,
        tr("WAV Files (*.wav)")
    );

    fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
    fileDialog.setFileMode(QFileDialog::AnyFile);
    QStringList fileNames;

    if (fileDialog.exec())
    {
        fileNames = fileDialog.selectedFiles();

        if (fileNames.size() > 0)
        {
            m_settings.m_fileRecordName = fileNames.at(0);
		    ui->fileNameText->setText(m_settings.m_fileRecordName);
            m_settingsKeys.append("fileRecordName");
            applySettings();
        }
    }
}

void DenoiserGUI::on_denoiserType_currentIndexChanged(int index)
{
    if (index < 0) {
        return;
    }
    m_settings.m_denoiserType = static_cast<DenoiserSettings::DenoiserType>(ui->denoiserType->itemData(index).toInt());
    if (m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_Vst3 && !m_vst3ScanAttempted) {
        scanVst3Plugins({}, true);
    }
    updateControls();
    m_settingsKeys.append("denoiserType");
    applySettings();
}

void DenoiserGUI::updateControls()
{
    RollupContents *rollups = getRollupContents();
    const int previousContentHeight = rollups->arrangeRollups();
    const bool showNvidia = m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_Nvidia;
    ui->nvidiaIntensityLabel->setVisible(showNvidia);
    ui->nvidiaIntensity->setVisible(showNvidia);
    ui->nvidiaIntensityText->setVisible(showNvidia);
    ui->nvidiaVad->setVisible(showNvidia);

    const bool showVst3 = m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_Vst3;
    ui->vst3Plugin->setVisible(showVst3);
    ui->vst3Refresh->setVisible(showVst3);
    ui->vst3Browse->setVisible(showVst3);
    ui->vst3Parameters->setVisible(showVst3);
    ui->settingsContainer->layout()->invalidate();
    ui->settingsContainer->layout()->activate();
    const int contentHeight = rollups->arrangeRollups();
    layout()->activate();
    sizeToContents();

    if (isVisible() && !isMaximized()) {
        resize(qMax(width(), minimumWidth()), qMax(height() + contentHeight - previousContentHeight, minimumHeight()));
    }
}

void DenoiserGUI::displayVst3Selection()
{
    const QSignalBlocker blocker(ui->vst3Plugin);
    ui->vst3Plugin->clear();
    int selected = -1;
    for (const Vst3PluginInfo& plugin : m_vst3Plugins) 
    {
        const int index = ui->vst3Plugin->count();
        ui->vst3Plugin->addItem(plugin.name);
        ui->vst3Plugin->setItemData(index, plugin.modulePath, Qt::ToolTipRole);
        if (plugin.modulePath == m_settings.m_vst3ModulePath && plugin.classId == m_settings.m_vst3ClassId) {
            selected = index;
        }
    }
    if (selected < 0 && !m_settings.m_vst3ModulePath.isEmpty()) 
    {
        selected = ui->vst3Plugin->count();
        ui->vst3Plugin->addItem(QFileInfo(m_settings.m_vst3ModulePath).fileName() + tr(" (saved)"));
        ui->vst3Plugin->setItemData(selected, m_settings.m_vst3ModulePath, Qt::ToolTipRole);
    }
    if (selected >= 0) {
        ui->vst3Plugin->setCurrentIndex(selected);
    } else {
        ui->vst3Plugin->setCurrentIndex(-1);
    }
}

void DenoiserGUI::scanVst3Plugins(const QStringList& paths, bool automatic)
{
    if (m_vst3ScanProcess) {
        return;
    }
    if (paths.isEmpty()) {
        m_vst3ScanAttempted = true;
    }
    auto *process = new QProcess(this);
    m_vst3ScanProcess = process;
    ui->vst3Refresh->setText(tr("Scanning..."));
    ui->vst3Refresh->setToolTip(QString());
    ui->vst3Refresh->setEnabled(false);
    ui->vst3Browse->setEnabled(false);
    const QString scanner = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
        QStringLiteral("sdrangel-vst3-scan.exe")
#else
        QStringLiteral("sdrangel-vst3-scan")
#endif
    );
    process->setProgram(scanner);
    process->setArguments(paths);
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this, process, automatic](int exitCode, QProcess::ExitStatus status) {
            if (m_vst3ScanProcess != process) return;
            const QJsonDocument document = QJsonDocument::fromJson(process->readAllStandardOutput());
            if (status != QProcess::NormalExit || exitCode != 0 || !document.isArray()) 
            {
                const QString error = tr("The VST3 scanner failed. A plugin may have crashed during discovery.");
                if (automatic) {
                    ui->vst3Refresh->setToolTip(error);
                } else {
                    QMessageBox::warning(this, tr("VST3 scan"), error);
                }
                displayVst3Selection();
            } 
            else 
            {
                for (const QJsonValue& value : document.array()) 
                {
                    const QJsonObject object = value.toObject();
                    Vst3PluginInfo plugin;
                    plugin.modulePath = object.value(QStringLiteral("path")).toString();
                    plugin.classId = QByteArray::fromHex(object.value(QStringLiteral("id")).toString().toLatin1());
                    plugin.name = object.value(QStringLiteral("name")).toString();
                    if (plugin.modulePath.isEmpty() || plugin.classId.size() != 16) continue;
                    bool duplicate = false;
                    for (const Vst3PluginInfo& known : m_vst3Plugins) 
                    {
                        if (known.modulePath == plugin.modulePath && known.classId == plugin.classId) 
                        {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate) {
                        m_vst3Plugins.append(plugin);
                    }
                }
                displayVst3Selection();
                if (m_vst3Plugins.isEmpty()) 
                {
                    const QString message = tr("No VST3 audio effects were found.");
                    if (automatic) { 
                        ui->vst3Refresh->setToolTip(message);
                    } else {
                        QMessageBox::information(this, tr("VST3 effects"), message);
                    }
                }
            }
            ui->vst3Refresh->setText(tr("Scan"));
            ui->vst3Refresh->setEnabled(true);
            ui->vst3Browse->setEnabled(true);
            m_vst3ScanProcess = nullptr;
            process->deleteLater();
        });
    connect(process, &QProcess::errorOccurred, this, [this, process, automatic](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_vst3ScanProcess != process) {
            return;
        }
        const QString message = tr("Could not launch the VST3 scanner: %1").arg(process->errorString());
        if (automatic) {
            ui->vst3Refresh->setToolTip(message);
        } else {
            QMessageBox::warning(this, tr("VST3 scan"), message);
        }
        displayVst3Selection();
        ui->vst3Refresh->setText(tr("Scan"));
        ui->vst3Refresh->setEnabled(true);
        ui->vst3Browse->setEnabled(true);
        m_vst3ScanProcess = nullptr;
        process->deleteLater();
    });
    process->start();
    QPointer<QProcess> guarded(process);
    QTimer::singleShot(300000, this, [guarded]() {
        if (guarded && guarded->state() != QProcess::NotRunning) guarded->kill();
    });
}

void DenoiserGUI::on_vst3Plugin_currentIndexChanged(int index)
{
    if (index < 0 || index >= m_vst3Plugins.size()) {
        return;
    }
    const Vst3PluginInfo& plugin = m_vst3Plugins[index];
    m_settings.m_vst3ModulePath = plugin.modulePath;
    m_settings.m_vst3ClassId = plugin.classId;
    m_settings.m_vst3Parameters.clear();
    m_settings.m_vst3State.clear();
    m_settingsKeys.append("vst3ModulePath");
    m_settingsKeys.append("vst3ClassId");
    m_settingsKeys.append("vst3Parameters");
    m_settingsKeys.append("vst3State");
    applySettings();
}

void DenoiserGUI::on_vst3Refresh_clicked()
{
    m_vst3Plugins.clear();
    scanVst3Plugins();
}

void DenoiserGUI::on_vst3Browse_clicked()
{
#ifdef Q_OS_WIN
    const QString path = QFileDialog::getOpenFileName(this, tr("Select VST3 effect"), QString(), tr("VST3 effects (*.vst3)"));
#else
    const QString path = QFileDialog::getExistingDirectory(this, tr("Select VST3 bundle"));
#endif
    if (!path.isEmpty()) {
        scanVst3Plugins({path});
    }
}

void DenoiserGUI::on_vst3Parameters_clicked()
{
    if (m_settings.m_vst3ModulePath.isEmpty() || m_settings.m_vst3ClassId.size() != 16) 
    {
        QMessageBox::information(this, tr("VST3 parameters"),
            tr("Select a VST3 effect from the list first."));
        return;
    }

    // Keep the editor instance independent of the audio worker. Audio format or
    // channel changes can then recreate the processing instance safely.
    auto parameterEffect = std::make_unique<Vst3Effect>();
    QString error;
    if (!parameterEffect->open(m_settings.m_vst3ModulePath, m_settings.m_vst3ClassId,
            m_sampleRate > 0 ? m_sampleRate : 48000, 2, error, m_settings.m_vst3State))
    {
        QMessageBox::warning(this, tr("VST3 parameters"),
            tr("Could not load the selected VST3 effect: %1").arg(error));
        return;
    }
    for (auto it = m_settings.m_vst3Parameters.cbegin(); it != m_settings.m_vst3Parameters.cend(); ++it) {
        parameterEffect->setParameter(it.key(), it.value());
    }
    parameterEffect->setParameterEditCallback([this](quint32 id, double value) {
        if (m_settings.m_vst3Parameters.value(id, -1.0) == value) {
            return;
        }
        m_settings.m_vst3Parameters.insert(id, value);
        m_settingsKeys.append("vst3Parameters");
        applySettings();
    });

    QDialog dialog(this);
    dialog.setWindowTitle(tr("VST3 parameters"));
    auto *outer = new QVBoxLayout(&dialog);
    if (!m_denoiser->hasVst3Effect()) 
    {
        auto *notice = new QLabel(tr("Live processing was unavailable when this dialog opened. Changes are saved and applied when the effect loads."), &dialog);
        notice->setWordWrap(true);
        outer->addWidget(notice);
    }
    QString editorError;
    auto *nativeScroll = new QScrollArea(&dialog);
    nativeScroll->setWidgetResizable(false);
    if (QWidget *editor = parameterEffect->createEditorWidget(nativeScroll->viewport(), editorError)) 
    {
        nativeScroll->setWidget(editor);
        outer->addWidget(nativeScroll);
        if (QScreen *screen = dialog.screen()) 
        {
            const QRect available = screen->availableGeometry();
            dialog.resize(qMin(editor->width() + 36, available.width() - 80),
                qMin(editor->height() + 64, available.height() - 80));
        } 
        else 
        {
            dialog.adjustSize();
        }
        dialog.exec();
        saveVst3State(*parameterEffect);
        return;
    }
    delete nativeScroll;

    const QVector<Vst3ParameterInfo> parameters = parameterEffect->parameters();
    bool hasEditableParameter = false;
    for (const Vst3ParameterInfo& parameter : parameters) 
    {
        if (!parameter.readOnly && !parameter.hidden) 
        {
            hasEditableParameter = true;
            break;
        }
    }
    if (!hasEditableParameter) 
    {
        QMessageBox::information(this, tr("VST3 parameters"),
            tr("This VST3 effect has no available editor or editable parameters. %1").arg(editorError));
        return;
    }
    dialog.resize(440, 500);
    auto *scroll = new QScrollArea(&dialog);
    scroll->setWidgetResizable(true);
    outer->addWidget(scroll);
    auto *content = new QWidget(scroll);
    auto *layout = new QVBoxLayout(content);
    for (const Vst3ParameterInfo& parameter : parameters) 
    {
        if (parameter.readOnly || parameter.hidden) {
            continue;
        }
        auto *row = new QWidget(content);
        auto *rowLayout = new QHBoxLayout(row);
        auto *name = new QLabel(parameter.name, row);
        name->setMinimumWidth(130);
        auto *slider = new QSlider(Qt::Horizontal, row);
        const int maximum = parameter.steps > 0 ? qMin(parameter.steps, 10000) : 1000;
        slider->setRange(0, maximum);
        const double value = m_settings.m_vst3Parameters.value(parameter.id, parameter.currentValue);
        slider->setValue(qRound(value * maximum));
        auto *valueLabel = new QLabel(parameterEffect->parameterText(parameter.id, value), row);
        valueLabel->setMinimumWidth(70);
        valueLabel->setMaximumWidth(120);
        rowLayout->addWidget(name);
        rowLayout->addWidget(slider, 1);
        rowLayout->addWidget(valueLabel);
        layout->addWidget(row);
        connect(slider, &QSlider::valueChanged, &dialog, [this, parameter, maximum, valueLabel, effect = parameterEffect.get()](int raw) {
            const double normalized = static_cast<double>(raw) / maximum;
            effect->setParameter(parameter.id, normalized);
            valueLabel->setText(effect->parameterText(parameter.id, normalized));
            m_settings.m_vst3Parameters.insert(parameter.id, normalized);
            m_settingsKeys.append("vst3Parameters");
            applySettings();
        });
    }
    layout->addStretch();
    scroll->setWidget(content);
    dialog.exec();
    saveVst3State(*parameterEffect);
}

void DenoiserGUI::saveVst3State(Vst3Effect& effect)
{
    QString error;
    const QByteArray state = effect.state(error);
    if (state.isEmpty())
    {
        qWarning() << "DenoiserGUI::saveVst3State:" << error;
        return;
    }

    // Editor changes such as presets may bypass performEdit, so refresh the saved
    // parameter values. Otherwise they would override the state when it is restored.
    bool parametersChanged = false;
    for (auto it = m_settings.m_vst3Parameters.begin(); it != m_settings.m_vst3Parameters.end(); ++it)
    {
        const double value = effect.parameterValue(it.key());
        if (value >= 0.0 && value != it.value())
        {
            it.value() = value;
            parametersChanged = true;
        }
    }

    if (state == m_settings.m_vst3State && !parametersChanged) {
        return;
    }
    m_settings.m_vst3State = state;
    m_settingsKeys.append("vst3State");
    if (parametersChanged) {
        m_settingsKeys.append("vst3Parameters");
    }
    applySettings();
}

void DenoiserGUI::on_nvidiaIntensity_valueChanged(int value)
{
    ui->nvidiaIntensityText->setText(QStringLiteral("%1%").arg(value));
    if (ui->nvidiaIntensity->isSliderDown() || m_settings.m_nvidiaIntensity == value) {
        return;
    }
    m_settings.m_nvidiaIntensity = value;
    m_settingsKeys.append("nvidiaIntensity");
    applySettings();
}

void DenoiserGUI::on_nvidiaVad_toggled(bool checked)
{
    m_settings.m_nvidiaVad = checked;
    m_settingsKeys.append("nvidiaVad");
    applySettings();
}

void DenoiserGUI::on_enable_toggled(bool checked)
{
    m_settings.m_enableDenoiser = checked;
    displayNRenabled();
    m_settingsKeys.append("enableDenoiser");
    applySettings();
}

void DenoiserGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
    m_settingsKeys.append("audioMute");
    applySettings();
}

void DenoiserGUI::on_volume_valueChanged(int value)
{
    m_settings.m_volumeTenths = value;
    ui->volumeText->setText(QString::number(value / 10.0, 'f', 1));
    m_settingsKeys.append("volumeTenths");
    applySettings();
}

void DenoiserGUI::audioSelect(const QPoint& p)
{
    qDebug("DenoiserGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        m_settingsKeys.append("audioDeviceName");
        applySettings();
    }
}


void DenoiserGUI::tick()
{
	m_channelPowerAvg(m_denoiser->getMagSqAvg());
	double powDb = CalcDb::dbPower((double) m_channelPowerAvg);
	ui->channelPower->setText(tr("%1 dB").arg(powDb, 0, 'f', 1));
}

void DenoiserGUI::updateFeatureState()
{
    updateStartStopButton(ui->startStop);
    if (m_denoiser->getState() == Feature::StRunning && !m_vst3ScanAttempted) {
        scanVst3Plugins({}, true);
    }
}

void DenoiserGUI::displayNRenabled()
{
    if (m_settings.m_enableDenoiser) {
        ui->enable->setStyleSheet("QToolButton { background-color : green; }");
    } else {
        ui->enable->setStyleSheet("QToolButton { background-color : blue; }");
    }
}

void DenoiserGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
	    Denoiser::MsgConfigureDenoiser* message = Denoiser::MsgConfigureDenoiser::create( m_settings, m_settingsKeys, force);
	    m_denoiser->getInputMessageQueue()->push(message);
	}

    m_settingsKeys.clear();
}

void DenoiserGUI::makeUIConnections()
{
	QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &DenoiserGUI::on_startStop_toggled);
	QObject::connect(ui->channels, qOverload<int>(&QComboBox::currentIndexChanged), this, &DenoiserGUI::on_channels_currentIndexChanged);
	QObject::connect(ui->channelApply, &QPushButton::clicked, this, &DenoiserGUI::on_channelApply_clicked);
    QObject::connect(ui->record, &ButtonSwitch::toggled, this, &DenoiserGUI::on_record_toggled);
    QObject::connect(ui->showFileDialog, &QPushButton::clicked, this, &DenoiserGUI::on_showFileDialog_clicked);
    QObject::connect(ui->denoiserType, qOverload<int>(&QComboBox::currentIndexChanged), this, &DenoiserGUI::on_denoiserType_currentIndexChanged);
    QObject::connect(ui->nvidiaIntensity, &QSlider::valueChanged, this, &DenoiserGUI::on_nvidiaIntensity_valueChanged);
    QObject::connect(ui->nvidiaIntensity, &QSlider::sliderReleased, this, [this]() {
        on_nvidiaIntensity_valueChanged(ui->nvidiaIntensity->value());
    });
    QObject::connect(ui->nvidiaVad, &QCheckBox::toggled, this, &DenoiserGUI::on_nvidiaVad_toggled);
    QObject::connect(ui->vst3Plugin, qOverload<int>(&QComboBox::currentIndexChanged), this, &DenoiserGUI::on_vst3Plugin_currentIndexChanged);
    QObject::connect(ui->vst3Refresh, &QPushButton::clicked, this, &DenoiserGUI::on_vst3Refresh_clicked);
    QObject::connect(ui->vst3Browse, &QPushButton::clicked, this, &DenoiserGUI::on_vst3Browse_clicked);
    QObject::connect(ui->vst3Parameters, &QPushButton::clicked, this, &DenoiserGUI::on_vst3Parameters_clicked);
    QObject::connect(ui->enable, &ButtonSwitch::toggled, this, &DenoiserGUI::on_enable_toggled);
    QObject::connect(ui->audioMute, &ButtonSwitch::toggled, this, &DenoiserGUI::on_audioMute_toggled);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &DenoiserGUI::on_volume_valueChanged);
}
