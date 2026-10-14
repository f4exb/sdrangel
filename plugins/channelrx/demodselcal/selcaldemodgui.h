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

#ifndef INCLUDE_SELCALDEMODGUI_H
#define INCLUDE_SELCALDEMODGUI_H

#include <QSharedPointer>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "dsp/spectrummarkers.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"
#include "util/osndb.h"
#include "util/selcaldb.h"
#include "selcaldemod.h"
#include "selcaldemodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class SELCALDemod;
class SELCALDemodGUI;

namespace Ui {
class SELCALDemodGUI;
}
class SpectrumVis;
class QProgressDialog;
class QMenu;
class SELCALDemodGUI;

class SELCALDemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static SELCALDemodGUI *create(PluginAPI *pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index) { m_settings.m_workspaceIndex = index; };
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; };
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; };
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; };
    virtual QString getTitle() const { return m_settings.m_title; };
    virtual QColor getTitleColor() const { return m_settings.m_rgbColor; };
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) { m_settings.m_streamIndex = streamIndex; }

public slots:
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private:
    // Logical columns of the calls table
    enum Column {
        COL_UTC,
        COL_CODE,
        COL_REGISTRATION,
        COL_TYPE,
        COL_SIDEVIEW,
        COL_COUNTRY,
        COL_FREQUENCY,
        COL_RSSI,
        COL_SNR
    };

    Ui::SELCALDemodGUI *ui;
    DeviceUISet *m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    SELCALDemodSettings m_settings;
    SELCALDemod *m_selcalDemod;
    SpectrumVis *m_spectrumVis;
    MessageQueue m_inputMessageQueue;
    double m_markerOffset = 1e9;
    SELCALDecoder::Pair m_markerPair{{-1, -1}};
    QList<SpectrumAnnotationMarker> m_markers; // Tone markers last shown
    QString m_cellText;    // Of the table cell whose context menu is shown
    QMenu *m_columnMenu = nullptr; // Selects the table columns shown
    qint64 m_deviceCenterFrequency = 0;
    int m_basebandSampleRate = 48000;
    bool m_doApplySettings = true;
    bool m_squelchOpen = false;
    SELCALDB m_selcalDB;
    OsnDB m_osnDB;
    QProgressDialog *m_progressDialog = nullptr;
    bool m_downloadAircraftDB = false;
    QSharedPointer<const QHash<QString, AircraftInformation *>> m_aircraftInfoByReg;

    explicit SELCALDemodGUI(PluginAPI *pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget *parent = nullptr);
    ~SELCALDemodGUI() override;
    void applySettings(const QStringList& keys, bool force = false);
    void displaySettings();
    void makeUIConnections();
    void loadAircraftInformation();
    void messageReceived(const SELCALMessages::Call& call, bool update = true);
    void updateAudioMute();
    void filter();
    void updateAbsoluteCenterFrequency();
    void exportCsv();
    void copySelection();
    void updateCarrierStatus(const SELCALMessages::Status& status);
    void updateToneMarkers(double offset, const SELCALDecoder::Pair& pair);
    static QString signedHz(double value);

    void lookupAircraft(int row);
    void updateAircraft();
    void finishDownload();
private slots:
    void on_getSELCALDB_clicked();
    void downloadingURL(const QString& url);
    void downloadProgress(qint64 bytesRead, qint64 totalBytes);
    void downloadError(const QString& error);
    void downloadSELCALInformationFinished();
    void downloadAircraftInformationFinished();
    void on_deltaFrequency_changed(qint64 value);
    void on_modulation_currentIndexChanged(int index);
    void on_rfBandwidth_valueChanged(int value);
    void on_toneFrequencyOffset_valueChanged(double value);
    void on_autoToneFrequencyOffset_toggled(bool checked);
    void on_threshold_valueChanged(double value);
    void on_volume_valueChanged(int value);
    void on_audioMute_toggled(bool checked);
    void on_squelch_valueChanged(int value);
    void on_filter_textChanged(const QString& text);
    void on_clearTable_clicked();
    void on_logEnable_toggled(bool checked);
    void on_logFilename_clicked();
    void on_logOpen_clicked();
    void on_udpEnabled_toggled(bool checked);
    void on_udpAddress_editingFinished();
    void on_udpPort_valueChanged(int value);
    void messages_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void messages_sectionResized(int logicalIndex, int oldSize, int newSize);
    void messages_customContextMenuRequested(QPoint pos);
    void copyCell();
    void messages_columnSelectMenu(QPoint pos);
    void messages_columnSelectMenuChecked(bool checked);
    void handleInputMessages();
    void audioSelect(const QPoint& point);
    void readLog(const QString& filename);
    void tick();
    void onMenuDialogCalled(const QPoint& point);
    void onWidgetRolled(QWidget *widget, bool rollDown);
};
#endif
