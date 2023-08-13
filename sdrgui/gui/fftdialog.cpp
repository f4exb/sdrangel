///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include "dsp/fftengine.h"

#include "fftdialog.h"
#include "ui_fftdialog.h"

FFTDialog::FFTDialog(MainSettings& mainSettings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::FFTDialog),
    m_mainSettings(mainSettings)
{
    ui->setupUi(this);

    for (const auto& engine: FFTEngine::getAllNames()) {
        ui->fftEngine->addItem(engine);
    }
    int idx = ui->fftEngine->findText(m_mainSettings.getFFTEngine());
    if (idx != -1) {
        ui->fftEngine->setCurrentIndex(idx);
    }
}

FFTDialog::~FFTDialog()
{
    delete ui;
}

void FFTDialog::accept()
{
    m_mainSettings.setFFTEngine(ui->fftEngine->currentText());
    QDialog::accept();
}
