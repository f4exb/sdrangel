#ifndef INCLUDE_GEOSCANDEMODGUI_H
#define INCLUDE_GEOSCANDEMODGUI_H

#include <QWidget>
#include <QTimer>
#include <QUdpSocket>
#include <QString>
#include <QByteArray>
#include <QColor>

#include "../../../sdrgui/channel/channelgui.h"
#include "../../../sdrbase/dsp/channelmarker.h"
#include "../../../sdrbase/util/messagequeue.h"

#include "geoscandemod.h"


class DeviceUISet;
class BasebandSampleSink;

namespace Ui { class GeoscanDemodGUI; }

class GeoscanDemodGUI : public ChannelGUI
{
    Q_OBJECT
public:
    // Конструктор должен соответствовать сигнатуре в .cpp
    explicit GeoscanDemodGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget *parent = nullptr);
    virtual ~GeoscanDemodGUI();

    // === Реализация интерфейса ChannelGUI ===
    virtual void destroy() { delete this; }
    virtual QString getTitle() const override { return m_settingsTitle.isEmpty() ? "Geoscan Satellites Decoder" : m_settingsTitle; }
    virtual QColor getTitleColor() const override { return QColor(Qt::cyan); }
    virtual void setWorkspaceIndex(int index) override { m_workspaceIndex = index; }
    virtual int getWorkspaceIndex() const override { return m_workspaceIndex; }
    virtual void setGeometryBytes(const QByteArray& blob) override { m_geometryBytes = blob; }
    virtual QByteArray getGeometryBytes() const override { return m_geometryBytes; }
    virtual void zetHidden(bool hidden) override { m_hidden = hidden; }
    virtual bool getHidden() const override { return m_hidden; }
    virtual ChannelMarker& getChannelMarker() override { return *m_channelMarker; }
    virtual int getStreamIndex() const override { return m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) override { m_streamIndex = streamIndex; }
    virtual MessageQueue* getInputMessageQueue() override { return &m_inputMessageQueue; }
    virtual void updateSettings();
    virtual void displaySettings(const GeoscanSettings& settings, bool force = false);

    virtual void resetToDefaults() override;
    virtual QByteArray serialize() const override;
    virtual bool deserialize(const QByteArray& data) override;
    virtual QSize minimumSizeHint() const override { return QSize(400, 600); }
    virtual QSize sizeHint() const override { return QSize(480, 720); }

    static const QString m_channelIdURI;
    static const QString m_channelId;

public slots:
    void on_deltaF_valueChanged(int value);
    void on_bw_valueChanged(int value);
    void on_dev_valueChanged(int value);
    void on_threshold_valueChanged(int value);
    void on_udpAddress_textChanged(const QString& text);
    void on_udpPort_valueChanged(int value);
    void on_decoder_currentIndexChanged(int index);
    void on_tleFileButton_clicked();
    void on_satelliteCombo_currentIndexChanged(int index);
    void on_dopplerEnable_toggled(bool checked);
    void on_udpFormat_currentIndexChanged(int index);
    void on_startDecoderButton_clicked();
    void on_stopDecoderButton_clicked();
    void on_timer_timeout();

private:
    Ui::GeoscanDemodGUI *ui;
    DeviceUISet *m_deviceUISet;
    GeoscanDemod *m_geoscanDemod;
    QString m_settingsTitle;
    GeoscanSettings m_settings;
    QTimer *m_updateTimer;
    QUdpSocket *m_udpSocket;

    int m_workspaceIndex = 0;
    QByteArray m_geometryBytes;
    bool m_hidden = false;
    ChannelMarker *m_channelMarker = nullptr;  // теперь тип известен
    int m_streamIndex = 0;
    MessageQueue m_inputMessageQueue;  // теперь тип известен

    struct TLEData {
        QString name;
        double line1[7], line2[7];
    };
    QVector<TLEData> m_tleList;
    bool m_dopplerEnabled = false;

    void parseTLEFile(const QString& filePath);
    double calculateDopplerShift(const TLEData& tle, double groundLat, double groundLon, double groundAlt);
    void sendPacketToUDP(const QByteArray& packet);
    void updateDecoderPanel(int decoderIndex);
    void applySettingsToDemod();
};
#endif
