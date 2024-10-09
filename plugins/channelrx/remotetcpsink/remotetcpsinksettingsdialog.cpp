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

#include <QFileDialog>
#include <QPushButton>

#include "remotetcpsinksettingsdialog.h"
#include "ui_remotetcpsinksettingsdialog.h"

RemoteTCPSinkSettingsDialog::RemoteTCPSinkSettingsDialog(RemoteTCPSinkSettings *settings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::RemoteTCPSinkSettingsDialog),
    m_settings(settings),
    m_availableRotatorHandler({"sdrangel.feature.gs232controller"})
{
    ui->setupUi(this);

    ui->maxClients->setValue(m_settings->m_maxClients);
    ui->timeLimit->setValue(m_settings->m_timeLimit);
    ui->maxSampleRate->setValue(m_settings->m_maxSampleRate);
    ui->iqOnly->setChecked(m_settings->m_iqOnly);

    ui->compressor->setCurrentIndex((int) m_settings->m_compression);
    ui->compressionLevel->setValue(m_settings->m_compressionLevel);
    ui->blockSize->setCurrentIndex(ui->blockSize->findText(QString::number(m_settings->m_blockSize)));

    ui->certificate->setText(m_settings->m_certificate);
    ui->key->setText(m_settings->m_key);

    ui->publicListing->setChecked(m_settings->m_public);
    ui->publicAddress->setText(m_settings->m_publicAddress);
    ui->publicPort->setValue(m_settings->m_publicPort);
    ui->minFrequency->setValue(m_settings->m_minFrequency / 1000000);
    ui->maxFrequency->setValue(m_settings->m_maxFrequency / 1000000);
    ui->antenna->setText(m_settings->m_antenna);
    ui->location->setText(m_settings->m_location);
    ui->isotropic->setChecked(m_settings->m_isotropic);
    ui->azimuth->setValue(m_settings->m_azimuth);
    ui->elevation->setValue(m_settings->m_elevation);
    ui->rotator->setCurrentText(m_settings->m_rotator);

    for (const auto& ip : m_settings->m_ipBlacklist) {
        ui->ipBlacklist->addItem(ip);
    }

    QObject::connect(
        &m_availableRotatorHandler,
        &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged,
        this,
        &RemoteTCPSinkSettingsDialog::rotatorsChanged
    );
    m_availableRotatorHandler.scanAvailableChannelsAndFeatures();
}

RemoteTCPSinkSettingsDialog::~RemoteTCPSinkSettingsDialog()
{
    delete ui;
}

