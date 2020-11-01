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

#ifndef __APPLE__
#include <fcntl.h>
#include <sys/stat.h>
#endif

#ifndef _MSC_VER
#include <dirent.h>
#include <unistd.h>
#include <libgen.h>
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#endif

#include <chrono>
#include <thread>

#include <QThread>
#include <QBuffer>
#include <QDataStream>

#include "ambeworker.h"
#include "ambeengine.h"

AMBEEngine::AMBEEngine()
{}

AMBEEngine::~AMBEEngine()
{
    qDebug("AMBEEngine::~AMBEEngine: %lu controllers", m_controllers.size());
}

#if defined(_WIN32)
void AMBEEngine::getComList()
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

// Do not activate serial support at all for windows
void AMBEEngine::scan(std::vector<QString>& ambeDevices)
{
    (void) ambeDevices;
}
#elif defined(__APPLE__)
void AMBEEngine::getComList()
{
}
void AMBEEngine::scan(std::vector<QString>& ambeDevices)
{
    (void) ambeDevices;
}
#else
void AMBEEngine::getComList()
{
    int n;
    struct dirent **namelist;
    m_comList.clear();
    m_comList8250.clear();
    const char* sysdir = "/sys/class/tty/";

    // Scan through /sys/class/tty - it contains all tty-devices in the system
    n = scandir(sysdir, &namelist, NULL, alphasort);
    if (n < 0)
    {
        perror("scandir");
    }
    else
    {
        while (n--)
        {
            if (strcmp(namelist[n]->d_name, "..") && strcmp(namelist[n]->d_name, "."))
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
#endif // not Windows nor Apple

#if !defined(_WIN32) && !defined(__APPLE__)
void AMBEEngine::register_comport(
    std::vector<std::string>& comList,
    std::vector<std::string>& comList8250,
    const std::string& dir)
{
    // Get the driver the device is using
    std::string driver = get_driver(dir);

    // Skip devices without a driver
    if (driver.size() > 0)
    {
        //std::cerr << "register_comport: dir: "<< dir << " driver: " << driver << std::endl;
        std::string devfile = std::string("/dev/") + basename((char *) dir.c_str());

        // Put serial8250-devices in a seperate list
        if (driver == "serial8250") {
            comList8250.push_back(devfile);
        } else {
            comList.push_back(devfile);
        }
    }
}

void AMBEEngine::probe_serial8250_comports(
    std::vector<std::string>& comList,
    std::vector<std::string> comList8250)
{
    struct serial_struct serinfo;
    std::vector<std::string>::iterator it = comList8250.begin();

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
                if (serinfo.type != PORT_UNKNOWN) {
                    comList.push_back(*it);
                }
            }

            close(fd);
        }
        it++;
    }
}

std::string AMBEEngine::get_driver(const std::string& tty)
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

        if (readlink(devicedir.c_str(), buffer, sizeof(buffer)) > 0) {
            return basename(buffer);
        }
    }

    return "";
}

void AMBEEngine::scan(std::vector<QString>& ambeDevices)
{
    getComList();
    std::vector<std::string>::const_iterator it = m_comList.begin();
    ambeDevices.clear();

    while (it != m_comList.end())
    {
        AMBEWorker *worker = new AMBEWorker();
        qDebug("AMBEEngine::scan: com: %s", it->c_str());

        if (worker->open(*it))
        {
            ambeDevices.push_back(QString(it->c_str()));
            worker->close();
        }

        delete worker;
        ++it;
    }
}
#endif // not Windows nor Apple

bool AMBEEngine::registerController(const std::string& deviceRef)
{
    AMBEWorker *worker = new AMBEWorker();

    if (worker->open(deviceRef))
    {
        m_controllers.push_back(AMBEController());
        m_controllers.back().worker = worker;
        m_controllers.back().thread = new QThread();
        m_controllers.back().device = deviceRef;

        m_controllers.back().worker->moveToThread(m_controllers.back().thread);
        connect(m_controllers.back().worker, SIGNAL(finished()), m_controllers.back().thread, SLOT(quit()));
        connect(m_controllers.back().worker, SIGNAL(finished()), m_controllers.back().worker, SLOT(deleteLater()));
        connect(m_controllers.back().thread, SIGNAL(finished()), m_controllers.back().thread, SLOT(deleteLater()));
        connect(&m_controllers.back().worker->m_inputMessageQueue, SIGNAL(messageEnqueued()), m_controllers.back().worker, SLOT(handleInputMessages()));
        std::this_thread::sleep_for(std::chrono::seconds(1));
        m_controllers.back().thread->start();

        return true;
    }

    return false;
}

