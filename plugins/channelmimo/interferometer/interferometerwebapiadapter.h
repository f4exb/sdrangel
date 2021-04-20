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

#ifndef INCLUDE_INTERFEROMETER_WEBAPIADAPTER_H
#define INCLUDE_INTERFEROMETER_WEBAPIADAPTER_H

#include "channel/channelwebapiadapter.h"
#include "dsp/glscopesettings.h"
#include "dsp/spectrumsettings.h"
#include "interferometersettings.h"

/**
 * Standalone API adapter only for the settings
 */
class InterferometerWebAPIAdapter : public ChannelWebAPIAdapter {
public:
    InterferometerWebAPIAdapter();
    virtual ~InterferometerWebAPIAdapter();

    virtual QByteArray serialize() const { return m_settings.serialize(); }
    virtual bool deserialize(const QByteArray& data) { return m_settings.deserialize(data); }

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
            SWGSDRangel::SWGChannelSettings& response,
            const InterferometerSettings& settings,
            const GLScopeSettings& scopeSettings,
            const SpectrumSettings& spectrumSettings);

    static void webapiUpdateChannelSettings(
            InterferometerSettings& settings,
            GLScopeSettings& scopeSettings,
            SpectrumSettings& spectrumSettings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

private:
    InterferometerSettings m_settings;
    GLScopeSettings m_glScopeSettings;
    SpectrumSettings m_SpectrumSettings;

    static int qColorToInt(const QColor& color);
    static QColor intToQColor(int intColor);
};

#endif // INCLUDE_INTERFEROMETER_WEBAPIADAPTER_H
