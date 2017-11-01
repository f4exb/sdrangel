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
    m_pluginManager(0)
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

QPushButton *SamplingDeviceControl::getDeviceSelectionConfirm()
{
    return ui->deviceConfirm;
}

QComboBox *SamplingDeviceControl::getChannelSelector()
{
    return ui->channelSelect;
}

QPushButton *SamplingDeviceControl::getAddChannelButton()
{
    return ui->addChannel;
}
