///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "gui/samplingdevicecontrol.h"
#include "gui/pluginsdialog.h"
#include "plugin/pluginmanager.h"
#include "ui_samplingdevicecontrol.h"


SamplingDeviceControl::SamplingDeviceControl(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::SamplingDeviceControl),
    m_pluginManager(0),
    m_deviceAPI(0)
{
    ui->setupUi(this);
}

SamplingDeviceControl::~SamplingDeviceControl()
{
    delete ui;
}

QComboBox *SamplingDeviceControl::getDeviceSelector()
{
    return ui->deviceSelect;
}

void SamplingDeviceControl::populateChannelSelector()
{
    if (m_pluginManager)
    {
        m_pluginManager->populateChannelComboBox(ui->channelSelect);
    }
}

void SamplingDeviceControl::on_showLoadedPlugins_clicked(bool checked)
{
    if (m_pluginManager)
    {
        PluginsDialog pluginsDialog(m_pluginManager, this);
        pluginsDialog.exec();
    }
}

void SamplingDeviceControl::on_addChannel_clicked(bool checked)
{
    if (m_pluginManager)
    {
        m_pluginManager->createChannelInstance(ui->channelSelect->currentIndex(), m_deviceAPI);
    }
}

