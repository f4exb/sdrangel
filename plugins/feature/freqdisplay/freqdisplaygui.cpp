///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by Copilot                                                          //
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

#include <QColorDialog>
#include <QContextMenuEvent>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include "channel/channelwebapiutils.h"
#include "gui/buttonswitch.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "feature/featureuiset.h"

#include "ui_freqdisplaygui.h"
#include "freqdisplay.h"
#include "freqdisplaygui.h"

// --- FreqDisplayOverlay ---------------------------------------------------

FreqDisplayOverlay::FreqDisplayOverlay(QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool),
      m_dragging(false)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(false);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_label);
    setLayout(layout);
}

void FreqDisplayOverlay::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    QAction* exitAction = menu.addAction(tr("Exit transparent mode"));
    if (menu.exec(event->globalPos()) == exitAction) {
        emit exitTransparentMode();
    }
}

void FreqDisplayOverlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    emit resized();
}

void FreqDisplayOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_dragging = true;
        m_dragStartPos = event->globalPos() - pos();
    }
    QWidget::mousePressEvent(event);
}

void FreqDisplayOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - m_dragStartPos);
    }
    QWidget::mouseMoveEvent(event);
}

void FreqDisplayOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

// --- FreqDisplayGUI -------------------------------------------------------

FreqDisplayGUI* FreqDisplayGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    return new FreqDisplayGUI(pluginAPI, featureUISet, feature);
}

void FreqDisplayGUI::destroy()
{
    delete this;
}

bool FreqDisplayGUI::handleMessage(const Message& message)
{
    if (FreqDisplay::MsgConfigureFreqDisplay::match(message))
    {
        qDebug("FreqDisplayGUI::handleMessage: FreqDisplay::MsgConfigureFreqDisplay");
        const FreqDisplay::MsgConfigureFreqDisplay& cfg = (FreqDisplay::MsgConfigureFreqDisplay&) message;
        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        updateFrequencyText();
        return true;
    }

    return false;
}

void FreqDisplayGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void FreqDisplayGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applyAllSettings();
    updateFrequencyText();
}

QByteArray FreqDisplayGUI::serialize() const
{
    return m_settings.serialize();
}

bool FreqDisplayGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        displaySettings();
        applyAllSettings();
        updateFrequencyText();
        return true;
    }

    resetToDefaults();
    return false;
}

void FreqDisplayGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    RollupContents *rollupContents = getRollupContents();

    rollupContents->saveState(m_rollupState);
    applySetting("rollupState");
}

FreqDisplayGUI::FreqDisplayGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::FreqDisplayGUI),
    m_freqDisplay(reinterpret_cast<FreqDisplay*>(feature)),
    m_availableChannelOrFeatureHandler(QStringList(), m_rxTxChannelKinds),
    m_doApplySettings(true),
    m_overlay(nullptr)
{
    (void) pluginAPI;
    (void) featureUISet;

    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/freqdisplay/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    sizeToContents();

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    ui->frequencyValue->setWordWrap(false); // If true, RollupContents::arrangeRollups uses heightForWidth rather than minimumSizeHint

    m_freqDisplay->setMessageQueueToGUI(&m_inputMessageQueue);
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    connect(
        &m_availableChannelOrFeatureHandler,
        &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged,
        this,
        &FreqDisplayGUI::channelsOrFeaturesChanged
    );
    m_availableChannelOrFeatureHandler.scanAvailableChannelsAndFeatures();

    makeUIConnections();
    m_pollTimer.start(m_pollIntervalMs);

#ifndef QT_TEXTTOSPEECH_FOUND
    ui->speech->setVisible(false);
#endif

    m_settings.setRollupState(&m_rollupState);

    displaySettings();
    applyAllSettings();
    updateFrequencyText();
    m_resizer.enableChildMouseTracking();
}

FreqDisplayGUI::~FreqDisplayGUI()
{
#ifdef QT_TEXTTOSPEECH_FOUND
    if (m_speech)
    {
        disconnect(m_speech, &QTextToSpeech::stateChanged, this, &FreqDisplayGUI::speechStateChanged);
        delete m_speech;
        m_speech = nullptr;
    }
#endif
    delete m_overlay;
    delete ui;
}

void FreqDisplayGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void FreqDisplayGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void FreqDisplayGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);

    if (!m_settings.m_fontName.isEmpty()) {
        ui->fontFamily->setCurrentFont(QFont(m_settings.m_fontName));
    }
    ui->displayMode->setCurrentIndex(static_cast<int>(m_settings.m_displayMode));
    ui->speech->setChecked(m_settings.m_speechEnabled);
    ui->activeOnly->setChecked(m_settings.m_activeOnly);
    ui->transparentBackground->setChecked(m_settings.m_transparentBackground);
    ui->frequencyUnits->setCurrentIndex(static_cast<int>(m_settings.m_frequencyUnits));
    ui->showUnits->setChecked(m_settings.m_showUnits);
    ui->powerDecimalPlaces->setValue(m_settings.m_powerDecimalPlaces);

    // Must come after frequencyUnits is set so the range/enabled state is correct
    updateFreqDecimalSpinbox();

    updateTextColorButton();
    updateDropShadowColorButton();

    ui->dropShadow->setChecked(m_settings.m_dropShadowEnabled);

    blockApplySettings(false);

    applyTransparency();
    applyTextColor();
    applySpeech();
    updateChannelList();

    getRollupContents()->restoreState(m_rollupState);
    getRollupContents()->arrangeRollups();
}

void FreqDisplayGUI::onMenuDialogCalled(const QPoint &p)
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

        applySettings(m_settingsKeys);
    }

    resetContextMenuType();
}

void FreqDisplayGUI::applySetting(const QString& settingsKey)
{
    applySettings({settingsKey});
}

void FreqDisplayGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    m_settingsKeys.append(settingsKeys);
    if (m_doApplySettings)
    {
        FreqDisplay::MsgConfigureFreqDisplay *msg = FreqDisplay::MsgConfigureFreqDisplay::create(m_settings, settingsKeys, force);
        m_freqDisplay->getInputMessageQueue()->push(msg);
        m_settingsKeys.clear();
    }
}

void FreqDisplayGUI::applyAllSettings()
{
    applySettings(QStringList(), true);
}

void FreqDisplayGUI::updateChannelList()
{
    m_availableChannels = m_availableChannelOrFeatureHandler.getAvailableChannelOrFeatureList();

    ui->channels->blockSignals(true);
    ui->channels->clear();

    int selectedIndex = -1;

    for (int i = 0; i < m_availableChannels.size(); ++i)
    {
        const QString longId = m_availableChannels.at(i).getLongId();
        ui->channels->addItem(longId);

        if (longId == m_settings.m_selectedChannel) {
            selectedIndex = i;
        }
    }

    if ((selectedIndex < 0) && (m_availableChannels.size() > 0)) {
        selectedIndex = 0;
    }

    if (selectedIndex >= 0)
    {
        ui->channels->setCurrentIndex(selectedIndex);
        m_settings.m_selectedChannel = m_availableChannels.at(selectedIndex).getLongId();
    }
    else
    {
        m_settings.m_selectedChannel.clear();
    }

    ui->channels->blockSignals(false);
}

void FreqDisplayGUI::channelsOrFeaturesChanged(const QStringList& renameFrom, const QStringList& renameTo, const QStringList& removed, const QStringList& added)
{
    (void) removed;
    (void) added;

    for (int i = 0; i < renameFrom.size() && i < renameTo.size(); ++i)
    {
        if (m_settings.m_selectedChannel == renameFrom.at(i)) {
            m_settings.m_selectedChannel = renameTo.at(i);
        }
    }

    updateChannelList();
    applySetting("selectedChannel");
    updateFrequencyText();
}

void FreqDisplayGUI::on_channels_currentIndexChanged(int index)
{
    if ((index >= 0) && (index < m_availableChannels.size())) {
        m_settings.m_selectedChannel = m_availableChannels.at(index).getLongId();
    } else {
        m_settings.m_selectedChannel.clear();
    }

    applySetting("selectedChannel");
    updateFrequencyText();
}

