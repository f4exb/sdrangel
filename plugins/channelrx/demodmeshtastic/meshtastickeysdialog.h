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
#ifndef INCLUDE_MESHTASTICKEYSDIALOG_H
#define INCLUDE_MESHTASTICKEYSDIALOG_H

#include <QDialog>

namespace Ui {
class MeshtasticKeysDialog;
}

class MeshtasticKeysDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MeshtasticKeysDialog(QWidget* parent = nullptr);
    ~MeshtasticKeysDialog() override;

    void setKeySpecList(const QString& keySpecList);
    QString getKeySpecList() const;

private slots:
    void on_validate_clicked();
    void accept() override;

private:
    bool validateCurrentInput();

    Ui::MeshtasticKeysDialog* ui;
};

#endif // INCLUDE_MESHTASTICKEYSDIALOG_H
