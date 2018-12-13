#ifndef BASICCHANNELSETTINGSDIALOG_H
#define BASICCHANNELSETTINGSDIALOG_H

#include <QDialog>

#include "export.h"

namespace Ui {
    class BasicChannelSettingsDialog;
}

class ChannelMarker;

class SDRGUI_API BasicChannelSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BasicChannelSettingsDialog(ChannelMarker* marker, QWidget *parent = 0);
    ~BasicChannelSettingsDialog();
    bool hasChanged() const { return m_hasChanged; }
    bool useReverseAPI() const { return m_useReverseAPI; }
    const QString& getReverseAPIAddress() const { return m_reverseAPIAddress; }
    uint16_t getReverseAPIPort() const { return m_reverseAPIPort; }
    void setUseReverseAPI(bool useReverseAPI);
    void setReverseAPIAddress(const QString& address);
    void setReverseAPIPort(uint16_t port);

private slots:
    void on_colorBtn_clicked();
    void on_reverseAPI_toggled(bool checked);
    void on_reverseAPIAddress_returnPressed();
    void on_reverseAPIPort_returnPressed();
    void accept();

private:
    Ui::BasicChannelSettingsDialog *ui;
    ChannelMarker* m_channelMarker;
    QColor m_color;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    bool m_hasChanged;

    void paintColor();
};

#endif // BASICCHANNELSETTINGSDIALOG_H
