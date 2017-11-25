///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
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

#ifndef SDRBASE_WEBAPI_WEBAPIADAPTERINTERFACE_H_
#define SDRBASE_WEBAPI_WEBAPIADAPTERINTERFACE_H_

#include <QString>

namespace Swagger
{
    class SWGInstanceSummaryResponse;
    class SWGInstanceDevicesResponse;
    class SWGInstanceChannelsResponse;
    class SWGLoggingInfo;
    class SWGAudioDevices;
    class SWGAudioDevicesSelect;
    class SWGLocationInformation;
    class SWGErrorResponse;
}

class WebAPIAdapterInterface
{
public:
    virtual ~WebAPIAdapterInterface() {}

    /**
     * Handler of /sdrangel (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceSummary
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceSummary(
            Swagger::SWGInstanceSummaryResponse& response __attribute__((unused)),
            Swagger::SWGErrorResponse& error __attribute__((unused)))
    { return 501; }

    /**
     * Handler of /sdrangel/devices (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceDevices
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceDevices(
            bool tx __attribute__((unused)),
            Swagger::SWGInstanceDevicesResponse& response __attribute__((unused)),
            Swagger::SWGErrorResponse& error __attribute__((unused)))
    { return 501; }

    /**
     * Handler of /sdrangel/channels (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceChannels(
            bool tx __attribute__((unused)),
            Swagger::SWGInstanceChannelsResponse& response __attribute__((unused)),
            Swagger::SWGErrorResponse& error __attribute__((unused)))
    { return 501; }

    /**
     * Handler of /sdrangel/logging (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLoggingGet(
            Swagger::SWGLoggingInfo& response __attribute__((unused)),
            Swagger::SWGErrorResponse& error __attribute__((unused)))
    { return 501; }

    /**
     * Handler of /sdrangel/logging (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLoggingPut(
            Swagger::SWGLoggingInfo& response __attribute__((unused)),
            Swagger::SWGErrorResponse& error __attribute__((unused)))
    { return 501; }

    /**
     * Handler of /sdrangel/audio (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceAudioGet(
            Swagger::SWGAudioDevices& response __attribute__((unused)),
            Swagger::SWGErrorResponse& error __attribute__((unused)))
    { return 501; }

    /**
     * Handler of /sdrangel/audio (PATCH) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceAudioPatch(
            Swagger::SWGAudioDevicesSelect& response __attribute__((unused)),
            Swagger::SWGErrorResponse& error __attribute__((unused)))
    { return 501; }

    /**
     * Handler of /sdrangel/location (GET) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLocationGet(
            Swagger::SWGLocationInformation& response __attribute__((unused)),
            Swagger::SWGErrorResponse& error __attribute__((unused)))
    { return 501; }

    /**
     * Handler of /sdrangel/location (PUT) swagger/sdrangel/code/html2/index.html#api-Default-instanceChannels
     * returns the Http status code (default 501: not implemented)
     */
    virtual int instanceLocationPut(
            Swagger::SWGLocationInformation& response __attribute__((unused)),
            Swagger::SWGErrorResponse& error __attribute__((unused)))
    { return 501; }

    static QString instanceSummaryURL;
    static QString instanceDevicesURL;
    static QString instanceChannelsURL;
    static QString instanceLoggingURL;
    static QString instanceAudioURL;
    static QString instanceLocationURL;
};



#endif /* SDRBASE_WEBAPI_WEBAPIADAPTERINTERFACE_H_ */
