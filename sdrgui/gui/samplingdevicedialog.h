///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// OpenGL interface modernization.                                               //
// See: http://doc.qt.io/qt-5/qopenglshaderprogram.html                          //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef SDRGUI_GUI_SAMPLINGDEVICEDIALOG_H_
#define SDRGUI_GUI_SAMPLINGDEVICEDIALOG_H_

#include <QDialog>
#include <vector>

namespace Ui {
    class SamplingDeviceDialog;
}

class SamplingDeviceDialog : public QDialog {
    Q_OBJECT

public:
    explicit SamplingDeviceDialog(bool rxElseTx, int deviceTabIndex, QWidget* parent = 0);
    ~SamplingDeviceDialog();
    int getSelectedDeviceIndex() const { return m_selectedDeviceIndex; }

private:
    Ui::SamplingDeviceDialog* ui;
    bool m_rxElseTx;
    int m_deviceTabIndex;
    int m_selectedDeviceIndex;
    std::vector<int> m_deviceIndexes;

private slots:
    void accept();
};

#endif /* SDRGUI_GUI_SAMPLINGDEVICEDIALOG_H_ */
