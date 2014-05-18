///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef INCLUDE_AUDIODEVICEINFO_H
#define INCLUDE_AUDIODEVICEINFO_H

#include <QStringList>
#include "util/export.h"

class SDRANGELOVE_API AudioDeviceInfo {
public:
	struct Device {
		QString name;
		QString api;
		int id;

		Device(const QString& _name, const QString& _api, int _id) :
			name(_name),
			api(_api),
			id(_id)
		{ }
	};
	typedef QList<Device> Devices;

	AudioDeviceInfo();

	int match(const QString& api, const QString device) const;

	const Devices& getDevices() const { return m_devices; }

private:
	Devices m_devices;
};

#endif // INCLUDE_AUDIODEVICEINFO_H
