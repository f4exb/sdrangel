///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#ifndef INCLUDE_FREEDVDEMOD_WEBAPIADAPTER_H
#define INCLUDE_FREEDVDEMOD_WEBAPIADAPTER_H

#include "channel/channelapi.h"
#include "freedvdemodsettings.h"

/**
 * Standalone API adapter only for the settings
 */
class FreeDVDemodWebAPIAdapter : public ChannelAPI {
public:
    FreeDVDemodWebAPIAdapter();
    virtual ~FreeDVDemodWebAPIAdapter();

    // unused pure virtual methods
    virtual void destroy() {}
    virtual void getIdentifier(QString& id) {}
    virtual void getTitle(QString& title) {}
    virtual qint64 getCenterFrequency() const { return 0; }
    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return 0;
    }

    // virtual methods actually implemented
    virtual QByteArray serialize() const { return m_settings.serialize(); }
    virtual bool deserialize(const QByteArray& data) { m_settings.deserialize(data); }

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

private:
    FreeDVDemodSettings m_settings;
};

#endif // INCLUDE_FREEDVDEMOD_WEBAPIADAPTER_H