void FreqDisplayGUI::pollSelectedChannel()
{
    updateFrequencyText();
}

void FreqDisplayGUI::updateFrequencyText()
{
    auto setLabelText = [this](const QString& text) {
        ui->frequencyValue->setText(text);
        if (m_overlay) {
            m_overlay->label()->setText(text);
        }
#ifdef QT_TEXTTOSPEECH_FOUND
        if (m_settings.m_speechEnabled && m_speech && (text != m_previousDisplayText))
        {
            const QString speechText = textForSpeech(text);
            if (m_speech->state() == QTextToSpeech::Speaking)
            {
                // Engine is busy — save the latest text so the stateChanged
                // slot can say it once the current utterance finishes.
                m_pendingSpeechText = speechText;
            }
            else
            {
                m_pendingSpeechText.clear();
                m_speech->say(speechText);
            }
        }
#endif
        m_previousDisplayText = text;
    };

    if (m_settings.m_selectedChannel.isEmpty())
    {
        setLabelText(tr("No channel selected"));
        updateFrequencyFont();
        return;
    }

    const int channelListIndex = m_availableChannels.indexOfLongId(m_settings.m_selectedChannel);

    if (channelListIndex < 0)
    {
        setLabelText(tr("Selected channel unavailable"));
        updateFrequencyFont();
        return;
    }

    const auto& selectedChannel = m_availableChannels.at(channelListIndex);
    const FreqDisplaySettings::DisplayMode mode = m_settings.m_displayMode;

    if (m_settings.m_activeOnly)
    {
        // Only display frequency if channel is unmuted and squelch open
        // If not, we clear the frequency display
        int squelchValue;
        int audioMuteValue;
        bool hasSquelch = ChannelWebAPIUtils::getChannelReportValue(selectedChannel.m_superIndex, selectedChannel.m_index, "squelch", squelchValue);
        bool hasAudioMute = ChannelWebAPIUtils::getChannelSetting(selectedChannel.m_superIndex, selectedChannel.m_index, "audioMute", audioMuteValue);
        if ((hasSquelch && !squelchValue) || (hasAudioMute && audioMuteValue)) 
        {
            setLabelText(tr(""));
            updateFrequencyFont();
            return;
        }
    }

    // --- Frequency ---
    QString freqText;
    if (mode == FreqDisplaySettings::Frequency || mode == FreqDisplaySettings::Both)
    {
        double centerFrequencyHz = 0.0;
        int offsetHz = 0;

        if (!ChannelWebAPIUtils::getCenterFrequency(selectedChannel.m_superIndex, centerFrequencyHz))
        {
            setLabelText(tr("Frequency unavailable"));
            updateFrequencyFont();
            return;
        }
        // Not all channels have an offset
        if (!ChannelWebAPIUtils::getFrequencyOffset(selectedChannel.m_superIndex, selectedChannel.m_index, offsetHz)) {
            offsetHz = 0;
        }

        const qint64 absoluteFrequency = qRound64(centerFrequencyHz) + static_cast<qint64>(offsetHz);
        freqText = formatFrequency(absoluteFrequency);
    }

    // --- Power ---
    QString powerText;
    if (mode == FreqDisplaySettings::Power || mode == FreqDisplaySettings::Both)
    {
        double power = 0.0;
        if (!ChannelWebAPIUtils::getChannelReportValue(selectedChannel.m_superIndex, selectedChannel.m_index, "channelPowerDB", power))
        {
            if (mode == FreqDisplaySettings::Power)
            {
                // Power-only mode: nothing else to show
                setLabelText(tr("Power unavailable"));
                updateFrequencyFont();
                return;
            }
            // Both mode: power unavailable but frequency is valid — show frequency only
        }
        else
        {
            powerText = m_settings.m_showUnits
                ? QString("%1 dB").arg(power, 0, 'f', m_settings.m_powerDecimalPlaces)
                : QString("%1").arg(power, 0, 'f', m_settings.m_powerDecimalPlaces);
        }
    }

    // --- Compose display text ---
    if (mode == FreqDisplaySettings::Frequency) {
        setLabelText(freqText);
    } else if (mode == FreqDisplaySettings::Power) {
        setLabelText(powerText);
    } else {
        // Both: show frequency alone when power is unavailable
        setLabelText(powerText.isEmpty() ? freqText : freqText + "\n" + powerText);
    }

    updateFrequencyFont();
}

