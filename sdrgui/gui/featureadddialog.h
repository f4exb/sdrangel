///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRGUI_GUI_FEATUREADDDIALOG_H_
#define SDRGUI_GUI_FEATUREADDDIALOG_H_

#include <QDialog>
#include <QStringList>
#include <vector>

#include "export.h"

class QAbstractButton;

namespace Ui {
    class FeatureAddDialog;
}

class SDRGUI_API FeatureAddDialog : public QDialog {
    Q_OBJECT
public:
    explicit FeatureAddDialog(QWidget* parent = nullptr);
    ~FeatureAddDialog();

    void resetFeatureNames();
    void addFeatureNames(const QStringList& featureNames);

private:
    Ui::FeatureAddDialog* ui;
    std::vector<int> m_featureIndexes;

private slots:
    void apply(QAbstractButton*);

signals:
    void addFeature(int);
};

#endif /* SDRGUI_GUI_FEATUREADDDIALOG_H_ */
