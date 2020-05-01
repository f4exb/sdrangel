///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef WSSPECTRUMSETTINGSDIALOG_H
#define WSSPECTRUMSETTINGSDIALOG_H

#include <QDialog>

#include "../../exports/export.h"

namespace Ui {
    class WebsocketSpectrumSettingsDialog;
}

class SDRGUI_API WebsocketSpectrumSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WebsocketSpectrumSettingsDialog(QWidget *parent = nullptr);
    ~WebsocketSpectrumSettingsDialog();
    bool hasChanged() const { return m_hasChanged; }
    const QString& getAddress() const { return m_address; }
    uint16_t getPort() const { return m_port; }
    void setAddress(const QString& address);
    void setPort(uint16_t port);

private slots:
    void on_address_editingFinished();
    void on_port_editingFinished();
    void accept();

private:
    Ui::WebsocketSpectrumSettingsDialog *ui;
    QString m_address;
    uint16_t m_port;
    bool m_hasChanged;
};

#endif // BASICDEVICESETTINGSDIALOG_H
