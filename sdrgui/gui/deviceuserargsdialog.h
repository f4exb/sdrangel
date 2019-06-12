///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRGUI_GUI_DEVICEUSERARGSDIALOG_H
#define SDRGUI_GUI_DEVICEUSERARGSDIALOG_H

#include <QDialog>
#include <QSet>

#include "plugin/plugininterface.h"
#include "export.h"

class QTreeWidgetItem;
class DeviceEnumerator;
struct DeviceUserArgs;

namespace Ui {
	class DeviceUserArgsDialog;
}

class SDRGUI_API DeviceUserArgsDialog : public QDialog {
	Q_OBJECT

public:
	explicit DeviceUserArgsDialog(
        DeviceEnumerator* deviceEnumerator,
        DeviceUserArgs& hardwareDeviceUserArgs,
        QWidget* parent = 0);
	~DeviceUserArgsDialog();

private:
    struct HWDeviceReference {
        QString m_hardwareId;
        int m_sequence;
        QString m_description;

        bool operator==(const HWDeviceReference& rhs) {
            return (m_hardwareId == rhs.m_hardwareId) && (m_sequence == rhs.m_sequence);
        }
    };

	Ui::DeviceUserArgsDialog* ui;
    DeviceEnumerator* m_deviceEnumerator;
    DeviceUserArgs& m_hardwareDeviceUserArgs;
    std::vector<HWDeviceReference> m_availableHWDevices;
    QMap<QString, QString> m_argsByDeviceCopy;

    void pushHWDeviceReference(const PluginInterface::SamplingDevice *samplingDevice);
    void displayArgsByDevice();

private slots:
	void accept();
	void reject();
    void on_importDevice_clicked(bool checked);
    void on_deleteArgs_clicked(bool checked);
    void on_argStringEdit_returnPressed();
};

#endif // SDRGUI_GUI_DEVICEUSERARGSDIALOG_H