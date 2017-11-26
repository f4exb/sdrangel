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

#ifndef SDRGUI_WEBAPI_WEBAPIADAPTERGUI_H_
#define SDRGUI_WEBAPI_WEBAPIADAPTERGUI_H_

#include <QtGlobal>

#include "webapi/webapiadapterinterface.h"

class MainWindow;

class WebAPIAdapterGUI: public WebAPIAdapterInterface
{
public:
    WebAPIAdapterGUI(MainWindow& mainWindow);
    virtual ~WebAPIAdapterGUI();

    virtual int instanceSummary(
            Swagger::SWGInstanceSummaryResponse& response,
            Swagger::SWGErrorResponse& error);

    virtual int instanceDevices(
            bool tx,
            Swagger::SWGInstanceDevicesResponse& response,
            Swagger::SWGErrorResponse& error);

    virtual int instanceChannels(
            bool tx,
            Swagger::SWGInstanceChannelsResponse& response,
            Swagger::SWGErrorResponse& error);

    virtual int instanceLoggingGet(
            Swagger::SWGLoggingInfo& response,
            Swagger::SWGErrorResponse& error);

    virtual int instanceLoggingPut(
            Swagger::SWGLoggingInfo& response,
            Swagger::SWGErrorResponse& error);

    virtual int instanceAudioGet(
            Swagger::SWGAudioDevices& response,
            Swagger::SWGErrorResponse& error);

    virtual int instanceAudioPatch(
            Swagger::SWGAudioDevicesSelect& response,
            Swagger::SWGErrorResponse& error);

    virtual int instanceLocationGet(
            Swagger::SWGLocationInformation& response,
            Swagger::SWGErrorResponse& error);

    virtual int instanceLocationPut(
            Swagger::SWGLocationInformation& response,
            Swagger::SWGErrorResponse& error);

    virtual int instanceDVSerialPatch(
            bool dvserial,
            Swagger::SWGDVSeralDevices& response,
            Swagger::SWGErrorResponse& error);

    virtual int instancePresetGet(
            Swagger::SWGPresets& response,
            Swagger::SWGErrorResponse& error);

    virtual int instancePresetPatch(
            Swagger::SWGPresetTransfer& query,
            Swagger::SWGPresetIdentifier& response,
            Swagger::SWGErrorResponse& error);

    virtual int instancePresetPut(
            Swagger::SWGPresetTransfer& query,
            Swagger::SWGPresetIdentifier& response,
            Swagger::SWGErrorResponse& error);

    virtual int instancePresetPost(
            Swagger::SWGPresetTransfer& query,
            Swagger::SWGPresetIdentifier& response,
            Swagger::SWGErrorResponse& error);

    virtual int instancePresetDelete(
            Swagger::SWGPresetIdentifier& response,
            Swagger::SWGErrorResponse& error);

    virtual int instanceDeviceSetsGet(
            Swagger::SWGDeviceSetList& response,
            Swagger::SWGErrorResponse& error);

    virtual int instanceDeviceSetsPost(
            bool tx,
            Swagger::SWGDeviceSet& response,
            Swagger::SWGErrorResponse& error);

    virtual int instanceDeviceSetsDelete(
            Swagger::SWGDeviceSetList& response,
            Swagger::SWGErrorResponse& error);

    virtual int devicesetGet(
            int deviceSetIndex,
            Swagger::SWGDeviceSet& response,
            Swagger::SWGErrorResponse& error);

private:
    MainWindow& m_mainWindow;

    void getDeviceSetList(Swagger::SWGDeviceSetList* deviceSetList);
    void getDeviceSet(Swagger::SWGDeviceSet *deviceSet, const DeviceUISet* deviceUISet, int deviceUISetIndex);
    static QtMsgType getMsgTypeFromString(const QString& msgTypeString);
    static void getMsgTypeString(const QtMsgType& msgType, QString& level);
};

#endif /* SDRGUI_WEBAPI_WEBAPIADAPTERGUI_H_ */
