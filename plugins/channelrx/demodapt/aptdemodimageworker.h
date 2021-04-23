///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_APTDEMODIMAGEWORKER_H
#define INCLUDE_APTDEMODIMAGEWORKER_H

#include <QObject>
#include <QMutex>
#include <QImage>

#include <apt.h>

#include "util/messagequeue.h"
#include "util/message.h"

#include "aptdemodsettings.h"

class APTDemodImageWorker : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureAPTDemodImageWorker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const APTDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureAPTDemodImageWorker* create(const APTDemodSettings& settings, bool force)
        {
            return new MsgConfigureAPTDemodImageWorker(settings, force);
        }

    private:
        APTDemodSettings m_settings;
        bool m_force;

        MsgConfigureAPTDemodImageWorker(const APTDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgSaveImageToDisk : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgSaveImageToDisk* create()
        {
            return new MsgSaveImageToDisk();
        }

    private:

        MsgSaveImageToDisk() :
            Message()
        {
        }
    };

    APTDemodImageWorker();
    ~APTDemodImageWorker();
    void reset();
    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_messageQueueToGUI = messageQueue; }

private:
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    MessageQueue *m_messageQueueToGUI;
    APTDemodSettings m_settings;

    // Image buffers
    apt_image_t m_image;                // Received image
    apt_image_t m_tempImage;            // Processed image
    QImage m_greyImage;
    QImage m_colourImage;
    QString m_satelliteName;

    bool m_running;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const APTDemodSettings& settings, bool force = false);
    void resetDecoder();
    void processPixels(const float *pixels);
    void sendImageToGUI();
    void sendLineToGUI();
    void saveImageToDisk();
    QImage processImage(QStringList& imageTypes);
    QImage extractImage(QImage image);

    static void copyImage(apt_image_t *dst, apt_image_t *src);
    static uchar roundAndClip(float p);

private slots:
    void handleInputMessages();
};

#endif // INCLUDE_APTDEMODIMAGEWORKER_H
