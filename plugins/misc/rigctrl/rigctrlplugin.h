///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_RIGCTRLPLUGIN_H
#define INCLUDE_RIGCTRLPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"
#include "rigctrl.h"

class PluginAPI;
class QMainWindow;

#define RIGCTRL_DEVICE_TYPE_ID "sdrangel.misc.rigctrl"

class RigCtrlPlugin : public QObject, public PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID RIGCTRL_DEVICE_TYPE_ID)

public:
	explicit RigCtrlPlugin(RigCtrl *rigCtrl = NULL, QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	virtual bool createTopLevelGUI(QMainWindow* mainWindow);
    virtual QByteArray serializeGlobalSettings() const;
	virtual bool deserializeGlobalSettings(const QByteArray& data);

private slots:
    void showRigCtrlUI();

private:
	static const PluginDescriptor m_pluginDescriptor;
    QMainWindow* m_mainWindow;
    RigCtrl *m_rigCtrl;
};

#endif // INCLUDE_RIGCTRLPLUGIN_H
