///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE                                        //
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

#include <QTableWidgetItem>
#include <QDebug>

#include "util/vlftransmitters.h"
#include "channel/channelwebapiutils.h"
#include "device/deviceapi.h"
#include "device/deviceset.h"
#include "dsp/dspdevicesourceengine.h"
#include "maincore.h"

#include "sidaddchannelsdialog.h"

SIDAddChannelsDialog::SIDAddChannelsDialog(SIDSettings *settings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::SIDAddChannelsDialog),
    m_settings(settings)
{
    ui->setupUi(this);

    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();

    ui->channels->setColumnCount(deviceSets.size() + 2);

    // Create column header
    ui->channels->setHorizontalHeaderItem(COL_TX_NAME, new QTableWidgetItem("Callsign"));
    ui->channels->setHorizontalHeaderItem(COL_TX_FREQUENCY, new QTableWidgetItem("Frequency (Hz)"));
    for (unsigned int i = 0; i < deviceSets.size(); i++)
    {
        if (deviceSets[i]->m_deviceSourceEngine || deviceSets[i]->m_deviceMIMOEngine)
        {
            QTableWidgetItem *item = new QTableWidgetItem(mainCore->getDeviceSetId(deviceSets[i]));

            ui->channels->setHorizontalHeaderItem(COL_DEVICE + i, item);
        }
    }

    // Add row for each transmitter, with checkbox for each device
    ui->channels->setSortingEnabled(false);
    for (int j = 0; j < VLFTransmitters::m_transmitters.size(); j++)
    {
        int row = ui->channels->rowCount();
        ui->channels->setRowCount(row+1);

        ui->channels->setItem(row, COL_TX_NAME, new QTableWidgetItem(VLFTransmitters::m_transmitters[j].m_callsign));
        ui->channels->setItem(row, COL_TX_FREQUENCY, new QTableWidgetItem(QString::number(VLFTransmitters::m_transmitters[j].m_frequency)));
        ui->channels->item(row, COL_TX_FREQUENCY)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        for (unsigned int i = 0; i < deviceSets.size(); i++)
        {
            if (deviceSets[i]->m_deviceSourceEngine || deviceSets[i]->m_deviceMIMOEngine)
            {
                QTableWidgetItem *enableItem = new QTableWidgetItem();
                enableItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
                enableItem->setCheckState(Qt::Unchecked);
                ui->channels->setItem(row, COL_DEVICE + i, enableItem);
            }
        }
    }
    ui->channels->sortItems(COL_TX_FREQUENCY);
    ui->channels->setSortingEnabled(true);
    ui->channels->resizeColumnsToContents();
}

SIDAddChannelsDialog::~SIDAddChannelsDialog()
{
    delete ui;
}

void SIDAddChannelsDialog::accept()
{
    if (ui->channels->columnCount() > 2)
    {
        MainCore *mainCore = MainCore::instance();
        connect(mainCore, &MainCore::channelAdded, this, &SIDAddChannelsDialog::channelAdded);

        m_count = m_settings->m_channelSettings.size();
        m_row = 0;
        m_col = COL_DEVICE;
        addNextChannel();
    }
    else
    {
        QDialog::accept();
    }
}

void SIDAddChannelsDialog::addNextChannel()
{
    if (m_row < ui->channels->rowCount())
    {
        QString id = ui->channels->horizontalHeaderItem(m_col)->text();
        unsigned int deviceSetIndex;

        if (ui->channels->item(m_row, m_col)->checkState() == Qt::Checked)
        {
            MainCore::getDeviceSetIndexFromId(id, deviceSetIndex);
            ChannelWebAPIUtils::addChannel(deviceSetIndex,  "sdrangel.channel.channelpower", 0);
        }
        else
        {
            nextChannel(); // can recursively call this method
        }
    }
    else
    {
        QDialog::accept();
    }
}

void SIDAddChannelsDialog::nextChannel()
{
    m_col++;
    if (m_col >= ui->channels->columnCount())
    {
        m_col = COL_DEVICE;
        m_row++;
    }

    addNextChannel();
}

void SIDAddChannelsDialog::channelAdded(int deviceSetIndex, ChannelAPI *channel)
{
    (void) deviceSetIndex;

    const VLFTransmitters::Transmitter *transmitter = VLFTransmitters::m_callsignHash.value(ui->channels->item(m_row, COL_TX_NAME)->text());

    ChannelWebAPIUtils::patchChannelSetting(channel, "title", transmitter->m_callsign);
    ChannelWebAPIUtils::patchChannelSetting(channel, "frequency", transmitter->m_frequency);
    ChannelWebAPIUtils::patchChannelSetting(channel, "frequencyMode", 1);
    ChannelWebAPIUtils::patchChannelSetting(channel, "rfBandwidth", 300);
    ChannelWebAPIUtils::patchChannelSetting(channel, "averagePeriodUS", 10000000);

    // Update settings if they are created by SIDGUI before this slot is called
    if (m_count < m_settings->m_channelSettings.size()) {
        m_settings->m_channelSettings[m_count].m_label = transmitter->m_callsign;
    }

    m_count++;
    nextChannel();
}
