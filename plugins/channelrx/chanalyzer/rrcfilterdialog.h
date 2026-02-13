///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#ifndef PLUGINS_CHANNELRX_CHANALYZER_RRCFILTERDIALOG_H_
#define PLUGINS_CHANNELRX_CHANALYZER_RRCFILTERDIALOG_H_

#include <QDialog>
#include "chanalyzersettings.h"


namespace Ui {
    class RRCFilterDialog;
}

class RRCFilterDialog : public QDialog {
    Q_OBJECT
public:
    enum ValueChanged {
        ChangedRRCType,
        ChangedRRCSymbolSpan,
        ChangedRRCNormalization,
        ChangedRRCFFTLog2Size,
    };

    explicit RRCFilterDialog(QWidget* parent = nullptr);
    ~RRCFilterDialog() override;

    void setRRCType(ChannelAnalyzerSettings::RRCType rrcType);
    void setRRCSymbolSpan(int symbolSpan);
    void setRRCNormalization(ChannelAnalyzerSettings::RRCNormalization normalization);
    void setRRCFFTLog2Size(int log2Size);

    ChannelAnalyzerSettings::RRCType getRRCType() const { return m_rrcType; }
    int getRRCSymbolSpan() const { return m_rrcSymbolSpan; }
    ChannelAnalyzerSettings::RRCNormalization getRRCNormalization() const { return m_rrcNormalization; }
    int getRRCFFTLog2Size() const { return m_rrcFFTLog2Size; }

signals:
    void valueChanged(int valueChanged);

private:
    Ui::RRCFilterDialog* ui;
    ChannelAnalyzerSettings::RRCType m_rrcType;
    int m_rrcSymbolSpan;
    ChannelAnalyzerSettings::RRCNormalization m_rrcNormalization;
    int m_rrcFFTLog2Size;

    void on_firGroup_clicked(bool checked);
    void on_fftGroup_clicked(bool checked);
    void on_symbolSpan_valueChanged(int value);
    void on_normalization_currentIndexChanged(int index);
    void on_fftLog2Size_currentIndexChanged(int index);
    void makeUIConnections();
};

#endif /* PLUGINS_CHANNELRX_CHANALYZER_RRCFILTERDIALOG_H_ */
