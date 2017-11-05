///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// OpenGL interface modernization.                                               //
// See: http://doc.qt.io/qt-5/qopenglshaderprogram.html                          //
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

#include "externalclockdialog.h"
#include "externalclockbutton.h"

ExternalClockButton::ExternalClockButton(QWidget* parent) :
    QPushButton(parent),
    m_externalClockFrequency(0),
    m_externalClockActive(false)
{
    setObjectName("ExternalClockButton");
    connect(this, SIGNAL(clicked()), this, SLOT(onClicked()));
}

void ExternalClockButton::onClicked()
{
    ExternalClockDialog externalClockDialog(m_externalClockFrequency, m_externalClockActive, this);
    externalClockDialog.exec();
    updateState();
}

void ExternalClockButton::updateState()
{
    setToolTip(tr("External clock dialog. External clock frequency %1 MHz %2")
            .arg(m_externalClockFrequency/1000000.0)
            .arg(m_externalClockActive ? "enabled" : "disabled"));

    if(m_externalClockActive)
    {
        setStyleSheet("ExternalClockButton { background:rgb(128, 70, 0); }");
    }
    else
    {
        setStyleSheet("ExternalClockButton { background:rgb(48, 48, 48); }");
    }

}
