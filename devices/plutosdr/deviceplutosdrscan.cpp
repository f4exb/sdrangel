///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <iostream>
#include <cstdio>
#include <cstring>
#include <regex>
#include <memory>
#include <iio.h>

#include <QtGlobal>

#include "deviceplutosdrbox.h"
#include "deviceplutosdrscan.h"

void DevicePlutoSDRScan::scan()
{
    int i, num_contexts;
    struct iio_scan_context *scan_ctx;
    struct iio_context_info **info;

    scan_ctx = iio_create_scan_context(0, 0);

    if (!scan_ctx)
    {
        qCritical("PlutoSDRScan::scan: could not create scan context");
        return;
    }

    num_contexts = iio_scan_context_get_info_list(scan_ctx, &info);

    if (num_contexts < 0)
    {
        qCritical("PlutoSDRScan::scan: could not get contexts");
        return;
    }

    m_scans.clear();
    m_scans.reserve(num_contexts);

    for (i = 0; i < num_contexts; i++)
    {
        const char *description = iio_context_info_get_description(info[i]);
        const char *uri = iio_context_info_get_uri(info[i]);

        if (!DevicePlutoSDRBox::probeURI(std::string(uri))) { // continue if not accessible
            continue;
        }

        qDebug("PlutoSDRScan::scan: %d: %s [%s]", i, description, uri);
        char *pch = strstr(const_cast<char*>(description), "PlutoSDR");

        if (pch)
        {
            // As device scan is used across multiple vectors it's best to use a
            // managed pointer, as to keep track of when it's safe to delete.
            std::shared_ptr<DeviceScan> dev_scan = std::make_shared<DeviceScan>(
                DeviceScan({
                    std::string(description),
                    std::string("TBD"),
                    std::string(uri)
                }));
            m_scans.push_back(dev_scan);
            m_urilMap[m_scans.back()->m_uri] = m_scans.back();

            std::regex desc_regex(".*serial=(.+)");
            std::smatch desc_match;
            std::regex_search(m_scans.back()->m_name, desc_match, desc_regex);

            if (desc_match.size() == 2)
            {
                m_scans.back()->m_serial = desc_match[1];
                m_serialMap[m_scans.back()->m_serial] = m_scans.back();
            }
        }
    }

    iio_context_info_list_free(info);
    iio_scan_context_destroy(scan_ctx);
}

const std::string* DevicePlutoSDRScan::getURIAt(unsigned int index) const
{
    if (index < m_scans.size()) {
        return &(m_scans[index]->m_uri);
    } else {
        return 0;
    }
}

const std::string* DevicePlutoSDRScan::getSerialAt(unsigned int index) const
{
    if (index < m_scans.size()) {
        return &(m_scans[index]->m_serial);
    } else {
        return 0;
    }
}

const std::string* DevicePlutoSDRScan::getURIFromSerial(
        const std::string& serial) const
{
    std::map<std::string, std::shared_ptr<DeviceScan>>::const_iterator it = m_serialMap.find(serial);
    if (it == m_serialMap.end()) {
        return 0;
    } else {
        return &((it->second)->m_uri);
    }
}

void DevicePlutoSDRScan::getSerials(std::vector<std::string>& serials) const
{
    std::vector<std::shared_ptr<DeviceScan>>::const_iterator it = m_scans.begin();
    serials.clear();

    for (; it != m_scans.end(); ++it) {
        serials.push_back((*it)->m_serial);
    }
}

void DevicePlutoSDRScan::enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices)
{
    scan();
    std::vector<std::string> serials;
    getSerials(serials);

    std::vector<std::string>::const_iterator it = serials.begin();
    int i;

	for (i = 0; it != serials.end(); ++it, ++i)
	{
	    QString serial_str = QString::fromLocal8Bit(it->c_str());
	    QString displayableName(QString("PlutoSDR[%1] %2").arg(i).arg(serial_str));

        originDevices.append(PluginInterface::OriginDevice(
            displayableName,
            hardwareId,
            serial_str,
            i, // sequence
            1, // Nb Rx
            1  // Nb Tx
        ));

        qDebug("DevicePlutoSDRScan::enumOriginDevices: enumerated PlutoSDR device #%d", i);
	}

}