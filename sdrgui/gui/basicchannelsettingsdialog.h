///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017-2018, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>        //
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                         //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#ifndef BASICCHANNELSETTINGSDIALOG_H
#define BASICCHANNELSETTINGSDIALOG_H

#include <QDialog>

#include "../../exports/export.h"

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
    uint16_t getReverseAPIDeviceIndex() const { return m_reverseAPIDeviceIndex; }
    uint16_t getReverseAPIChannelIndex() const { return m_reverseAPIChannelIndex; }
    int getSelectedStreamIndex() const { return m_streamIndex; }
    void setUseReverseAPI(bool useReverseAPI);
    void setReverseAPIAddress(const QString& address);
    void setReverseAPIPort(uint16_t port);
    void setReverseAPIDeviceIndex(uint16_t deviceIndex);
    void setReverseAPIChannelIndex(uint16_t channelIndex);
    void setNumberOfStreams(int numberOfStreams);
    void setStreamIndex(int index);
    void setDefaultTitle(const QString& title) { m_defaultTitle = title; }

private slots:
    void on_titleReset_clicked();
    void on_colorBtn_clicked();
    void on_reverseAPI_toggled(bool checked);
    void on_reverseAPIAddress_editingFinished();
    void on_reverseAPIPort_editingFinished();
    void on_reverseAPIDeviceIndex_editingFinished();
    void on_reverseAPIChannelIndex_editingFinished();
    void on_streamIndex_valueChanged(int value);
    void on_presets_clicked();
    void accept();

private:
    Ui::BasicChannelSettingsDialog *ui;
    ChannelMarker* m_channelMarker;
    QColor m_color;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    QString m_defaultTitle;
    int m_streamIndex;
    bool m_hasChanged;

    void paintColor();
};

#endif // BASICCHANNELSETTINGSDIALOG_H
