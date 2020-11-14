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
#include "device/deviceuserargs.h"
#include "export.h"

class QTreeWidgetItem;
class DeviceEnumerator;

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
    DeviceUserArgs m_deviceUserArgsCopy;
    QString m_xDeviceHardwareId;
    unsigned int m_xDeviceSequence;

    void pushHWDeviceReference(const PluginInterface::SamplingDevice *samplingDevice);
    void displayArgsByDevice();

private slots:
	void accept();
	void reject();
    void on_importDevice_clicked();
    void on_deleteArgs_clicked();
    void on_argsTree_currentItemChanged(QTreeWidgetItem* currentItem, QTreeWidgetItem* previousItem);
    void on_argStringEdit_editingFinished();
    void on_addDeviceHwIDEdit_editingFinished();
    void on_addDeviceSeqEdit_editingFinished();
    void on_addDevice_clicked();
};

#endif // SDRGUI_GUI_DEVICEUSERARGSDIALOG_H