void RemoteTCPSinkSettingsDialog::accept()
{
    if (!isValid()) {
        return;
    }

    QDialog::accept();

    if (ui->maxClients->value() != m_settings->m_maxClients)
    {
        m_settings->m_maxClients = ui->maxClients->value();
        m_settingsKeys.append("maxClients");
    }
    if (ui->timeLimit->value() != m_settings->m_timeLimit)
    {
        m_settings->m_timeLimit = ui->timeLimit->value();
        m_settingsKeys.append("timeLimit");
    }
    if (ui->maxSampleRate->value() != m_settings->m_maxSampleRate)
    {
        m_settings->m_maxSampleRate = ui->maxSampleRate->value();
        m_settingsKeys.append("maxSampleRate");
    }
    if (ui->iqOnly->isChecked() != m_settings->m_iqOnly)
    {
        m_settings->m_iqOnly = ui->iqOnly->isChecked();
        m_settingsKeys.append("iqOnly");
    }
    RemoteTCPSinkSettings::Compressor compressor = (RemoteTCPSinkSettings::Compressor) ui->compressor->currentIndex();
    if (compressor != m_settings->m_compression)
    {
        m_settings->m_compression = compressor;
        m_settingsKeys.append("compression");
    }
    if (ui->compressionLevel->value() != m_settings->m_compressionLevel)
    {
        m_settings->m_compressionLevel = ui->compressionLevel->value();
        m_settingsKeys.append("compressionLevel");
    }
    int blockSize = ui->blockSize->currentText().toInt();
    if (blockSize != m_settings->m_blockSize)
    {
        m_settings->m_blockSize = blockSize;
        m_settingsKeys.append("blockSize");
    }
    if (ui->certificate->text() != m_settings->m_certificate)
    {
        m_settings->m_certificate = ui->certificate->text();
        m_settingsKeys.append("certificate");
    }
    if (ui->key->text() != m_settings->m_key)
    {
        m_settings->m_key = ui->key->text();
        m_settingsKeys.append("key");
    }
    if (ui->publicListing->isChecked() != m_settings->m_public)
    {
        m_settings->m_public = ui->publicListing->isChecked();
        m_settingsKeys.append("public");
    }
    if (ui->publicAddress->text() != m_settings->m_publicAddress)
    {
        m_settings->m_publicAddress = ui->publicAddress->text();
        m_settingsKeys.append("publicAddress");
    }
    if (ui->publicPort->value() != m_settings->m_publicPort)
    {
        m_settings->m_publicPort = ui->publicPort->value();
        m_settingsKeys.append("publicPort");
    }
    qint64 minFrequency = ui->minFrequency->value() * 1000000;
    if (minFrequency != m_settings->m_minFrequency)
    {
        m_settings->m_minFrequency = minFrequency;
        m_settingsKeys.append("minFrequency");
    }
    qint64 maxFrequency = ui->maxFrequency->value() * 1000000;
    if (maxFrequency != m_settings->m_maxFrequency)
    {
        m_settings->m_maxFrequency = maxFrequency;
        m_settingsKeys.append("maxFrequency");
    }
    if (ui->antenna->text() != m_settings->m_antenna)
    {
        m_settings->m_antenna = ui->antenna->text();
        m_settingsKeys.append("antenna");
    }
    if (ui->location->text() != m_settings->m_location)
    {
        m_settings->m_location = ui->location->text();
        m_settingsKeys.append("location");
    }
    if (ui->isotropic->isChecked() != m_settings->m_isotropic)
    {
        m_settings->m_isotropic = ui->isotropic->isChecked();
        m_settingsKeys.append("isotropic");
    }
    if (ui->azimuth->value() != m_settings->m_azimuth)
    {
        m_settings->m_azimuth = ui->azimuth->value();
        m_settingsKeys.append("azimuth");
    }
    if (ui->elevation->value() != m_settings->m_elevation)
    {
        m_settings->m_elevation = ui->elevation->value();
        m_settingsKeys.append("elevation");
    }
    if (ui->rotator->currentText() != m_settings->m_rotator)
    {
        m_settings->m_rotator = ui->rotator->currentText();
        m_settingsKeys.append("rotator");
    }
    QStringList ipBlacklist;
    for (int i = 0; i < ui->ipBlacklist->count(); i++)
    {
        QString ip = ui->ipBlacklist->item(i)->text().trimmed();
        if (!ip.isEmpty()) {
            ipBlacklist.append(ip);
        }
    }
    if (ipBlacklist != m_settings->m_ipBlacklist)
    {
        m_settings->m_ipBlacklist = ipBlacklist;
        m_settingsKeys.append("ipBlacklist");
    }
}

void RemoteTCPSinkSettingsDialog::on_browseCertificate_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select SSL Certificate"),
                                                "",
                                                tr("SSL certificate (*.cert *.pem)"));
    if (!fileName.isEmpty()) {
        ui->certificate->setText(fileName);
    }
}

void RemoteTCPSinkSettingsDialog::on_browseKey_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select SSL Key"),
                                                "",
                                                tr("SSL key (*.key *.pem)"));
    if (!fileName.isEmpty()) {
        ui->key->setText(fileName);
    }
}

void RemoteTCPSinkSettingsDialog::on_addIP_clicked()
{
    QListWidgetItem *item = new QListWidgetItem("1.1.1.1");
    item->setFlags(Qt::ItemIsEditable | item->flags());
    ui->ipBlacklist->addItem(item);
    item->setSelected(true);
}

void RemoteTCPSinkSettingsDialog::on_removeIP_clicked()
{
    qDeleteAll(ui->ipBlacklist->selectedItems());
}

void RemoteTCPSinkSettingsDialog::on_publicListing_toggled()
{
    displayValid();
    displayEnabled();
}

void RemoteTCPSinkSettingsDialog::on_publicAddress_textChanged()
{
    displayValid();
}

