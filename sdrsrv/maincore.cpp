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

#include <QDebug>
#include <unistd.h>

#include "dsp/dspengine.h"
#include "dsp/dspdevicesourceengine.h"
#include "plugin/pluginmanager.h"
#include "loggerwithfile.h"
#include "webapi/webapirequestmapper.h"
#include "webapi/webapiserver.h"
#include "webapi/webapiadaptersrv.h"

#include "maincore.h"

MESSAGE_CLASS_DEFINITION(MainCore::MsgDeleteInstance, Message)

MainCore *MainCore::m_instance = 0;

MainCore::MainCore(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QObject *parent) :
    QObject(parent),
    m_settings(),
    m_masterTabIndex(-1),
    m_dspEngine(DSPEngine::instance()),
    m_lastEngineState((DSPDeviceSourceEngine::State)-1),
    m_logger(logger),
    m_running(true)
{
    qDebug() << "MainCore::MainCore: start";

    m_instance = this;
    m_settings.setAudioDeviceInfo(&m_audioDeviceInfo);

    m_pluginManager = new PluginManager(this);
    m_pluginManager->loadPlugins(QString("pluginssrv"));

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()), Qt::QueuedConnection);
    m_masterTimer.start(50);

    m_apiAdapter = new WebAPIAdapterSrv(*this);
    m_requestMapper = new WebAPIRequestMapper(QCoreApplication::instance());
    m_requestMapper->setAdapter(m_apiAdapter);
    m_apiServer = new WebAPIServer(parser.getServerAddress(), parser.getServerPort(), m_requestMapper);
    m_apiServer->start();

    qDebug() << "MainCore::MainCore: end";
}

MainCore::~MainCore()
{
    m_apiServer->stop();
    delete m_apiServer;
    delete m_requestMapper;
    delete m_apiAdapter;

    delete m_pluginManager;

    qDebug() << "MainCore::~MainCore: end";
    delete m_logger;
}

void MainCore::run()
{
    qDebug() << "MainCore::run: start";

    while (m_running) {
        sleep(1);
    }

    qDebug() << "MainCore::run: end";
    emit finished();
}

bool MainCore::handleMessage(const Message& cmd)
{
    if (MsgDeleteInstance::match(cmd))
    {
        m_running = false;
        return true;
    }
    else
    {
        return false;
    }
}

void MainCore::handleMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("MainCore::handleMessages: message: %s", message->getIdentifier());
        handleMessage(*message);
        delete message;
    }
}

