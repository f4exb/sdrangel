///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2016, 2018-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2020, 2022 Jon Beniston, M7RCE <jon@beniston.com>               //
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

#ifndef INCLUDE_FEATURE_REMOTECONTROLVISASENSORDIALOG_H
#define INCLUDE_FEATURE_REMOTECONTROLVISASENSORDIALOG_H

#include "ui_remotecontrolvisasensordialog.h"
#include "remotecontrolsettings.h"
#include "util/iot/visa.h"

class RemoteControlVISASensorDialog : public QDialog {
    Q_OBJECT

public:
    explicit RemoteControlVISASensorDialog(RemoteControlSettings *settings, RemoteControlDevice *device, VISADevice::VISASensor *sensor, bool add, QWidget* parent = 0);
    ~RemoteControlVISASensorDialog();

private slots:
    void accept();
    void on_name_textChanged(const QString &text);
    void on_id_textChanged(const QString &text);
    void on_id_textEdited(const QString &text);
    void on_getState_textChanged();

private:

    Ui::RemoteControlVISASensorDialog* ui;
    RemoteControlSettings *m_settings;
    RemoteControlDevice *m_device;
    VISADevice::VISASensor *m_sensor;
    bool m_add;
    bool m_userHasEditedId;

    void validate();

};

#endif // INCLUDE_FEATURE_REMOTECONTROLVISASENSORDIALOG_H
