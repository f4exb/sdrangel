///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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
#include <QMessageBox>

#include "meshtastickeysdialog.h"
#include "ui_meshtastickeysdialog.h"
#include "meshtasticpacket.h"

MeshtasticKeysDialog::MeshtasticKeysDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MeshtasticKeysDialog)
{
    ui->setupUi(this);
    validateCurrentInput();
}

MeshtasticKeysDialog::~MeshtasticKeysDialog()
{
    delete ui;
}

void MeshtasticKeysDialog::setKeySpecList(const QString& keySpecList)
{
    ui->keyEditor->setPlainText(keySpecList);
    validateCurrentInput();
}

QString MeshtasticKeysDialog::getKeySpecList() const
{
    return ui->keyEditor->toPlainText().trimmed();
}

void MeshtasticKeysDialog::on_validate_clicked()
{
    validateCurrentInput();
}

void MeshtasticKeysDialog::accept()
{
    if (!validateCurrentInput())
    {
        QMessageBox::warning(this, tr("Invalid Keys"), tr("Fix the Meshtastic key list before saving."));
        return;
    }

    QDialog::accept();
}

bool MeshtasticKeysDialog::validateCurrentInput()
{
    const QString keyText = ui->keyEditor->toPlainText().trimmed();

    if (keyText.isEmpty())
    {
        ui->statusLabel->setStyleSheet("QLabel { color: #bbbbbb; }");
        ui->statusLabel->setText(tr("No custom keys set. Decoder will use environment/default keys."));
        return true;
    }

    QString error;
    int keyCount = 0;

    if (!modemmeshtastic::Packet::validateKeySpecList(keyText, error, &keyCount))
    {
        ui->statusLabel->setStyleSheet("QLabel { color: #ff5555; }");
        ui->statusLabel->setText(tr("Invalid key list: %1").arg(error));
        return false;
    }

    ui->statusLabel->setStyleSheet("QLabel { color: #7cd67c; }");
    ui->statusLabel->setText(tr("Valid: %1 key(s) parsed").arg(keyCount));
    return true;
}
