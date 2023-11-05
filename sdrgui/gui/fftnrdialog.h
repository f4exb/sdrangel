///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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
#ifndef SDRGUI_GUI_FFTNRDIALOG_H_
#define SDRGUI_GUI_FFTNRDIALOG_H_

#include <QDialog>

#include "dsp/fftnr.h"
#include "export.h"

namespace Ui {
    class FFTNRDialog;
}

class SDRGUI_API FFTNRDialog : public QDialog {
    Q_OBJECT
public:
    enum ValueChanged {
        ChangedScheme,
        ChangedAboveAvgFactor,
        ChangedSigmaFactor,
        ChangedNbPeaks,
        ChangedAlpha
    };

    explicit FFTNRDialog(QWidget* parent = nullptr);
    ~FFTNRDialog();

    void setScheme(FFTNoiseReduction::Scheme scheme);
    void setAboveAvgFactor(float aboveAvgFactor);
    void setSigmaFactor(float sigmaFactor);
    void setNbPeaks(int nbPeaks);
    void setAlpha(float alpha, int fftLength, int sampleRate);

    FFTNoiseReduction::Scheme getScheme() const { return m_scheme; }
    float getAboveAvgFactor() const { return m_aboveAvgFactor; }
    float getSigmaFactor() const { return m_sigmaFactor; }
    int getNbPeaks() const { return m_nbPeaks; }
    float getAlpha() const { return m_alpha; }

signals:
    void valueChanged(int valueChanged);

private:
    Ui::FFTNRDialog *ui;
    FFTNoiseReduction::Scheme m_scheme;
    float m_aboveAvgFactor; //!< above average factor
    float m_sigmaFactor; //!< sigma multiplicator for average + std deviation
    int m_nbPeaks; //!< number of peaks (peaks scheme)
    float m_alpha; //!< parameter EMA alpha factor
    int m_flen; //!< FFT filter FFT length used to display time constant
    int m_sampleRate; //!< Sample rate used to display time constant
    void updateScheme();

private slots:
    void on_scheme_currentIndexChanged(int index);
    void on_genParam_valueChanged(int value);
    void on_alpha_valueChanged(int value);
};

#endif
