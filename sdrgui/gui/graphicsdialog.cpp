///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>

#include "graphicsdialog.h"
#include "ui_graphicsdialog.h"

GraphicsDialog::GraphicsDialog(MainSettings& mainSettings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::GraphicsDialog),
    m_mainSettings(mainSettings)
{
    ui->setupUi(this);
    int samples = m_mainSettings.getMultisampling();
    if (samples == 0) {
        ui->multisampling->setCurrentText("Off");
    } else {
        ui->multisampling->setCurrentText(QString::number(samples));
    }
    samples = m_mainSettings.getMapMultisampling();
    if (samples == 0) {
        ui->mapMultisampling->setCurrentText("Off");
    } else {
        ui->mapMultisampling->setCurrentText(QString::number(samples));
    }
    ui->mapSmoothing->setChecked(m_mainSettings.getMapSmoothing());

    QSettings settings;
    m_initScaleFactor = settings.value("graphics.ui_scale_factor", "1").toFloat();
    ui->uiScaleFactor->setValue((int)(m_initScaleFactor * 100.0f));
}

GraphicsDialog::~GraphicsDialog()
{
    delete ui;
}

void GraphicsDialog::accept()
{
    m_mainSettings.setMultisampling(ui->multisampling->currentText().toInt());
    m_mainSettings.setMapMultisampling(ui->mapMultisampling->currentText().toInt());
    m_mainSettings.setMapSmoothing(ui->mapSmoothing->isChecked());

    // We use QSetting rather than MainSettings, as we need to set QT_SCALE_FACTOR
    // before MainSettings are currently read
    QSettings settings;
    bool showRestart = false;

    float scaleFactor = ui->uiScaleFactor->value() / 100.0f;
    if (m_initScaleFactor != scaleFactor)
    {
        QString newScaleFactor = QString("%1").arg(scaleFactor);
        settings.setValue("graphics.ui_scale_factor", newScaleFactor);
        showRestart = true;
    }

    if (showRestart)
    {
        QMessageBox msgBox;
        msgBox.setText("Please restart SDRangel to use the new UI settings.");
        msgBox.exec();
    }
    QDialog::accept();
}

void GraphicsDialog::on_uiScaleFactor_valueChanged(int value)
{
    ui->uiScaleFactorText->setText(QString("%1%").arg(value));
}