void FreqDisplayGUI::updateFrequencyFont()
{
    QLabel* label = m_overlay ? m_overlay->label() : ui->frequencyValue;
    const int availableWidth = label->width();
    const int availableHeight = label->height();
    if (availableWidth <= 0 || availableHeight <= 0) {
        return;
    }

    const QString text = label->text();
    if (text.isEmpty()) {
        return;
    }

    // Build a font with the user-chosen family (or the widget's current family if none saved)
    QFont font = label->font();
    if (!m_settings.m_fontName.isEmpty()) {
        font.setFamily(m_settings.m_fontName);
    }

    // Probe at a large reference size to get accurate text dimensions, then
    // scale linearly to find the largest point size that fits in both directions.
    font.setPointSize(m_fontProbePointSize);
    const QFontMetrics fm(font);

    // For multi-line text (Both mode) find the widest line; divide available
    // height equally across lines so each line receives the same font size.
    const QStringList lines = text.split('\n');
    const int numLines = lines.size();
    int maxLineWidth = 0;
    for (const QString& line : lines) {
        maxLineWidth = qMax(maxLineWidth, fm.horizontalAdvance(line));
    }
    const int lineHeight = fm.lineSpacing();

    if (maxLineWidth <= 0 || lineHeight <= 0) {
        return;
    }

    maxLineWidth += 5; // Add some space for a border

    const int maxFromWidth  = m_fontProbePointSize * availableWidth  / maxLineWidth;
    const int maxFromHeight = m_fontProbePointSize * availableHeight / (lineHeight * numLines);
    int pointSize = qMax(m_minimumFrequencyFontPointSize, qMin(maxFromWidth, maxFromHeight));
    font.setPointSize(pointSize);

    // Verify the text actually fits at the calculated point size.  The linear
    // interpolation from m_fontProbePointSize can be slightly inaccurate due to
    // font hinting or non-linear glyph metrics at the target size; if the text
    // would overflow either horizontally or vertically, reduce by one point at a
    // time until it fits.
    while (pointSize > m_minimumFrequencyFontPointSize)
    {
        const QFontMetrics verifyFm(font);
        bool overflow = false;
        for (const QString& line : lines)
        {
            if (verifyFm.horizontalAdvance(line) > availableWidth)
            {
                overflow = true;
                break;
            }
        }
        if (!overflow && (verifyFm.lineSpacing() * numLines > availableHeight)) {
            overflow = true;
        }
        if (!overflow) {
            break;
        }
        font.setPointSize(--pointSize);
    }

    label->setFont(font);
}

void FreqDisplayGUI::showEvent(QShowEvent* event)
{
    FeatureGUI::showEvent(event);
    applyTransparency();
}

void FreqDisplayGUI::applyTransparency()
{
    if (m_settings.m_transparentBackground)
    {
        // Only create the overlay if this widget is already visible on screen.
        // When called at startup (before the widget has been shown and placed by
        // the workspace), skip overlay creation so that the window appears at its
        // saved MDI position rather than at (0,0).
        if (!m_overlay && isVisible())
        {
            m_overlay = new FreqDisplayOverlay();
            m_overlay->label()->setText(ui->frequencyValue->text());
            connect(m_overlay, &FreqDisplayOverlay::exitTransparentMode,
                    this, &FreqDisplayGUI::onExitTransparentMode);
            connect(m_overlay, &FreqDisplayOverlay::resized,
                    this, &FreqDisplayGUI::updateFrequencyFont);
            // Position the overlay at the current screen position of frequencyValue widget.
            m_overlay->move(ui->frequencyValue->mapToGlobal(QPoint(0, 0)));
            m_overlay->resize(ui->frequencyValue->size());
            applyTextColor();
            applyDropShadow();
            m_overlay->show();
        }
        hide();
    }
    else
    {
        if (m_overlay)
        {
            // Hide immediately so the overlay disappears at once, then schedule
            // deletion for after the current event-loop iteration.  Plain delete
            // would crash here because onExitTransparentMode() is invoked via a
            // signal emitted from inside the overlay's contextMenuEvent(), which
            // is still on the call stack when we reach this point.
            m_overlay->hide();
            m_overlay->deleteLater();
            m_overlay = nullptr;
        }
        show();
        applyTextColor();
        applyDropShadow();
        updateFrequencyFont();
    }
}

