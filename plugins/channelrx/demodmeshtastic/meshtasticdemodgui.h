///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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

#ifndef INCLUDE_MESHTASTICDEMODGUI_H
#define INCLUDE_MESHTASTICDEMODGUI_H

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"
#include <vector>
#include <QMap>
#include <QVector>

#include "meshtasticdemodsettings.h"

class PluginAPI;
class DeviceUISet;
class MeshtasticDemod;
class SpectrumVis;
class BasebandSampleSink;
class QComboBox;
class QCheckBox;
class QPushButton;
class QTabWidget;
class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;
namespace MeshtasticDemodMsg { class MsgReportDecodeBytes; }

namespace Ui {
	class MeshtasticDemodGUI;
}

class MeshtasticDemodGUI : public ChannelGUI {
	Q_OBJECT

public:
	static MeshtasticDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceAPI, BasebandSampleSink *rxChannel);
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
    virtual QColor getTitleColor() const  { return m_settings.m_rgbColor; };
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) { m_settings.m_streamIndex = streamIndex; }

private slots:
	void channelMarkerChangedByCursor();
    void on_deltaFrequency_changed(qint64 value);
	void on_BW_valueChanged(int value);
	void on_Spread_valueChanged(int value);
    void on_deBits_valueChanged(int value);
    void on_fftWindow_currentIndexChanged(int index);
    void on_preambleChirps_valueChanged(int value);
	void on_scheme_currentIndexChanged(int index);
	void on_mute_toggled(bool checked);
	void on_clear_clicked(bool checked);
    void on_eomSquelch_valueChanged(int value);
    void on_messageLength_valueChanged(int value);
	void on_messageLengthAuto_stateChanged(int state);
	void on_header_stateChanged(int state);
	void on_fecParity_valueChanged(int value);
	void on_crc_stateChanged(int state);
	void on_packetLength_valueChanged(int value);
	void on_udpSend_stateChanged(int state);
	void on_udpAddress_editingFinished();
	void on_udpPort_editingFinished();
	void on_invertRamps_stateChanged(int state);
	void on_meshRegion_currentIndexChanged(int index);
    void on_meshPreset_currentIndexChanged(int index);
    void on_meshChannel_currentIndexChanged(int index);
    void on_meshApply_clicked(bool checked);
    void on_meshKeys_clicked(bool checked);
    void on_meshAutoSampleRate_toggled(bool checked);
    void on_meshAutoLock_clicked(bool checked);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDialogCalled(const QPoint& p);
    void channelMarkerHighlightedByCursor();
	void handleInputMessages();
    void onPipelineTreeSelectionChanged();
	void tick();

