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

#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <libgen.h>

#ifndef __WINDOWS__
#include <termios.h>
#include <sys/ioctl.h>
#ifndef __APPLE__
#include <linux/serial.h>
#endif
#endif

#include <QDebug>
#include <QThread>
#include <QMutexLocker>

#include "audio/audiooutput.h"
#include "dvserialengine.h"
#include "dvserialworker.h"

DVSerialEngine::DVSerialEngine()
{

}

DVSerialEngine::~DVSerialEngine()
{
    release();
}

#ifdef __WINDOWS__
void DVSerialEngine::getComList()
{
    m_comList.clear();
    m_comList8250.clear();
    char comCStr[16];

    // Arbitrarily set the list to the 20 first COM ports
    for (int i = 1; i <= 20; i++)
    {
        sprintf(comCStr, "COM%d", i);
        m_comList.push_back(std::string(comCStr));
    }
}
#else
std::string DVSerialEngine::get_driver(const std::string& tty)
{
    struct stat st;
    std::string devicedir = tty;

    // Append '/device' to the tty-path
    devicedir += "/device";

    // Stat the devicedir and handle it if it is a symlink
    if (lstat(devicedir.c_str(), &st) == 0 && S_ISLNK(st.st_mode))
    {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));

        // Append '/driver' and return basename of the target
        devicedir += "/driver";

        if (readlink(devicedir.c_str(), buffer, sizeof(buffer)) > 0)
        {
            return basename(buffer);
        }
    }
    return "";
}

void DVSerialEngine::register_comport(std::list<std::string>& comList,
        std::list<std::string>& comList8250, const std::string& dir)
{
    // Get the driver the device is using
    std::string driver = get_driver(dir);

    // Skip devices without a driver
    if (driver.size() > 0)
    {
        //std::cerr << "register_comport: dir: "<< dir << " driver: " << driver << std::endl;
        std::string devfile = std::string("/dev/") + basename((char *) dir.c_str());

        // Put serial8250-devices in a seperate list
        if (driver == "serial8250")
        {
            comList8250.push_back(devfile);
        }
        else
            comList.push_back(devfile);
    }
}

void DVSerialEngine::probe_serial8250_comports(std::list<std::string>& comList,
        std::list<std::string> comList8250)
{
    struct serial_struct serinfo;
    std::list<std::string>::iterator it = comList8250.begin();

    // Iterate over all serial8250-devices
    while (it != comList8250.end())
    {

        // Try to open the device
        int fd = open((*it).c_str(), O_RDWR | O_NONBLOCK | O_NOCTTY);

        if (fd >= 0)
        {
            // Get serial_info
            if (ioctl(fd, TIOCGSERIAL, &serinfo) == 0)
            {
                // If device type is no PORT_UNKNOWN we accept the port
                if (serinfo.type != PORT_UNKNOWN)
                    comList.push_back(*it);
            }
            close(fd);
        }
        it++;
    }
}

void DVSerialEngine::getComList()
{
    int n;
    struct dirent **namelist;
    m_comList.clear();
    m_comList8250.clear();
    const char* sysdir = "/sys/class/tty/";

    // Scan through /sys/class/tty - it contains all tty-devices in the system
    n = scandir(sysdir, &namelist, NULL, NULL);
    if (n < 0)
        perror("scandir");
    else
    {
        while (n--)
        {
            if (strcmp(namelist[n]->d_name, "..")
                    && strcmp(namelist[n]->d_name, "."))
            {

                // Construct full absolute file path
                std::string devicedir = sysdir;
                devicedir += namelist[n]->d_name;

                // Register the device
                register_comport(m_comList, m_comList8250, devicedir);
            }
            free(namelist[n]);
        }
        free(namelist);
    }

    // Only non-serial8250 has been added to comList without any further testing
    // serial8250-devices must be probe to check for validity
    probe_serial8250_comports(m_comList, m_comList8250);
}
#endif // __WINDOWS__

bool DVSerialEngine::scan()
{
    getComList();
    std::list<std::string>::iterator it = m_comList.begin();

    while (it != m_comList.end())
    {
        DVSerialWorker *worker = new DVSerialWorker();

        if (worker->open(*it))
        {
            DVSerialController controller;
            controller.worker = worker;
            controller.device = *it;
            controller.thread = new QThread();

            controller.worker->moveToThread(controller.thread);
            //connect(controller.thread, SIGNAL(started()), controller.worker, SLOT(process()));
            connect(controller.worker, SIGNAL(finished()), controller.thread, SLOT(quit()));
            connect(controller.worker, SIGNAL(finished()), controller.worker, SLOT(deleteLater()));
            connect(controller.thread, SIGNAL(finished()), controller.thread, SLOT(deleteLater()));
            connect(&controller.worker->m_inputMessageQueue, SIGNAL(messageEnqueued()), controller.worker, SLOT(handleInputMessages()));
            controller.thread->start();

            m_controllers.push_back(controller);
            qDebug() << "DVSerialEngine::scan: found device at: " << it->c_str();
        }
        else
        {
            delete worker;
        }

        it++;
    }

    return m_controllers.size() > 0;
}

void DVSerialEngine::release()
{
    qDebug("DVSerialEngine::release");
    std::vector<DVSerialController>::iterator it = m_controllers.begin();

    while (it != m_controllers.end())
    {
        disconnect(&it->worker->m_inputMessageQueue, SIGNAL(messageEnqueued()), it->worker, SLOT(handleInputMessages()));
        it->worker->stop();
        it->thread->wait(100);
        it->worker->m_inputMessageQueue.clear();
        it->worker->close();
        qDebug() << "DVSerialEngine::release: closed device at: " << it->device.c_str();
        ++it;
    }

    m_controllers.clear();
}

void DVSerialEngine::getDevicesNames(std::vector<std::string>& deviceNames)
{
    std::vector<DVSerialController>::iterator it = m_controllers.begin();

    while (it != m_controllers.end())
    {
        deviceNames.push_back(it->device);
        ++it;
    }
}

void DVSerialEngine::pushMbeFrame(const unsigned char *mbeFrame, int mbeRateIndex, int mbeVolumeIndex, unsigned char channels, AudioFifo *audioFifo)
{
    std::vector<DVSerialController>::iterator it = m_controllers.begin();
    std::vector<DVSerialController>::iterator itAvail = m_controllers.end();
    bool done = false;
    QMutexLocker locker(&m_mutex);

    while (it != m_controllers.end())
    {
        if (it->worker->hasFifo(audioFifo))
        {
            it->worker->pushMbeFrame(mbeFrame, mbeRateIndex, mbeVolumeIndex, channels, audioFifo);
            done = true;
        }
        else if (it->worker->isAvailable())
        {
            itAvail = it;
        }

        ++it;
    }

    if (!done)
    {
        if (itAvail != m_controllers.end())
        {
            int wNum = itAvail - m_controllers.begin();

            qDebug("DVSerialEngine::pushMbeFrame: push %p on empty queue %d", audioFifo, wNum);
            itAvail->worker->pushMbeFrame(mbeFrame, mbeRateIndex, mbeVolumeIndex, channels, audioFifo);
        }
        else
        {
            qDebug("DVSerialEngine::pushMbeFrame: no DV serial device available. MBE frame dropped");
        }
    }
}
