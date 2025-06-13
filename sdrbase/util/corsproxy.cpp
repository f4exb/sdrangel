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

#include "corsproxy.h"

#include <QDebug>

#ifdef __EMSCRIPTEN__
#include <emscripten/val.h>
#endif

QString CORSProxy::adjustHost(const QString& url)
{
#ifdef __EMSCRIPTEN__
    return CORSProxy::adjustHost(QUrl(url)).toString();
#else
    return url;
#endif
}

// Some servers don't support CORS, so we assume proxy running on same server SDRangel is on
// Look at using https://corsproxy.io/ as alternative - although text only
// E.g: https://corsproxy.io/?https://db.satnogs.org/api/satellites/
QUrl CORSProxy::adjustHost(const QUrl& url)
{
#ifdef __EMSCRIPTEN__
    QUrl requestURL(url);

    emscripten::val location = emscripten::val::global("location");
    QString proxyHost = QString::fromStdString(location["hostname"].as<std::string>());

    // sdrangel.org doesn't currently support proxying
    if (proxyHost == "www.sdrangel.org") {
        proxyHost = "sdrangel.beniston.com";
    }

    QString host = requestURL.host();
    static const QStringList hosts = {
        "db.satnogs.org",
        "sdo.gsfc.nasa.gov",
        "www.amsat.org",
        "datacenter.stix.i4ds.net",
        "user-web.icecube.wisc.edu",
        "www.sws.bom.gov.au",
        "www.spaceweather.gc.ca",
        "airspy.com",
        "kiwisdr.com",
        "opensky-network.org",
        "storage.googleapis.com"
    };
    if (hosts.contains(host))
    {
        requestURL.setHost(proxyHost);
        QString newPath = "/" + host + requestURL.path();
        requestURL.setPath(newPath);
    }

    return requestURL;

#else
    return url;
#endif
}