private:
	Ui::MeshtasticDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	RollupState m_rollupState;
	MeshtasticDemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
	bool m_doApplySettings;

	MeshtasticDemod* m_chirpChatDemod;
	SpectrumVis* m_spectrumVis;
    QComboBox* m_meshRegionCombo;
    QComboBox* m_meshPresetCombo;
    QComboBox* m_meshChannelCombo;
    QPushButton* m_meshApplyButton;
    QPushButton* m_meshKeysButton;
    QPushButton* m_meshAutoLockButton;
    QPushButton* m_dechirpLiveFollowButton;
    QCheckBox* m_meshAutoSampleRateCheck;
    struct PipelineView
    {
        QWidget *tabWidget = nullptr;
        QPlainTextEdit *logText = nullptr;
        QTreeWidget *treeWidget = nullptr;
    };
    QTabWidget *m_pipelineTabs;
    QMap<int, PipelineView> m_pipelineViews;
    bool m_meshControlsUpdating;
    struct MeshAutoLockCandidate {
        int inputOffsetHz;
        bool invertRamps;
        int deBits;
        double score;
        int samples;
        double sourceScore;
        int sourceSamples;
        int syncWordZeroCount;
        int headerParityOkOrFixCount;
        int headerCRCCount;
        int payloadCRCCount;
        int earlyEOMCount;
    };
    QVector<MeshAutoLockCandidate> m_meshAutoLockCandidates;
    bool m_meshAutoLockActive;
    int m_meshAutoLockCandidateIndex;
    qint64 m_meshAutoLockCandidateStartMs;
    int m_meshAutoLockObservedSamplesForCandidate;
    int m_meshAutoLockObservedSourceSamplesForCandidate;
    int m_meshAutoLockTotalDecodeSamples;
    bool m_meshAutoLockTrafficSeen;
    int m_meshAutoLockActivityTicks;
    qint64 m_meshAutoLockArmStartMs;
    int m_meshAutoLockBaseOffsetHz;
    bool m_meshAutoLockBaseInvert;
    int m_meshAutoLockBaseDeBits;
    bool m_remoteTcpReconnectAutoApplyPending;
    int m_remoteTcpReconnectAutoApplyWaitTicks;
    bool m_remoteTcpLastRunningState;
    struct DechirpSnapshot
    {
        int fftSize = 0;
        std::vector<std::vector<float>> lines;
    };
    QMap<QString, DechirpSnapshot> m_dechirpSnapshots;
    QVector<QString> m_dechirpSnapshotOrder;
    bool m_dechirpInspectionActive;
    QString m_dechirpSelectedMessageKey;
    QString m_replayPendingMessageKey;
    bool m_replayPendingHasSelection;
    bool m_replaySelectionQueued;
    QMap<QString, QString> m_pipelineMessageKeyByBase;
    QMap<QString, QVector<QString>> m_pipelinePendingMessageKeysByBase;
    quint64 m_pipelineMessageSequence;
	MessageQueue m_inputMessageQueue;
	unsigned int m_tickCount;

	explicit MeshtasticDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~MeshtasticDemodGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
    void displaySettings();
    void updateControlAvailabilityHints();
    void displaySquelch();
    void setBandwidths();
    void showLoRaMessage(const Message& message); //!< For LoRa coding scheme
    void showTextMessage(const Message& message); //!< For TTY and ASCII
    void showFTMessage(const Message& message);   //!< For FT coding scheme
    void setupPipelineViews();
    PipelineView& ensurePipelineView(int pipelineId, const QString& pipelineName);
    void clearPipelineViews();
    void appendPipelineLogLine(int pipelineId, const QString& pipelineName, const QString& line);
    void appendPipelineStatusLine(int pipelineId, const QString& pipelineName, const QString& status);
    void appendPipelineBytes(int pipelineId, const QString& pipelineName, const QByteArray& bytes);
    void appendPipelineTreeFields(
        int pipelineId,
        const QString& pipelineName,
        const QString& messageTitle,
        const QVector<QPair<QString, QString>>& fields,
        const QString& messageKey
    );
	void displayText(const QString& text);
	void displayBytes(const QByteArray& bytes);
	void displayStatus(const QString& status);
    void displayLoRaStatus(int headerParityStatus, bool headerCRCStatus, int payloadParityStatus, bool payloadCRCStatus);
    void displayFTStatus(int payloadParityStatus, bool payloadCRCStatus);
	QString getParityStr(int parityStatus);
    void resetLoRaStatus();
	bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
    void setupMeshtasticAutoProfileControls();
    void rebuildMeshtasticChannelOptions();
    bool retuneDeviceToFrequency(qint64 centerFrequencyHz);
    bool autoTuneDeviceSampleRateForBandwidth(int bandwidthHz, QString& summary);
    int findBandwidthIndex(int bandwidthHz) const;
    void applyMeshtasticProfileFromSelection();
    void editMeshtasticKeys();
    void startMeshAutoLock();
    void stopMeshAutoLock(bool keepBestCandidate);
    void applyMeshAutoLockCandidate(const MeshAutoLockCandidate& candidate, bool applySettingsNow);
    void handleMeshAutoLockObservation(const MeshtasticDemodMsg::MsgReportDecodeBytes& msg);
    void handleMeshAutoLockSourceObservation();
    void advanceMeshAutoLock();
    QString buildPipelineMessageBaseKey(int pipelineId, uint32_t frameId, const QString& timestamp) const;
    QString allocatePipelineMessageKey(const QString& baseKey);
    QString resolvePipelineMessageKey(const QString& baseKey) const;
    void consumePipelineMessageKey(const QString& baseKey, const QString& key);
    void rememberLoRaDechirpSnapshot(const MeshtasticDemodMsg::MsgReportDecodeBytes& msg, const QString& messageKey);
    void setDechirpInspectionMode(bool enabled);
    void updateDechirpModeUI();
    void queueReplayForTree(QTreeWidget *treeWidget);
    void processQueuedReplay();
    void hardResetDechirpDisplayBuffers();
    void clearTreeMessageKeyReferences(const QString& messageKey);
    void replayDechirpSnapshot(const DechirpSnapshot& snapshot);
};

#endif // INCLUDE_MESHTASTICDEMODGUI_H
