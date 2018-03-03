#ifndef BASICCHANNELSETTINGSDIALOG_H
#define BASICCHANNELSETTINGSDIALOG_H

#include <QDialog>

#include "util/export.h"

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

private slots:
    void on_colorBtn_clicked();
    void accept();

private:
    Ui::BasicChannelSettingsDialog *ui;
    ChannelMarker* m_channelMarker;
    QColor m_color;
    bool m_hasChanged;

    void paintColor();
};

#endif // BASICCHANNELSETTINGSDIALOG_H
