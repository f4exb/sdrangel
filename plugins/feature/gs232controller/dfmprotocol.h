///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_DFMPROTOCOL_H_
#define INCLUDE_FEATURE_DFMPROTOCOL_H_

#include <QTimer>

#include "util/message.h"
#include "controllerprotocol.h"

class DFMProtocol : public QObject, public ControllerProtocol
{
    Q_OBJECT
public:

    struct DFMStatus {
        // STATL
        bool m_initialized;
        bool m_brakesOn;
        bool m_trackOn;
        bool m_slewEnabled;
        bool m_lubePumpsOn;
        bool m_approachingSWLimit;
        bool m_finalSWLimit;
        bool m_slewing;
        // STATH
        bool m_setting;
        bool m_haltMotorsIn;
        bool m_excomSwitchOn;
        bool m_servoPackAlarm;
        bool m_targetOutOfRange;
        bool m_cosdecOn;
        bool m_rateCorrOn;
        bool m_drivesOn;
        // STATLH
        bool m_pumpsReady;
        bool m_minorPlus;
        bool m_minorMinus;
        bool m_majorPlus;
        bool m_majorMinus;
        bool m_nextObjectActive;
        bool m_auxTrackRate;
        // Other status information
        float m_currentHA;
        float m_currentRA;
        float m_currentDec;
        float m_currentX;
        float m_currentY;
        enum Controller {NONE, OCU, LCU, MCU} m_controller;
        float m_torqueX;
        float m_torqueY;
        float m_rateX;
        float m_rateY;
        float m_siderealRateX;
        float m_siderealRateY;
        float m_siderealTime;
        float m_universalTime;
    };

    // Message from DFMProtocol to the GUI, with status information to display
    class MsgReportDFMStatus : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        DFMStatus getDFMStatus() const { return m_dfmStatus; }

        static MsgReportDFMStatus* create(const DFMStatus& dfmStatus)
        {
            return new MsgReportDFMStatus(dfmStatus);
        }
    private:
        DFMStatus m_dfmStatus;

        MsgReportDFMStatus(const DFMStatus& dfmStatus) :
            Message(),
            m_dfmStatus(dfmStatus)
        {
        }
    };

    DFMProtocol();
    ~DFMProtocol();
    void setAzimuthElevation(float azimuth, float elevation) override;
    void readData() override;
    void update() override;
    void applySettings(const GS232ControllerSettings& settings, const QList<QString>& settingsKeys, bool force) override;

private:
    void parseLCUResponse(const QString& packet);
    void sendCommand();

    QTimer m_timer;

    QString m_rxBuffer;
    int m_packetCnt;

    float m_targetRA;
    float m_targetDec;

private slots:
    void periodicTask();
};

#endif // INCLUDE_FEATURE_DFMPROTOCOL_H_

