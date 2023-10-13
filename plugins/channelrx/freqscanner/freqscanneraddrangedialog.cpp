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

#include <cmath>

#include "freqscanneraddrangedialog.h"
#include "ui_freqscanneraddrangedialog.h"

FreqScannerAddRangeDialog::FreqScannerAddRangeDialog(int step, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::FreqScannerAddRangeDialog)
{
    ui->setupUi(this);

    ui->start->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->start->setValueRange(false, 11, 0, 99999999999);
    ui->stop->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->stop->setValueRange(false, 11, 0, 99999999999);

    on_preset_currentTextChanged("Airband");

    //ui->step->setCurrentText(QString::number(step));
}

FreqScannerAddRangeDialog::~FreqScannerAddRangeDialog()
{
    delete ui;
}

void FreqScannerAddRangeDialog::accept()
{
    if (ui->preset->currentText() == "Digital Selective Calling")
    {
        // From ITU M.541
        static const QList<qint64> dscFreqs = {
            2177000, 2189500,
            4208000, 4208500, 4209000,
            6312500, 6313000,
            8415000, 8415500, 8416000,
            12577500, 12578000, 12578500,
            16805000, 16805500, 16806000, 18898500, 18899000, 18899500,
            22374500, 22375000, 22375500,
            25208500, 25209000, 25209500
        };
        m_frequencies.append(dscFreqs);
    }
    else if (ui->preset->currentText() == "DAB")
    {
        static const QList<qint64> dabFreqs = {
            174928000, 176640000, 178352000, 180064000,
            181936000, 183648000, 185360000, 187072000,
            188928000, 190640000, 192352000, 194064000,
            195936000, 197648000, 199360000, 201072000,
            202928000, 204640000, 206352000, 208064000,
            209936000, 211648000, 213360000, 215072000,
            216928000, 218640000, 220352000, 222064000,
            223936000, 225648000, 227360000, 229072000,
            230784000, 232496000, 234208000, 235776000,
            237448000, 239200000
        };
        m_frequencies.append(dabFreqs);
    }
    else
    {
        qint64 start = ui->start->getValue();
        qint64 stop = ui->stop->getValue();
        int step = ui->step->currentText().toInt();

        if ((start <= stop) && (step > 0))
        {
            if (step == 8333)
            {
                double fstep = 8333 + 1.0/3.0; // float will give incorrect results
                for (double f = start; f <= stop; f += fstep) {
                    m_frequencies.append(std::round(f));
                }
            }
            else
            {
                for (qint64 f = start; f <= stop; f += step) {
                    m_frequencies.append(f);
                }
            }
        }
    }

    QDialog::accept();
}

void FreqScannerAddRangeDialog::on_preset_currentTextChanged(const QString& text)
{
    bool enableManAdjust = true;
    if (text == "Airband")
    {
        ui->start->setValue(118000000);
        ui->stop->setValue(137000000);
        ui->step->setCurrentText("25000");
    }
    else if (text == "Broadcast FM")
    {
        ui->start->setValue(87500000);
        ui->stop->setValue(108000000);
        ui->step->setCurrentText("100000");
    }
    else if (text == "DAB")
    {
        enableManAdjust = false;
    }
    else if (text == "Marine")
    {
        ui->start->setValue(156000000);
        ui->stop->setValue(162150000);
        ui->step->setCurrentText("25000");
    }
    else if (text == "Digital Selective Calling")
    {
        enableManAdjust = false;
    }
    ui->start->setEnabled(enableManAdjust);
    ui->stop->setEnabled(enableManAdjust);
    ui->step->setEnabled(enableManAdjust);
}