void RemoteTCPSinkSettingsDialog::on_compressor_currentIndexChanged(int index)
{
    if (index == 0)
    {
        // FLAC settings
        ui->compressionLevel->setMaximum(8);
        ui->blockSize->clear();
        ui->blockSize->addItem("4096");
        ui->blockSize->addItem("16384");
        ui->blockSize->setCurrentIndex(1);
    }
    else if (index == 1)
    {
        // zlib settings
        ui->compressionLevel->setMaximum(9);
        ui->blockSize->clear();
        ui->blockSize->addItem("4096");
        ui->blockSize->addItem("8192");
        ui->blockSize->addItem("16384");
        ui->blockSize->addItem("32768");
        ui->blockSize->setCurrentIndex(3);
    }
}

void RemoteTCPSinkSettingsDialog::on_iqOnly_toggled(bool checked)
{
    ui->compressionSettings->setEnabled(!checked);
}

void RemoteTCPSinkSettingsDialog::on_isotropic_toggled(bool checked)
{
    (void) checked;

    displayEnabled();
}

void RemoteTCPSinkSettingsDialog::on_rotator_currentIndexChanged(int index)
{
    (void) index;

    displayEnabled();
}

bool RemoteTCPSinkSettingsDialog::isValid()
{
    bool valid = true;

    if (ui->publicListing->isChecked() && ui->publicAddress->text().isEmpty()) {
        valid = false;
    }

    return valid;
}

void RemoteTCPSinkSettingsDialog::displayValid()
{
    bool valid = isValid();

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
}

void RemoteTCPSinkSettingsDialog::displayEnabled()
{
    bool enabled = ui->publicListing->isChecked();
    bool none = ui->rotator->currentText() == "None";
    bool isotropic = ui->isotropic->isChecked();

    ui->publicAddressLabel->setEnabled(enabled);
    ui->publicAddress->setEnabled(enabled);
    ui->publicPort->setEnabled(enabled);
    ui->frequencyLabel->setEnabled(enabled);
    ui->minFrequency->setEnabled(enabled);
    ui->maxFrequency->setEnabled(enabled);
    ui->frequencyUnits->setEnabled(enabled);
    ui->antennaLabel->setEnabled(enabled);
    ui->antenna->setEnabled(enabled);
    ui->locationLabel->setEnabled(enabled);
    ui->location->setEnabled(enabled);
    ui->isotropicLabel->setEnabled(enabled);
    ui->isotropic->setEnabled(enabled);
    ui->rotatorLabel->setEnabled(enabled && !isotropic);
    ui->rotator->setEnabled(enabled && !isotropic);
    ui->directionLabel->setEnabled(enabled && !isotropic && none);
    ui->azimuthLabel->setEnabled(enabled && !isotropic && none);
    ui->azimuth->setEnabled(enabled && !isotropic && none);
    ui->elevationLabel->setEnabled(enabled && !isotropic && none);
    ui->elevation->setEnabled(enabled && !isotropic && none);
}

void RemoteTCPSinkSettingsDialog::rotatorsChanged(const QStringList& renameFrom, const QStringList& renameTo)
{
    AvailableChannelOrFeatureList rotators = m_availableRotatorHandler.getAvailableChannelOrFeatureList();
    updateRotatorList(rotators, renameFrom, renameTo);
}

void RemoteTCPSinkSettingsDialog::updateRotatorList(const AvailableChannelOrFeatureList& rotators, const QStringList& renameFrom, const QStringList& renameTo)
{
    // Update rotator settting if it has been renamed
    if (renameFrom.contains(m_settings->m_rotator)) {
        m_settings->m_rotator = renameTo[renameFrom.indexOf(m_settings->m_rotator)];
    }

    // Update list of rotators
    ui->rotator->blockSignals(true);
    ui->rotator->clear();
    ui->rotator->addItem("None");

    for (const auto& rotator : rotators) {
        ui->rotator->addItem(rotator.getLongId());
    }

    // Rotator feature can be created after this plugin, so select it
    // if the chosen rotator appears
    int rotatorIndex = ui->rotator->findText(m_settings->m_rotator);

    if (rotatorIndex >= 0)
    {
        ui->rotator->setCurrentIndex(rotatorIndex);
    }
    else
    {
        ui->rotator->setCurrentIndex(0); // return to None
    }

    ui->rotator->blockSignals(false);

    displayEnabled();
}