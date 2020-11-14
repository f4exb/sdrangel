///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef DEVICES_METIS_DEVICEMETISSCAN_H_
#define DEVICES_METIS_DEVICEMETISSCAN_H_

#include <QObject>
#include <QString>
#include <QUdpSocket>
#include <QList>
#include <QMap>

#include <string>
#include <vector>
#include <map>

#include "plugin/plugininterface.h"
#include "export.h"

class DEVICES_API DeviceMetisScan : public QObject
{
    Q_OBJECT
public:
    struct DeviceScan
    {
        QString m_serial;
        QHostAddress m_address;
        quint16 m_port;

        DeviceScan(
            const QString& serial,
            const QHostAddress& address,
            quint16 port
        ) :
            m_serial(serial),
            m_address(address),
            m_port(port)
        {}
    };

    void scan();
    int getNbDevices() const { return m_scans.size(); }
    const DeviceScan* getDeviceAt(int index) const;
    void getSerials(QList<QString>& serials) const;
    void enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices);

public slots:
    void readyRead();

private:
	QUdpSocket m_udpSocket;
    QList<DeviceScan> m_scans;
    QMap<QString, DeviceScan*> m_serialMap;

};

#endif /* DEVICES_METIS_DEVICEMETISSCAN_H_ */
