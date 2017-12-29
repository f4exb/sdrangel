///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// API for Rx channels                                                           //
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

#ifndef SDRBASE_CHANNEL_CHANNELSINKAPI_H_
#define SDRBASE_CHANNEL_CHANNELSINKAPI_H_

#include <QString>
#include <QByteArray>
#include <stdint.h>

#include "util/export.h"

namespace SWGSDRangel
{
    class SWGChannelSettings;
}

class SDRANGEL_API ChannelSinkAPI {
public:
    ChannelSinkAPI(const QString& name);
    virtual ~ChannelSinkAPI() {}
    virtual void destroy() = 0;

    virtual void getIdentifier(QString& id) = 0;
    virtual void getTitle(QString& title) = 0;
    virtual void setName(const QString& name) { m_name = name; }
    virtual const QString& getName() const { return m_name; }
    virtual qint64 getCenterFrequency() const = 0;

    virtual QByteArray serialize() const = 0;
    virtual bool deserialize(const QByteArray& data) = 0;

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response __attribute__((unused)),
            QString& errorMessage)
    { errorMessage = "Not implemented"; return 501; }

    virtual int webapiSettingsPutPatch(
            bool force __attribute__((unused)),
            const QStringList& channelSettingsKeys __attribute__((unused)),
            SWGSDRangel::SWGChannelSettings& response __attribute__((unused)),
            QString& errorMessage)
    { errorMessage = "Not implemented"; return 501; }

    int getIndexInDeviceSet() const { return m_indexInDeviceSet; }
    void setIndexInDeviceSet(int indexInDeviceSet) { m_indexInDeviceSet = indexInDeviceSet; }
    uint64_t getUID() const { return m_uid; }

private:
    /** Unique identifier in a device set used for sorting. Used when there is no GUI.
     * In GUI version it is supported by GUI object name accessed through PluginInstanceGUI.
     */
    QString m_name;

    int m_indexInDeviceSet;
    uint64_t m_uid;
};



#endif /* SDRBASE_CHANNEL_CHANNELSINKAPI_H_ */
