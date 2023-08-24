#ifndef BASICDEVICESETTINGSDIALOG_H
#define BASICDEVICESETTINGSDIALOG_H

#include <QDialog>

#include "../../exports/export.h"

namespace Ui {
    class BasicDeviceSettingsDialog;
}

class SDRGUI_API BasicDeviceSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BasicDeviceSettingsDialog(QWidget *parent = 0);
    ~BasicDeviceSettingsDialog();
    bool hasChanged() const { return m_hasChanged; }
    bool useReverseAPI() const { return m_useReverseAPI; }
    const QString& getReverseAPIAddress() const { return m_reverseAPIAddress; }
    uint16_t getReverseAPIPort() const { return m_reverseAPIPort; }
    uint16_t getReverseAPIDeviceIndex() const { return m_reverseAPIDeviceIndex; }
    void setUseReverseAPI(bool useReverseAPI);
    void setReverseAPIAddress(const QString& address);
    void setReverseAPIPort(uint16_t port);
    void setReverseAPIDeviceIndex(uint16_t deviceIndex);

private slots:
    void on_reverseAPI_toggled(bool checked);
    void on_reverseAPIAddress_editingFinished();
    void on_reverseAPIPort_editingFinished();
    void on_reverseAPIDeviceIndex_editingFinished();
    void on_presets_clicked();
    void accept();

private:
    Ui::BasicDeviceSettingsDialog *ui;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    bool m_hasChanged;
};

#endif // BASICDEVICESETTINGSDIALOG_H
