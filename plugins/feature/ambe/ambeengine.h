///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_AMBE_AMBEENGINE_H_
#define SDRBASE_AMBE_AMBEENGINE_H_

#include <vector>
#include <string>

#include <QObject>
#include <QMutex>
#include <QString>
#include <QList>
#include <QByteArray>

class QThread;
class AMBEWorker;
class AudioFifo;

class AMBEEngine : public QObject
{
    Q_OBJECT
public:
    struct DeviceRef
    {
        QString m_devicePath;    //!< device path or url
        uint32_t m_successCount; //!< number of frames successfully decoded
        uint32_t m_failureCount; //!< number of frames failing decoding
    };

    AMBEEngine();
    ~AMBEEngine();

    void scan(QList<QString>& ambeDevices);
    void releaseAll();

    int getNbDevices() const { return m_controllers.size(); }   //!< number of devices used
    void getDeviceRefs(QList<DeviceRef>& devicesRefs);          //!< reference of the devices used
    bool registerController(const std::string& deviceRef);      //!< create a new controller for the device in reference
    void releaseController(const std::string& deviceRef);       //!< release controller resources for the device in reference

    void pushMbeFrame(
            const unsigned char *mbeFrame,
            int mbeRateIndex,
            int mbeVolumeIndex,
            unsigned char channels,
            bool useHP,
            int upsampling,
            AudioFifo *audioFifo);

private:
    struct AMBEController
    {
        AMBEController() :
            thread(nullptr),
            worker(nullptr)
        {}

        QThread *thread;
        AMBEWorker *worker;
        std::string device;

        uint32_t getSuccessCount() const;
        uint32_t getFailureCount() const;
    };

#ifndef _WIN32
    static std::string get_driver(const std::string& tty);
    static void register_comport(std::vector<std::string>& comList, std::vector<std::string>& comList8250, const std::string& dir);
    static void probe_serial8250_comports(std::vector<std::string>& comList, std::vector<std::string> comList8250);
#endif
    void getComList();

    std::vector<AMBEController> m_controllers;
    std::vector<std::string> m_comList;
    std::vector<std::string> m_comList8250;
    QMutex m_mutex;
};



#endif /* SDRBASE_AMBE_AMBEENGINE_H_ */
