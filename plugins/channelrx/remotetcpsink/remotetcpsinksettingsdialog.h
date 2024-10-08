///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_REMOTETCPSINKSETTINGSDIALOG_H
#define INCLUDE_REMOTETCPSINKSETTINGSDIALOG_H

#include <QDialog>

#include "availablechannelorfeaturehandler.h"

#include "remotetcpsinksettings.h"

namespace Ui {
    class RemoteTCPSinkSettingsDialog;
}

class RemoteTCPSinkSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit RemoteTCPSinkSettingsDialog(RemoteTCPSinkSettings *settings, QWidget* parent = nullptr);
    ~RemoteTCPSinkSettingsDialog();

    const QStringList& getSettingsKeys() const { return m_settingsKeys; };

private slots:
    void accept() override;
    void on_browseCertificate_clicked();
    void on_browseKey_clicked();
    void on_addIP_clicked();
    void on_removeIP_clicked();
    void on_publicListing_toggled();
    void on_publicAddress_textChanged();
    void on_compressor_currentIndexChanged(int index);
    void on_iqOnly_toggled(bool checked);
    void on_isotropic_toggled(bool checked);
    void on_rotator_currentIndexChanged(int index);
    void rotatorsChanged(const QStringList& renameFrom, const QStringList& renameTo);

private:
    Ui::RemoteTCPSinkSettingsDialog *ui;
    RemoteTCPSinkSettings *m_settings;
    QStringList m_settingsKeys;
    AvailableChannelOrFeatureHandler m_availableRotatorHandler;

    bool isValid();
    void displayValid();
    void displayEnabled();
    void updateRotatorList(const AvailableChannelOrFeatureList& rotators, const QStringList& renameFrom, const QStringList& renameTo);
};

#endif // INCLUDE_REMOTETCPSINKSETTINGSDIALOG_H