void FreqDisplayGUI::applySpeech()
{
#ifdef QT_TEXTTOSPEECH_FOUND
    if (m_settings.m_speechEnabled && !m_speech)
    {
        m_speech = new QTextToSpeech(this);
        if (m_speech)
        {
            connect(m_speech, &QTextToSpeech::stateChanged, this, &FreqDisplayGUI::speechStateChanged);
        }
        else
        {
            ui->speech->setChecked(false);
            ui->speech->setEnabled(false);
        }
    }
#endif
}

void FreqDisplayGUI::applyTextColor()
{
    const QString styleSheet = QString("color: %1;").arg(m_settings.m_textColor.name());
    if (m_overlay) {
        m_overlay->label()->setStyleSheet(styleSheet);
    } else {
        ui->frequencyValue->setStyleSheet(styleSheet);
    }
}

void FreqDisplayGUI::updateTextColorButton()
{
    QPixmap pm(16, 16);
    pm.fill(m_settings.m_textColor);
    ui->textColor->setIcon(pm);
}

void FreqDisplayGUI::updateDropShadowColorButton()
{
    QPixmap pm(16, 16);
    pm.fill(m_settings.m_dropShadowColor);
    ui->dropShadowColor->setIcon(pm);
}

void FreqDisplayGUI::applyDropShadow()
{
    QWidget* target = m_overlay ? static_cast<QWidget*>(m_overlay->label())
                                : ui->frequencyValue;
    if (m_settings.m_dropShadowEnabled)
    {
        auto* effect = new QGraphicsDropShadowEffect(target);
        effect->setColor(m_settings.m_dropShadowColor);
        effect->setBlurRadius(m_dropShadowBlurRadius);
        effect->setOffset(m_dropShadowOffsetX, m_dropShadowOffsetY);
        target->setGraphicsEffect(effect);
    }
    else
    {
        target->setGraphicsEffect(nullptr);
    }
}

void FreqDisplayGUI::on_textColor_clicked()
{
    const QColor color = QColorDialog::getColor(
        m_settings.m_textColor, this, tr("Select text color"), QColorDialog::DontUseNativeDialog);
    if (color.isValid())
    {
        m_settings.m_textColor = color;
        updateTextColorButton();
        applyTextColor();
        applySetting("textColor");
    }
}

void FreqDisplayGUI::on_dropShadow_toggled(bool checked)
{
    m_settings.m_dropShadowEnabled = checked;
    applyDropShadow();
    applySetting("dropShadowEnabled");
}

void FreqDisplayGUI::on_dropShadowColor_clicked()
{
    const QColor color = QColorDialog::getColor(
        m_settings.m_dropShadowColor, this, tr("Select drop shadow color"), QColorDialog::DontUseNativeDialog);
    if (color.isValid())
    {
        m_settings.m_dropShadowColor = color;
        updateDropShadowColorButton();
        applyDropShadow();
        applySetting("dropShadowColor");
    }
}

void FreqDisplayGUI::on_displayMode_currentIndexChanged(int index)
{
    m_settings.m_displayMode = static_cast<FreqDisplaySettings::DisplayMode>(index);
    applySetting("displayMode");
    updateFrequencyText();
}

