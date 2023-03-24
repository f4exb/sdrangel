///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#ifndef _AARONIARTSA_AARONIARTSASETTINGS_H_
#define _AARONIARTSA_AARONIARTSASETTINGS_H_

#include <QString>
#include <QByteArray>

struct AaroniaRTSAInputSettings {

    enum ConnectionStatus
    {
        ConnectionIdle,        // 0 - gray
        ConnectionUnstable,    // 1 - yellow
        ConnectionOK,          // 2 - green
        ConnectionError,       // 3 - red
        ConnectionDisconnected // 4 - magenta
    };

    quint64 m_centerFrequency;
    int m_sampleRate;
	QString m_serverAddress;

	bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

	AaroniaRTSAInputSettings();
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const AaroniaRTSAInputSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* _AARONIARTSA_AARONIARTSASETTINGS_H_ */
