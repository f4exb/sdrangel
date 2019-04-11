///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// API for Tx channels                                                           //
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

#ifndef SDRBASE_CHANNEL_CHANNELSOURCEAPI_H_
#define SDRBASE_CHANNEL_CHANNELSOURCEAPI_H_

#include <QString>
#include <stdint.h>

#include "export.h"

namespace SWGSDRangel
{
    class SWGChannelSettings;
    class SWGChannelReport;
}

class SDRBASE_API ChannelSourceAPI {
public:
    ChannelSourceAPI(const QString& name);
    virtual ~ChannelSourceAPI() {}
    virtual void destroy() = 0;

    virtual void getIdentifier(QString& id) = 0;
    virtual void getTitle(QString& title) = 0;
    virtual void setName(const QString& name) { m_name = name; }
    virtual QString getName() const { return m_name; }
    virtual qint64 getCenterFrequency() const = 0;

    virtual QByteArray serialize() const = 0;
    virtual bool deserialize(const QByteArray& data) = 0;

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage)
    {
        (void) response;
        errorMessage = "Not implemented"; return 501;
    }

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage)
    {
        (void) force;
        (void) channelSettingsKeys;
        (void) response;
        errorMessage = "Not implemented"; return 501;
    }

    virtual int webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage)
    {
        (void) response;
        errorMessage = "Not implemented"; return 501;
    }

    int getIndexInDeviceSet() const { return m_indexInDeviceSet; }
    void setIndexInDeviceSet(int indexInDeviceSet) { m_indexInDeviceSet = indexInDeviceSet; }
    int getDeviceSetIndex() const { return m_deviceSetIndex; }
    void setDeviceSetIndex(int deviceSetIndex) { m_deviceSetIndex = deviceSetIndex; }
    uint64_t getUID() const { return m_uid; }

private:
    /** Unique identifier in a device set used for sorting. Used when there is no GUI.
     * In GUI version it is supported by GUI object name accessed through PluginInstanceGUI.
     */
    QString m_name;

    int m_indexInDeviceSet;
    int m_deviceSetIndex;
    uint64_t m_uid;
};



#endif /* SDRBASE_CHANNEL_CHANNELSOURCEAPI_H_ */