void FreqDisplayGUI::on_speech_toggled(bool checked)
{
    bool hadSpeech = m_settings.m_speechEnabled;
    m_settings.m_speechEnabled = checked;
    applySpeech();
    applySetting("speechEnabled");
#ifdef QT_TEXTTOSPEECH_FOUND
    if (checked && !hadSpeech && m_speech)
    {
        // Just enabled speech: speak the current text immediately
        const QString speechText = textForSpeech(ui->frequencyValue->text());
        m_speech->say(speechText);
    }
#endif
}

void FreqDisplayGUI::on_fontFamily_currentFontChanged(const QFont& font)
{
    m_settings.m_fontName = font.family();
    applySetting("fontName");
    updateFrequencyFont();
}

void FreqDisplayGUI::on_frequencyUnits_currentIndexChanged(int index)
{
    m_settings.m_frequencyUnits = static_cast<FreqDisplaySettings::FrequencyUnits>(index);
    updateFreqDecimalSpinbox();
    applySetting("frequencyUnits");
    updateFrequencyText();
}

void FreqDisplayGUI::on_showUnits_toggled(bool checked)
{
    m_settings.m_showUnits = checked;
    applySetting("showUnits");
    updateFrequencyText();
}

void FreqDisplayGUI::updateFreqDecimalSpinbox()
{
    const FreqDisplaySettings::FrequencyUnits units = m_settings.m_frequencyUnits;
    const bool enabled = (units != FreqDisplaySettings::Hz);
    ui->freqDecimalPlaces->setEnabled(enabled);
    ui->freqDecimalPlacesLabel->setEnabled(enabled);

    if (enabled)
    {
        int maxDecimals = 3; // kHz
        if (units == FreqDisplaySettings::MHz) {
            maxDecimals = 6;
        } else if (units == FreqDisplaySettings::GHz) {
            maxDecimals = 9;
        }

        ui->freqDecimalPlaces->blockSignals(true);
        ui->freqDecimalPlaces->setMaximum(maxDecimals);
        // Clamp stored value to new maximum so the spinbox and setting stay in sync
        m_settings.m_freqDecimalPlaces = qMin(m_settings.m_freqDecimalPlaces, maxDecimals);
        ui->freqDecimalPlaces->setValue(m_settings.m_freqDecimalPlaces);
        ui->freqDecimalPlaces->blockSignals(false);
    }
}

void FreqDisplayGUI::on_freqDecimalPlaces_valueChanged(int value)
{
    m_settings.m_freqDecimalPlaces = value;
    applySetting("freqDecimalPlaces");
    updateFrequencyText();
}

void FreqDisplayGUI::on_powerDecimalPlaces_valueChanged(int value)
{
    m_settings.m_powerDecimalPlaces = value;
    applySetting("powerDecimalPlaces");
    updateFrequencyText();
}

QString FreqDisplayGUI::formatFrequency(qint64 frequencyHz) const
{
    const QLocale locale;
    const bool showUnits = m_settings.m_showUnits;
    const int dp = m_settings.m_freqDecimalPlaces;

    switch (m_settings.m_frequencyUnits)
    {
    case FreqDisplaySettings::kHz: {
        const QString s = locale.toString(frequencyHz / m_kHzDivisor, 'f', dp);
        return showUnits ? s + tr(" kHz") : s;
    }
    case FreqDisplaySettings::MHz: {
        const QString s = locale.toString(frequencyHz / m_MHzDivisor, 'f', dp);
        return showUnits ? s + tr(" MHz") : s;
    }
    case FreqDisplaySettings::GHz: {
        const QString s = locale.toString(frequencyHz / m_GHzDivisor, 'f', dp);
        return showUnits ? s + tr(" GHz") : s;
    }
    case FreqDisplaySettings::Hz:
    default: {
        const QString s = locale.toString(frequencyHz);
        return showUnits ? s + tr(" Hz") : s;
    }
    }
}

void FreqDisplayGUI::on_activeOnly_toggled(bool checked)
{
    m_settings.m_activeOnly = checked;
    applySetting("activeOnly");
}