void AMBEEngine::releaseController(const std::string& deviceRef)
{
    std::vector<AMBEController>::iterator it = m_controllers.begin();

    while (it != m_controllers.end())
    {
        if (it->device == deviceRef)
        {
            disconnect(&it->worker->m_inputMessageQueue, SIGNAL(messageEnqueued()), it->worker, SLOT(handleInputMessages()));
            it->worker->stop();
            it->thread->wait(100);
            it->worker->m_inputMessageQueue.clear();
            it->worker->close();
            qDebug() << "AMBEEngine::releaseController: closed device at: " << it->device.c_str();
            m_controllers.erase(it);
            break;
        }

        ++it;
    }
}

void AMBEEngine::releaseAll()
{
    std::vector<AMBEController>::iterator it = m_controllers.begin();

    while (it != m_controllers.end())
    {
        disconnect(&it->worker->m_inputMessageQueue, SIGNAL(messageEnqueued()), it->worker, SLOT(handleInputMessages()));
        it->worker->stop();
        it->thread->wait(100);
        it->worker->m_inputMessageQueue.clear();
        it->worker->close();
        qDebug() << "AMBEEngine::release: closed device at: " << it->device.c_str();
        ++it;
    }

    m_controllers.clear();
}

void AMBEEngine::getDeviceRefs(std::vector<QString>& deviceNames)
{
    std::vector<AMBEController>::const_iterator it = m_controllers.begin();

    while (it != m_controllers.end())
    {
        deviceNames.push_back(QString(it->device.c_str()));
        ++it;
    }
}

void AMBEEngine::pushMbeFrame(
        const unsigned char *mbeFrame,
        int mbeRateIndex,
        int mbeVolumeIndex,
        unsigned char channels,
        bool useLP,
        int upsampling,
        AudioFifo *audioFifo)
{
    std::vector<AMBEController>::iterator it = m_controllers.begin();
    std::vector<AMBEController>::iterator itAvail = m_controllers.end();
    bool done = false;
    QMutexLocker locker(&m_mutex);

    while (it != m_controllers.end())
    {
        if (it->worker->hasFifo(audioFifo))
        {
            it->worker->pushMbeFrame(mbeFrame, mbeRateIndex, mbeVolumeIndex, channels, useLP, upsampling, audioFifo);
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

            qDebug("AMBEEngine::pushMbeFrame: push %p on empty queue %d", audioFifo, wNum);
            itAvail->worker->pushMbeFrame(mbeFrame, mbeRateIndex, mbeVolumeIndex, channels, useLP, upsampling, audioFifo);
        }
        else
        {
            qDebug("AMBEEngine::pushMbeFrame: no DV device available. MBE frame dropped");
        }
    }
}

QByteArray AMBEEngine::serialize() const
{
    QStringList qDeviceList;
    std::vector<AMBEController>::const_iterator it = m_controllers.begin();

    while (it != m_controllers.end())
    {
        qDebug("AMBEEngine::serialize: %s", it->device.c_str());
        qDeviceList << QString(it->device.c_str());
        ++it;
    }

    QByteArray data;
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    (*stream) << qDeviceList;
    delete stream;

    return data;
}

bool AMBEEngine::deserialize(const QByteArray& data)
{
    if (data.size() <= 0)
    {
        qDebug("AMBEEngine::deserialize: invalid or no data");
        return false;
    }

    QStringList qDeviceList;
    QDataStream *stream = new QDataStream(data);
    (*stream) >> qDeviceList;
    delete stream;

    releaseAll();

    for (int i = 0; i < qDeviceList.size(); ++i)
    {
        qDebug(" AMBEEngine::deserialize: %s", qDeviceList.at(i).toStdString().c_str());
        registerController(qDeviceList.at(i).toStdString());
    }

    return true;
}
