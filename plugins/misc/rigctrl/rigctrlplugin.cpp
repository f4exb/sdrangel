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

#include <QtPlugin>
#include <QtDebug>

#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"

#include "rigctrl.h"
#ifdef SERVER_MODE
#else
#include <QtWidgets/QAction>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include "mainwindow.h"
#include "rigctrlgui.h"
#endif
#include "rigctrlplugin.h"

const PluginDescriptor RigCtrlPlugin::m_pluginDescriptor = {
    QString("rigctrl"),
	QString("rigctrl Server"),
	QString("4.15.5"),
	QString("(c) Jon Beniston, M7RCE"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/srcejon/sdrangel/tree/rigctrl")
};

RigCtrlPlugin::RigCtrlPlugin(RigCtrl *rigCtrl, QObject* parent) :
    m_rigCtrl(rigCtrl),
	QObject(parent)
{
}

const PluginDescriptor& RigCtrlPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void RigCtrlPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_rigCtrl = new RigCtrl();
    pluginAPI->registerMiscPlugin(RIGCTRL_DEVICE_TYPE_ID, this);
}


#ifdef SERVER_MODE
bool RigCtrlPlugin::createTopLevelGUI(
        )
{
    return true;
}

void RigCtrlPlugin::showRigCtrlUI()
{
}

#else
bool RigCtrlPlugin::createTopLevelGUI()
{
    m_mainWindow = MainWindow::getInstance();
    m_rigCtrl->setAPIBaseURI(QString("http://%1:%2/sdrangel").arg(m_mainWindow->getAPIHost()).arg(m_mainWindow->getAPIPort()));
    QMenuBar *menuBar = m_mainWindow->menuBar();

    QMenu *prefMenu = menuBar->findChild<QMenu *>("menuPreferences", Qt::FindDirectChildrenOnly);
    if (prefMenu == nullptr) {
        qDebug() << "RigCtrlPlugin::createTopLevelGUI - Failed to find Preferences menu";
        return false;
    }

    QAction *prefAction;
    prefAction = new QAction(tr("rigctrl"), this);
    prefAction->setStatusTip(tr("Display preferences for rigctrl"));
    prefMenu->addAction(prefAction);
    connect(prefAction, &QAction::triggered, this, &RigCtrlPlugin::showRigCtrlUI);

    return true;
}

void RigCtrlPlugin::showRigCtrlUI()
{
	RigCtrlGUI dlg(m_rigCtrl, m_mainWindow);
	dlg.exec();
}

#endif

QByteArray RigCtrlPlugin::serializeGlobalSettings() const
{
    return m_rigCtrl->serialize();
}

bool RigCtrlPlugin::deserializeGlobalSettings(const QByteArray& data)
{
    return m_rigCtrl->deserialize(data);
}
