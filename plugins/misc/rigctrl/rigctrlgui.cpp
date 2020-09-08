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

#include "rigctrlgui.h"
#include "ui_rigctrlgui.h"
#include <QDebug>

RigCtrlGUI::RigCtrlGUI(RigCtrl *rigCtrl, QWidget* parent) :
    m_rigCtrl(rigCtrl),
	QDialog(parent),
	ui(new Ui::RigCtrlGUI)
{
	ui->setupUi(this);
    m_rigCtrl->getSettings(&m_settings);
    ui->enable->setChecked(m_settings.m_enabled);
    ui->api->setText(QString(m_settings.m_APIAddress));
    ui->rigCtrlPort->setValue(m_settings.m_rigCtrlPort);
    ui->maxFrequencyOffset->setValue(m_settings.m_maxFrequencyOffset);
    ui->deviceIndex->setValue(m_settings.m_deviceIndex);
    ui->channelIndex->setValue(m_settings.m_channelIndex);
}

RigCtrlGUI::~RigCtrlGUI()
{
	delete ui;
}

void RigCtrlGUI::accept()
{
    m_settings.m_enabled = ui->enable->isChecked();
    m_settings.m_APIAddress = ui->api->text();
    m_settings.m_rigCtrlPort = ui->rigCtrlPort->value();
    m_settings.m_maxFrequencyOffset = ui->maxFrequencyOffset->value();
    m_settings.m_deviceIndex = ui->deviceIndex->value();
    m_settings.m_channelIndex = ui->channelIndex->value();
    m_rigCtrl->setSettings(&m_settings);
    QDialog::accept();
}
