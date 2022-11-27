///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_RIGCTLSERVERWORKER_H_
#define INCLUDE_FEATURE_RIGCTLSERVERWORKER_H_

#include <QObject>

#include "util/message.h"
#include "util/messagequeue.h"

#include "rigctlserversettings.h"

class WebAPIAdapterInterface;
class QTcpServer;
class QTcpSocket;

class RigCtlServerWorker : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureRigCtlServerWorker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RigCtlServerSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureRigCtlServerWorker* create(const RigCtlServerSettings& settings, const QList<QString>& settingsKeys, bool force)
        {
            return new MsgConfigureRigCtlServerWorker(settings, settingsKeys, force);
        }

    private:
        RigCtlServerSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureRigCtlServerWorker(const RigCtlServerSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    struct ModeDemod {
        const char *mode;
        const char *modem;
    };

    RigCtlServerWorker(WebAPIAdapterInterface *webAPIAdapterInterface);
    ~RigCtlServerWorker();
    void reset();
    bool startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToFeature(MessageQueue *messageQueue) { m_msgQueueToFeature = messageQueue; }

private:
    enum RigCtrlState
    {
        idle,
        set_freq, set_freq_no_offset, set_freq_center, set_freq_center_no_offset, set_freq_set_offset, set_freq_offset,
        get_freq_center, get_freq_offset,
        set_mode_mod, set_mode_settings, set_mode_reply,
        get_power,
        set_power_on, set_power_off
    };

    // Hamlib rigctrl error codes
    enum rig_errcode_e {
        RIG_OK = 0,           /*!< No error, operation completed successfully */
        RIG_EINVAL = -1,      /*!< invalid parameter */
        RIG_ECONF = -2,       /*!< invalid configuration (serial,..) */
        RIG_ENOMEM = -3,      /*!< memory shortage */
        RIG_ENIMPL = -4,      /*!< function not implemented, but will be */
        RIG_ETIMEOUT = -5,    /*!< communication timed out */
        RIG_EIO = -6,         /*!< IO error, including open failed */
        RIG_EINTERNAL = -7,   /*!< Internal Hamlib error, huh! */
        RIG_EPROTO = -8,      /*!< Protocol error */
        RIG_ERJCTED = -9,     /*!< Command rejected by the rig */
        RIG_ETRUNC = -10,     /*!< Command performed, but arg truncated */
        RIG_ENAVAIL = -11,    /*!< function not available */
        RIG_ENTARGET = -12,   /*!< VFO not targetable */
        RIG_BUSERROR = -13,   /*!< Error talking on the bus */
        RIG_BUSBUSY = -14,    /*!< Collision on the bus */
        RIG_EARG = -15,       /*!< NULL RIG handle or any invalid pointer parameter in get arg */
        RIG_EVFO = -16,       /*!< Invalid VFO */
        RIG_EDOM = -17        /*!< Argument out of domain of func */
    };

    RigCtrlState m_state;
    double m_targetFrequency;
    double m_targetOffset;
    QString m_targetModem;
    int m_targetBW;

    QTcpServer *m_tcpServer;
    QTcpSocket *m_clientConnection;

    WebAPIAdapterInterface *m_webAPIAdapterInterface;
	MessageQueue m_inputMessageQueue;  //!< Queue for asynchronous inbound communication
    MessageQueue *m_msgQueueToFeature; //!< Queue to report channel change to main feature object
    RigCtlServerSettings m_settings;
    bool m_running;
    QRecursiveMutex m_mutex;

    static const unsigned int m_CmdLength;
    static const unsigned int m_ResponseLength;
    static const ModeDemod m_modeMap[];

    bool handleMessage(const Message& cmd);
    void applySettings(const RigCtlServerSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void restartServer(bool enabled, uint32_t port);
    bool setFrequency(double frequency, rig_errcode_e& rigCtlRC);
    bool getFrequency(double& frequency, rig_errcode_e& rigCtlRC);
    bool changeModem(const char *newMode, const char *newModemId, int newModemBw, rig_errcode_e& rigCtlRC);
    bool getMode(const char **mode, double& frequency, rig_errcode_e& rigCtlRC);
    bool setPowerOn(rig_errcode_e& rigCtlRC);
    bool setPowerOff(rig_errcode_e& rigCtlRC);
    bool getPower(bool& power, rig_errcode_e& rigCtlRC);

private slots:
    void handleInputMessages();
    void acceptConnection();
    void getCommand();
};

#endif // INCLUDE_FEATURE_RIGCTLSERVERWORKER_H_
