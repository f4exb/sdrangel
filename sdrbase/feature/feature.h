///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// API for features                                                              //
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

#ifndef SDRBASE_FETURE_FEATUREAPI_H_
#define SDRBASE_FETURE_FEATUREAPI_H_

#include <QObject>
#include <QString>
#include <QByteArray>

#include "export.h"
#include "util/messagequeue.h"

class WebAPIAdapterInterface;

class SDRBASE_API Feature : public QObject {
    Q_OBJECT
public:
    enum FeatureState {
		StNotStarted,  //!< feature is before initialization
		StIdle,        //!< feature is idle
		StRunning,     //!< feature is running
		StError        //!< feature is in error
	};

    Feature(const QString& name, WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~Feature() {}
    virtual void destroy() = 0;
	virtual bool handleMessage(const Message& cmd) = 0; //!< Processing of a message. Returns true if message has actually been processed

    virtual void getIdentifier(QString& id) = 0;
    virtual void getTitle(QString& title) = 0;
    virtual void setName(const QString& name) { m_name = name; }
    virtual const QString& getName() const { return m_name; }

    virtual QByteArray serialize() const = 0;
    virtual bool deserialize(const QByteArray& data) = 0;


    uint64_t getUID() const { return m_uid; }
    FeatureState getState() const { return m_state; }
    const QString& getErrorMessage() const { return m_errorMessage; }

	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }

protected:
    MessageQueue m_inputMessageQueue;
    MessageQueue *m_guiMessageQueue;  //!< Input message queue to the GUI
    FeatureState m_state;
    QString m_errorMessage;
    WebAPIAdapterInterface *m_webAPIAdapterInterface;

protected slots:
	void handleInputMessages();

private:
    QString m_name;
    uint64_t m_uid;
};

#endif // SDRBASE_FETURE_FEATUREAPI_H_