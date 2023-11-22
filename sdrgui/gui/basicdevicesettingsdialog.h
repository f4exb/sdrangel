///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017-2018 Edouard Griffiths, F4EXB <f4exb06@gmail.com>              //
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
    void setReplayBytesPerSecond(int bytesPerSecond);
    void setReplayLength(float replayLength);
    float getReplayLength() const { return m_replayLength; }
    void setReplayStep(float replayStep);
    float getReplayStep() const { return m_replayStep; }

private slots:
    void on_reverseAPI_toggled(bool checked);
    void on_reverseAPIAddress_editingFinished();
    void on_reverseAPIPort_editingFinished();
    void on_reverseAPIDeviceIndex_editingFinished();
    void on_presets_clicked();
    void on_replayLength_valueChanged(double value);
    void on_replayStep_valueChanged(double value);
    void accept();

private:
    Ui::BasicDeviceSettingsDialog *ui;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    bool m_hasChanged;
    int m_replayBytesPerSecond;
    float m_replayLength;
    float m_replayStep;
};

#endif // BASICDEVICESETTINGSDIALOG_H
