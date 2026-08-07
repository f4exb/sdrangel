///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017, 2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>         //
// Copyright (C) 2019 Sebastian Weiss <dl3yc@darc.de>                            //
// Copyright (C) 2021 Andreas Baulig <free.geronimo@hotmail.de>                  //
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
#include <string_view>
#include <iio.h>
#include <utility>

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
    qDebug("PlutoSDRScan::scan: found %d contexts", num_contexts);

    for (i = 0; i < num_contexts; i++)
    {
        const char *description = iio_context_info_get_description(info[i]);
        const char *uri = iio_context_info_get_uri(info[i]);

        if (!DevicePlutoSDRBox::probeURI(std::string(uri)) || !description) {
            continue;
        }

        // For uri which are ip:*.local replace with the ip address
        std::string fixedUri = replaceHostnameWithIP(uri, description);

        qDebug("PlutoSDRScan::scan: %d: %s [%s] [%s]", i, description, uri, fixedUri.c_str());
        const std::string_view descriptionView = std::string_view{description};
        const bool isPlutoDescription =
            (descriptionView.find("PlutoSDR") != std::string_view::npos) ||
            (descriptionView.find("AD93") != std::string_view::npos);

        if (isPlutoDescription)
        {
            // As device scan is used across multiple vectors it's best to use a
            // managed pointer, as to keep track of when it's safe to delete.
            std::shared_ptr<DeviceScan> dev_scan = std::make_shared<DeviceScan>(
                DeviceScan({
                    std::string(description),
                    std::string("TBD"),
                    std::move(fixedUri)
                }));
            m_scans.push_back(std::move(dev_scan));
            m_urilMap[m_scans.back()->m_uri] = m_scans.back();

            std::regex desc_regex(".*serial=(.+)");
            std::smatch desc_match;
            std::regex_search(m_scans.back()->m_name, desc_match, desc_regex);

            if (desc_match.size() == 2)
            {
                // A device can be discovered through multiple backends (e.g. USB and IP).
                // Keep the backend in the serial key so each scanned entry remains unique.
                m_scans.back()->m_serial = desc_match[1].str() + createBackendSuffix(uri);
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

    // Multiple backends can refer to the same physical device.
    // Keep a separate list of physical serials for stable display numbering.
    // Example: Pluto[0] (ip) and Pluto[0] (usb) refer to the same hardware.
    std::map<std::string, int> physicalSerialIndexes;
    std::vector<std::string>::const_iterator it = serials.begin();
    int i;

    for (i = 0; it != serials.end(); ++it, ++i)
    {
        // Keep the backend-qualified serial as the device identifier.
        QString serial_str = QString::fromLocal8Bit(it->c_str());
        // Remove backend suffix for display and grouping.
        std::string physicalSerial = getPhysicalSerial(*it);

        // Find the existing physical device index so USB/IP instances share a label.
        auto pos = physicalSerialIndexes.find(physicalSerial);

        // Display index is based on physical devices, not backend instances.
        int physical_idx;

        if (pos == physicalSerialIndexes.end())
        {
            physical_idx = physicalSerialIndexes.size();
            physicalSerialIndexes[physicalSerial] = physical_idx;
        }
        else
        {
            physical_idx = pos->second;
        }

        // Show backend information to distinguish multiple connections to the same device.
        QString displayableName(
           QString("PlutoSDR%1%2 %3")
                .arg(physical_idx)
                .arg(getBackendLabel(*it))
                .arg(QString::fromLocal8Bit(physicalSerial.c_str())));

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

/**
 * @brief Replaces an mDNS hostname with the discovered endpoint.
 *
 * Older libiio versions may return the advertised mDNS hostname (for example
 * "ip:pluto.local") rather than the endpoint that was actually discovered.
 * This becomes ambiguous when identical mDNS hostnames exist on different
 * network segments, since multiple devices can legitimately advertise the
 * same ".local" hostname.
 *
 * Since the scan description begins with the discovered endpoint (IPv4 or IPv6),
 * followed by " (". For ".local" URIs, substitute the endpoint while
 * preserving the backend and optional port number.
 *
 * Examples:
 *   URI:         ip:pluto.local
 *   Description: 192.168.2.1 (...)
 *   Result:      ip:192.168.2.1
 *
 *   URI:         ip:pluto.local:30431
 *   Description: fe80::205:f7ff:fe75:9c4f%eth1 (...)
 *   Result:      ip:[fe80::205:f7ff:fe75:9c4f%eth1]:30431
 */
std::string DevicePlutoSDRScan::replaceHostnameWithIP(
    const std::string& uri,
    const char *description) const
{
    if (!description) {
        return uri;
    }

    // Only applies to IP backends.
    if (uri.rfind("ip:", 0) != 0) {
        return uri;
    }

    // Only rewrite mDNS hostnames.
    if (uri.find(".local") == std::string::npos) {
        return uri;
    }

    // Description starts with "<endpoint> (".
    const std::string desc(description);
    const size_t endpointEnd = desc.find(" (");

    if (endpointEnd == std::string::npos) {
        return uri;
    }

    const std::string endpoint = desc.substr(0, endpointEnd);

    // Preserve any optional port.
    std::string port;
    const size_t localPos = uri.find(".local");

    if (localPos != std::string::npos)
    {
        const size_t afterLocal = localPos + 6; // strlen(".local")

        if (afterLocal < uri.size() && uri[afterLocal] == ':') {
            port = uri.substr(afterLocal);
        }
    }

    std::string newUri = "ip:";

    // RFC2732 requires brackets around IPv6 literals when a port is present.
    if (!port.empty() && endpoint.find(':') != std::string::npos) {
        newUri += "[" + endpoint + "]";
    } else {
        newUri += endpoint;
    }

    newUri += port;

    return newUri;
}

/**
 * @brief Creates a backend prefix from a libiio URI.
 *
 * The backend is converted into a serial suffix so that devices
 * discovered through different backends remain unique.
 *
 * Examples:
 *   ip:192.168.2.1       -> "_ip"
 *   usb:3.32.5           -> "_usb"
 *   serial:/dev/ttyUSB0  -> "_serial"
 *   local:               -> "_local"
 *
 * @param uri Device URI returned by libiio.
 *
 * @return iio backend suffix including the separator, or an empty string
 *         if no backend prefix is present.
 */
std::string DevicePlutoSDRScan::createBackendSuffix(const char *uri) const
{
    if (!uri) {
        return {};
    }

    const char *sep = std::strchr(uri, ':');

    if (!sep) {
        return {};
    }

    return std::string(1, BACKEND_SEPARATOR)  + std::string(uri, sep - uri);
}

/**
 * @brief Creates a display string for the backend portion of a serial.
 *
 * The scan process appends the backend to make serial numbers unique.
 * This function recovers the backend suffix into a user-facing label for
 * display purposes.
 *
 * Examples:
 *   ABC123_ip  -> " (ip)"
 *   ABC123_usb -> " (usb)"
 *
 * @param serial backend-qualified serial number.
 *
 * @return Display label suffix.
 */
QString DevicePlutoSDRScan::getBackendLabel(const std::string& serial) const
{
    size_t sep = serial.rfind(BACKEND_SEPARATOR);

    if (sep == std::string::npos)
    {
        return {};
    }

    return QString(" (%1)").arg(
        QString::fromStdString(serial.substr(sep + 1)));
}

/**
 * @brief Removes the backend suffix from a serial number.
 *
 * The scan process appends the backend to make serial numbers unique.
 * This function recovers the physical device serial for grouping and
 * display purposes.
 *
 * Examples:
 *   ABC123_ip  -> ABC123
 *   ABC123_usb -> ABC123
 *
 * @param serial backend-qualified serial number.
 *
 * @return Physical device serial number.
 */
std::string DevicePlutoSDRScan::getPhysicalSerial(const std::string& serial) const
{
    size_t sep = serial.rfind(BACKEND_SEPARATOR);

    if (sep == std::string::npos)
    {
        return serial;
    }

    return serial.substr(0, sep);
}
