///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_DSP_DVSERIALENGINE_H_
#define SDRBASE_DSP_DVSERIALENGINE_H_

#include <QObject>
#include <QMutex>
#include <vector>
#include <string>
#include <list>

class QThread;
class DVSerialWorker;
class AudioFifo;

class DVSerialEngine : public QObject
{
    Q_OBJECT
public:
    DVSerialEngine();
    ~DVSerialEngine();

    bool scan();
    void release();

    int getNbDevices() const { return m_controllers.size(); }
    void getDevicesNames(std::vector<std::string>& devicesNames);

    void pushMbeFrame(
            const unsigned char *mbeFrame,
            int mbeRateIndex,
            int mbeVolumeIndex,
            unsigned char channels,
            bool useHP,
            AudioFifo *audioFifo);

private:
    struct DVSerialController
    {
        QThread *thread;
        DVSerialWorker *worker;
        std::string device;
    };

#ifndef __WINDOWS__
    static std::string get_driver(const std::string& tty);
    static void register_comport(std::list<std::string>& comList, std::list<std::string>& comList8250, const std::string& dir);
    static void probe_serial8250_comports(std::list<std::string>& comList, std::list<std::string> comList8250);
#endif
    void getComList();

    std::list<std::string> m_comList;
    std::list<std::string> m_comList8250;
    std::vector<DVSerialController> m_controllers;
    QMutex m_mutex;
};



#endif /* SDRBASE_DSP_DVSERIALENGINE_H_ */
