///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_DABCARRIERPLOT_H
#define INCLUDE_DABCARRIERPLOT_H

#include <vector>

#include <QWidget>

// The carrier to noise ratio of each of the 1536 carriers, across the ensemble's bandwidth.
// Multipath shows as notches, and interference as dips on the carriers it covers.
class DABCarrierPlot : public QWidget
{
    Q_OBJECT

public:
    explicit DABCarrierPlot(QWidget *parent = nullptr);

    // dB by carrier index (k = -768..-1, then 1..768); empty to clear
    void setCarrierCN(const std::vector<float>& cn, float median);
    void clear();

    QSize sizeHint() const override { return QSize(360, 140); }
    QSize minimumSizeHint() const override { return QSize(200, 100); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    std::vector<float> m_cn;
    float m_median;
    float m_top;    // Of the scale, in dB
};

#endif // INCLUDE_DABCARRIERPLOT_H