void FreqDisplayGUI::on_transparentBackground_toggled(bool checked)
{
    m_settings.m_transparentBackground = checked;
    applyTransparency();
    applySetting("transparentBackground");
}

void FreqDisplayGUI::onExitTransparentMode()
{
    m_settings.m_transparentBackground = false;
    ui->transparentBackground->blockSignals(true);
    ui->transparentBackground->setChecked(false);
    ui->transparentBackground->blockSignals(false);
    applyTransparency();
    applySetting("transparentBackground");
}

void FreqDisplayGUI::resizeEvent(QResizeEvent *event)
{
    FeatureGUI::resizeEvent(event);
    updateFrequencyFont();
}

#ifdef QT_TEXTTOSPEECH_FOUND
void FreqDisplayGUI::speechStateChanged(QTextToSpeech::State state)
{
    if (state == QTextToSpeech::Ready && !m_pendingSpeechText.isEmpty())
    {
        const QString text = m_pendingSpeechText;
        m_pendingSpeechText.clear();
        m_speech->say(text);
    }
}

// Expand display-text unit abbreviations to full spoken words so that TTS
// engines read them naturally rather than letter-by-letter.
QString FreqDisplayGUI::textForSpeech(const QString& displayText)
{
    QString s = displayText;
    // Order matters: longer unit strings must be replaced before shorter ones
    // that are sub-strings of them (e.g. "GHz" before "Hz").
    s.replace(QLatin1String(" GHz"), QLatin1String(" gigahertz"));
    s.replace(QLatin1String(" MHz"), QLatin1String(" megahertz"));
    s.replace(QLatin1String(" kHz"), QLatin1String(" kilohertz"));
    s.replace(QLatin1String(" Hz"),  QLatin1String(" hertz"));
    s.replace(QLatin1String(" dB"),  QLatin1String(" decibels"));
    return s;
}
#endif

void FreqDisplayGUI::makeUIConnections()
{
    connect(ui->channels, qOverload<int>(&QComboBox::currentIndexChanged), this, &FreqDisplayGUI::on_channels_currentIndexChanged);
    connect(ui->displayMode, qOverload<int>(&QComboBox::currentIndexChanged), this, &FreqDisplayGUI::on_displayMode_currentIndexChanged);
    connect(ui->speech, &ButtonSwitch::toggled, this, &FreqDisplayGUI::on_speech_toggled);
    connect(ui->fontFamily, &QFontComboBox::currentFontChanged, this, &FreqDisplayGUI::on_fontFamily_currentFontChanged);
    connect(ui->activeOnly, &ButtonSwitch::toggled, this, &FreqDisplayGUI::on_activeOnly_toggled);
    connect(ui->transparentBackground, &ButtonSwitch::toggled, this, &FreqDisplayGUI::on_transparentBackground_toggled);
    connect(ui->frequencyUnits, qOverload<int>(&QComboBox::currentIndexChanged), this, &FreqDisplayGUI::on_frequencyUnits_currentIndexChanged);
    connect(ui->showUnits, &ButtonSwitch::toggled, this, &FreqDisplayGUI::on_showUnits_toggled);
    connect(ui->freqDecimalPlaces, qOverload<int>(&QSpinBox::valueChanged), this, &FreqDisplayGUI::on_freqDecimalPlaces_valueChanged);
    connect(ui->powerDecimalPlaces, qOverload<int>(&QSpinBox::valueChanged), this, &FreqDisplayGUI::on_powerDecimalPlaces_valueChanged);
    connect(ui->textColor, &QPushButton::clicked, this, &FreqDisplayGUI::on_textColor_clicked);
    connect(ui->dropShadow, &ButtonSwitch::toggled, this, &FreqDisplayGUI::on_dropShadow_toggled);
    connect(ui->dropShadowColor, &QPushButton::clicked, this, &FreqDisplayGUI::on_dropShadowColor_clicked);
    connect(&m_pollTimer, &QTimer::timeout, this, &FreqDisplayGUI::pollSelectedChannel);
}
