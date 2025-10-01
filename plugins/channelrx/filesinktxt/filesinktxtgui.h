#ifndef INCLUDE_FILESINKTXTGUI_H
#define INCLUDE_FILESINKTXTGUI_H

#include <QWidget>
#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "filesinktxt.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class FileSinkTxt;
class QLabel;
class QLineEdit;
class QPushButton;

namespace Ui {
    class FileSinkTxtGUI;
}

class FileSinkTxtGUI : public ChannelGUI
{
    Q_OBJECT

public:
    static FileSinkTxtGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
    virtual void destroy();

    void resetToDefaults();
    virtual void setWorkspaceIndex(int index) {}
    virtual int getWorkspaceIndex() const { return 0; }
    virtual void setGeometry(const QRect& r) { setGeometry(r); }
    virtual QRect getGeometry() const { return geometry(); }
    virtual void setTitle(const QString& title) { setWindowTitle(title); }
    virtual QString getTitle() const { return windowTitle(); }
    virtual QColor getTitleColor() const { return QColor(); }
    virtual void zetHidden(bool hidden) { setHidden(hidden); }
    virtual bool getHidden() const { return isHidden(); }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return -1; }
    virtual void setStreamIndex(int streamIndex) { (void) streamIndex; }

    virtual QByteArray serialize() const { return QByteArray(); }
    virtual bool deserialize(const QByteArray& data) { (void) data; return false; }
    virtual void setGeometryBytes(const QByteArray& blob) { (void) blob; }
    virtual QByteArray getGeometryBytes() const { return QByteArray(); }
    virtual MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }

protected:
    Ui::FileSinkTxtGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    FileSinkTxt* m_fileSinkTxt;
    MessageQueue m_inputMessageQueue;
    bool m_doApplySettings;

    explicit FileSinkTxtGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = nullptr);
    virtual ~FileSinkTxtGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();

protected slots:
    void handleSourceMessages();
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void tick();

private slots:
    void on_browsePushButton_clicked();
    void on_recordPushButton_toggled(bool checked);
    void on_savePushButton_clicked();
    void on_loadPushButton_clicked();
};

#endif // INCLUDE_FILESINKTXTGUI_H
