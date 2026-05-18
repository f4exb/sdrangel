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

#ifndef INCLUDE_FEATURE_FREQDISPLAYGUI_H_
#define INCLUDE_FEATURE_FREQDISPLAYGUI_H_

#include <QLabel>
#include <QPoint>
#include <QTimer>
#include <QFontComboBox>

#ifdef QT_TEXTTOSPEECH_FOUND
#include <QTextToSpeech>
#endif

#include "availablechannelorfeaturehandler.h"
#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "freqdisplaysettings.h"

class PluginAPI;
class FeatureUISet;
class FreqDisplay;
class Feature;

/// QLabel subclass that always reports a small minimum size hint so that the
/// FreqDisplayGUI window can be resized freely without the label preventing
/// the window from being made small.
class FrequencyLabel : public QLabel
{
    Q_OBJECT
public:
    explicit FrequencyLabel(QWidget *parent = nullptr) : QLabel(parent) {}
    QSize minimumSizeHint() const override { return QSize(50, 10); }
};

namespace Ui {
class FreqDisplayGUI;
}

/// Frameless, transparent top-level overlay window used when transparent-background
/// mode is active.  Shows a single label with the frequency/power text and
/// provides a right-click context menu to exit transparent mode.
class FreqDisplayOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit FreqDisplayOverlay(QWidget* parent = nullptr);

    /// Returns the label that displays the frequency/power text.
    QLabel* label() { return m_label; }

signals:
    void exitTransparentMode();
    void resized();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QLabel* m_label;
    bool m_dragging;
    QPoint m_dragStartPos;
};

class FreqDisplayGUI : public FeatureGUI
{
    Q_OBJECT

public:
    static FreqDisplayGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
    void destroy() override;

    bool handleMessage(const Message& message);

    void resetToDefaults() override;
    QByteArray serialize() const override;
    bool deserialize(const QByteArray& data) override;
    MessageQueue *getInputMessageQueue() override { return &m_inputMessageQueue; }
    void setWorkspaceIndex(int index) override;
    int getWorkspaceIndex() const override { return m_settings.m_workspaceIndex; }
    void setGeometryBytes(const QByteArray& blob) override { m_settings.m_geometryBytes = blob; }
    QByteArray getGeometryBytes() const override { return m_settings.m_geometryBytes; }

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent* event) override;

private:
    Ui::FreqDisplayGUI* ui;
    FreqDisplay* m_freqDisplay;
    FreqDisplaySettings m_settings;
    QStringList m_settingsKeys;
    RollupState m_rollupState;
    MessageQueue m_inputMessageQueue;
    AvailableChannelOrFeatureHandler m_availableChannelOrFeatureHandler;
    AvailableChannelOrFeatureList m_availableChannels;
    QTimer m_pollTimer;
    bool m_doApplySettings;
    QString m_previousDisplayText; ///< Last text set on frequencyValue, used to detect changes for speech
    FreqDisplayOverlay* m_overlay; ///< Transparent overlay window shown when transparent-background mode is active

#ifdef QT_TEXTTOSPEECH_FOUND
    QTextToSpeech *m_speech = nullptr;
    QString m_pendingSpeechText; ///< Most recent text to speak once the engine is no longer busy
    static QString textForSpeech(const QString& displayText);
#endif

    static constexpr const char* m_rxTxChannelKinds = "RTM";
    static constexpr int m_pollIntervalMs = 250;
    static constexpr int m_minimumFrequencyFontPointSize = 10;
    /// Reference point size used when probing text metrics in updateFrequencyFont().
    /// Large enough that integer rounding in QFontMetrics is negligible.
    static constexpr int m_fontProbePointSize = 200;
    static constexpr double m_kHzDivisor = 1e3;
    static constexpr double m_MHzDivisor = 1e6;
    static constexpr double m_GHzDivisor = 1e9;
    static constexpr double m_dropShadowBlurRadius = 10.0;
    static constexpr double m_dropShadowOffsetX = 2.0;
    static constexpr double m_dropShadowOffsetY = 2.0;

    explicit FreqDisplayGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    ~FreqDisplayGUI() override;

    void makeUIConnections();
    void blockApplySettings(bool block);
    void displaySettings();
    void applySetting(const QString& settingsKey);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void applyAllSettings();
    void updateChannelList();
    void updateFrequencyText();
    void updateFrequencyFont();
    void updateFreqDecimalSpinbox();
    void applyTransparency();
    void applySpeech();
    void applyTextColor();
    void applyDropShadow();
    void updateTextColorButton();
    void updateDropShadowColorButton();
    QString formatFrequency(qint64 frequencyHz) const;

private slots:
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void channelsOrFeaturesChanged(const QStringList& renameFrom, const QStringList& renameTo, const QStringList& removed, const QStringList& added);
    void on_channels_currentIndexChanged(int index);
    void on_displayMode_currentIndexChanged(int index);
    void on_speech_toggled(bool checked);
    void on_fontFamily_currentFontChanged(const QFont& font);
    void on_activeOnly_toggled(bool checked);
    void on_transparentBackground_toggled(bool checked);
    void on_frequencyUnits_currentIndexChanged(int index);
    void on_showUnits_toggled(bool checked);
    void on_freqDecimalPlaces_valueChanged(int value);
    void on_powerDecimalPlaces_valueChanged(int value);
    void on_textColor_clicked();
    void on_dropShadow_toggled(bool checked);
    void on_dropShadowColor_clicked();
    void pollSelectedChannel();
    void onExitTransparentMode();
#ifdef QT_TEXTTOSPEECH_FOUND
    void speechStateChanged(QTextToSpeech::State state);
#endif
};

#endif // INCLUDE_FEATURE_FREQDISPLAYGUI_H_
