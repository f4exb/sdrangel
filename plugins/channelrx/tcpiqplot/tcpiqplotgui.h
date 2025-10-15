#ifndef TCPIQPLOTGUI_H
#define TCPIQPLOTGUI_H

#include "channel/channelgui.h"
#include "tcpiqplot.h"
#include "tcpiqplotsettings.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"

class PluginAPI;
class DeviceUISet;

namespace Ui {
class TcpIqPlotGUI;
}

class TcpIqPlotGUI : public ChannelGUI
{
    Q_OBJECT

public:
    static TcpIqPlotGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelRx);
    void destroy();

    // ChannelGUI pure virtuals
    void resetToDefaults() override;
    void setWorkspaceIndex(int index) override {}
    int getWorkspaceIndex() const override { return 0; }
    void setGeometryBytes(const QByteArray& blob) override {}
    QByteArray getGeometryBytes() const override { return QByteArray(); }
    QString getTitle() const override { return m_settings.m_title; }
    QColor getTitleColor() const override { return QColor::fromRgb(m_settings.m_rgbColor); }
    void zetHidden(bool hidden) override {}
    bool getHidden() const override { return false; }
    ChannelMarker& getChannelMarker() override { static ChannelMarker dummy; return dummy; }
    int getStreamIndex() const override { return m_settings.m_streamIndex; }
    void setStreamIndex(int streamIndex) override { m_settings.m_streamIndex = streamIndex; }
    MessageQueue* getInputMessageQueue() override { return &m_inputMessageQueue; }
    QByteArray serialize() const override;
    bool deserialize(const QByteArray& data) override;
    bool handleMessage(const Message& message);

private slots:
    void handleSourceMessages();
    void on_port_editingFinished();
    void on_plotModeCombo_currentIndexChanged(int index);
    void on_startButton_clicked();
    void on_stopButton_clicked();
    void on_persistenceCheckBox_toggled(bool checked);

private:
    explicit TcpIqPlotGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelRx, QWidget *parent = nullptr);
    ~TcpIqPlotGUI();

    void applySettings(bool force = false);
    void displaySettings();
    void blockApplySettings(bool block);
    void makeUIConnections();

    Ui::TcpIqPlotGUI *ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    TcpIqPlot* m_tcpIqPlot;
    TcpIqPlotSettings m_settings;
    bool m_doApplySettings;
    MessageQueue m_inputMessageQueue;
    QTimer* m_updateTimer;
    
    void updateFrequencyLabels();
};

#endif // TCPIQPLOTGUI_H
