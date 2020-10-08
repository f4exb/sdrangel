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

#include "ieee_802_15_4_modrepeatdialog.h"
#include "ieee_802_15_4_modsettings.h"
#include <QLineEdit>

IEEE_802_15_4_ModRepeatDialog::IEEE_802_15_4_ModRepeatDialog(float repeatDelay, int repeatCount, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::IEEE_802_15_4_ModRepeatDialog)
{
    ui->setupUi(this);
    ui->repeatDelay->setValue(repeatDelay);
    QLineEdit *edit = ui->repeatCount->lineEdit();
    if (edit)
    {
        if (repeatCount == IEEE_802_15_4_ModSettings::infinitePackets)
            edit->setText("Infinite");
        else
            edit->setText(QString("%1").arg(repeatCount));
    }
}

IEEE_802_15_4_ModRepeatDialog::~IEEE_802_15_4_ModRepeatDialog()
{
    delete ui;
}

void IEEE_802_15_4_ModRepeatDialog::accept()
{
    m_repeatDelay = ui->repeatDelay->value();
    QString text =  ui->repeatCount->currentText();
    if (!text.compare(QString("Infinite"), Qt::CaseInsensitive))
        m_repeatCount = IEEE_802_15_4_ModSettings::infinitePackets;
    else
        m_repeatCount = text.toUInt();
    QDialog::accept();
}
