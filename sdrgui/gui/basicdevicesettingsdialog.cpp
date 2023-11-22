///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                         //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////

#include <cmath>

#include "gui/pluginpresetsdialog.h"
#include "gui/dialogpositioner.h"
#include "device/deviceapi.h"
#include "device/devicegui.h"
#include "device/deviceuiset.h"
#include "maincore.h"

#include "basicdevicesettingsdialog.h"
#include "ui_basicdevicesettingsdialog.h"

BasicDeviceSettingsDialog::BasicDeviceSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BasicDeviceSettingsDialog),
    m_hasChanged(false)
{
    ui->setupUi(this);
    setUseReverseAPI(false);
    setReverseAPIAddress("127.0.0.1");
    setReverseAPIPort(8888);
    setReverseAPIDeviceIndex(0);
    setReplayBytesPerSecond(0);
    setReplayStep(5.0f);
}

BasicDeviceSettingsDialog::~BasicDeviceSettingsDialog()
{
    delete ui;
}

void BasicDeviceSettingsDialog::setReplayBytesPerSecond(int bytesPerSecond)
{
    bool enabled = bytesPerSecond > 0;
    ui->replayLengthLabel->setEnabled(enabled);
    ui->replayLength->setEnabled(enabled);
    ui->replayLengthUnits->setEnabled(enabled);
    ui->replayLengthSize->setEnabled(enabled);
    ui->replayStepLabel->setEnabled(enabled);
    ui->replayStep->setEnabled(enabled);
    ui->replayStepUnits->setEnabled(enabled);
    m_replayBytesPerSecond = bytesPerSecond;
}

void BasicDeviceSettingsDialog::setReplayLength(float replayLength)
{
    m_replayLength = replayLength;
    ui->replayLength->setValue(replayLength);
}

void BasicDeviceSettingsDialog::on_replayLength_valueChanged(double value)
{
    m_replayLength = (float)value;
    float size = m_replayLength * m_replayBytesPerSecond;
    if (size < 1e6) {
        ui->replayLengthSize->setText("(<1MB)");
    } else {
        ui->replayLengthSize->setText(QString("(%1MB)").arg(std::ceil(size/1e6)));
    }
}

void BasicDeviceSettingsDialog::setReplayStep(float replayStep)
{
    m_replayStep = replayStep;
    ui->replayStep->setValue(replayStep);
}

void BasicDeviceSettingsDialog::on_replayStep_valueChanged(double value)

{
    m_replayStep = value;
}

void BasicDeviceSettingsDialog::setUseReverseAPI(bool useReverseAPI)
{
    m_useReverseAPI = useReverseAPI;
    ui->reverseAPI->setChecked(m_useReverseAPI);
}

void BasicDeviceSettingsDialog::setReverseAPIAddress(const QString& address)
{
    m_reverseAPIAddress = address;
    ui->reverseAPIAddress->setText(m_reverseAPIAddress);
}

void BasicDeviceSettingsDialog::setReverseAPIPort(uint16_t port)
{
    if (port < 1024) {
        return;
    } else {
        m_reverseAPIPort = port;
    }

    ui->reverseAPIPort->setText(tr("%1").arg(m_reverseAPIPort));
}

void BasicDeviceSettingsDialog::setReverseAPIDeviceIndex(uint16_t deviceIndex)
{
    m_reverseAPIDeviceIndex = deviceIndex > 99 ? 99 : deviceIndex;
    ui->reverseAPIDeviceIndex->setText(tr("%1").arg(m_reverseAPIDeviceIndex));
}

void BasicDeviceSettingsDialog::on_reverseAPI_toggled(bool checked)
{
    m_useReverseAPI = checked;
}

void BasicDeviceSettingsDialog::on_reverseAPIAddress_editingFinished()
{
    m_reverseAPIAddress = ui->reverseAPIAddress->text();
}

void BasicDeviceSettingsDialog::on_reverseAPIPort_editingFinished()
{
    bool dataOk;
    int reverseAPIPort = ui->reverseAPIPort->text().toInt(&dataOk);

    if((!dataOk) || (reverseAPIPort < 1024) || (reverseAPIPort > 65535)) {
        return;
    } else {
        m_reverseAPIPort = reverseAPIPort;
    }
}

void BasicDeviceSettingsDialog::on_reverseAPIDeviceIndex_editingFinished()
{
    bool dataOk;
    int reverseAPIDeviceIndex = ui->reverseAPIDeviceIndex->text().toInt(&dataOk);

    if ((!dataOk) || (reverseAPIDeviceIndex < 0)) {
        return;
    } else {
        m_reverseAPIDeviceIndex = reverseAPIDeviceIndex;
    }
}

void BasicDeviceSettingsDialog::on_presets_clicked()
{
    DeviceGUI *deviceGUI = qobject_cast<DeviceGUI *>(parent());
    if (!deviceGUI)
    {
        qDebug() << "BasicDeviceSettingsDialog::on_presets_clicked: parent not a DeviceGUI";
        return;
    }
    DeviceAPI *device = MainCore::instance()->getDevice(deviceGUI->getIndex());
    const QString& id = device->getSamplingDeviceId();
    // To include spectrum settings, we need to serialize DeviceUISet rather than just the DeviceGUI
    DeviceUISet *deviceUISet = deviceGUI->getDeviceUISet();

    PluginPresetsDialog dialog(id);
    dialog.setPresets(MainCore::instance()->getMutableSettings().getPluginPresets());
    dialog.setSerializableInterface(deviceUISet);
    dialog.populateTree();
    new DialogPositioner(&dialog, true);
    dialog.exec();
    if (dialog.wasPresetLoaded()) {
        QDialog::reject(); // Settings may have changed, so GUI will be inconsistent. Just close it
    }
}

void BasicDeviceSettingsDialog::accept()
{
    m_hasChanged = true;
    QDialog::accept();
}
