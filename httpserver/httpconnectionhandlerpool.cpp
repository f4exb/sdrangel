#ifndef QT_NO_OPENSSL
    #include <QSslSocket>
    #include <QSslKey>
    #include <QSslCertificate>
    #include <QSslConfiguration>
#endif
#include <QDir>
#include "httpconnectionhandlerpool.h"

using namespace qtwebapp;

HttpConnectionHandlerPool::HttpConnectionHandlerPool(QSettings* settings, HttpRequestHandler* requestHandler)
    : QObject(), useQtSettings(true)
{
    Q_ASSERT(settings != 0);
    this->settings = settings;
    this->listenerSettings = 0;
    this->requestHandler = requestHandler;
    this->sslConfiguration = 0;
    loadSslConfig();
    cleanupTimer.start(settings->value("cleanupInterval",1000).toInt());
    connect(&cleanupTimer, SIGNAL(timeout()), SLOT(cleanup()));
}

HttpConnectionHandlerPool::HttpConnectionHandlerPool(const HttpListenerSettings* settings, HttpRequestHandler* requestHandler)
    : QObject(), useQtSettings(false)
{
    Q_ASSERT(settings != 0);
    this->settings = 0;
    this->listenerSettings = settings;
    this->requestHandler = requestHandler;
    this->sslConfiguration = 0;
    loadSslConfig();
    cleanupTimer.start(settings->cleanupInterval);
    connect(&cleanupTimer, SIGNAL(timeout()), SLOT(cleanup()));
}

HttpConnectionHandlerPool::~HttpConnectionHandlerPool()
{
    // delete all connection handlers and wait until their threads are closed
    foreach(HttpConnectionHandler* handler, pool)
    {
       delete handler;
    }
    delete sslConfiguration;
    qDebug("HttpConnectionHandlerPool (%p): destroyed", this);
}


HttpConnectionHandler* HttpConnectionHandlerPool::getConnectionHandler()
{
    HttpConnectionHandler* freeHandler=0;
    mutex.lock();
    // find a free handler in pool
    foreach(HttpConnectionHandler* handler, pool)
    {
        if (!handler->isBusy())
        {
            freeHandler=handler;
            freeHandler->setBusy();
            break;
        }
    }
    // create a new handler, if necessary
    if (!freeHandler)
    {
        int maxConnectionHandlers = useQtSettings ? settings->value("maxThreads",100).toInt() : listenerSettings->maxThreads;
        if (pool.count()<maxConnectionHandlers)
        {
            if (useQtSettings) {
                freeHandler = new HttpConnectionHandler(settings, requestHandler, sslConfiguration);
            } else {
                freeHandler = new HttpConnectionHandler(listenerSettings, requestHandler, sslConfiguration);
            }

            freeHandler->setBusy();
            pool.append(freeHandler);
        }
    }
    mutex.unlock();
    return freeHandler;
}


void HttpConnectionHandlerPool::cleanup()
{
    int maxIdleHandlers = useQtSettings ? settings->value("minThreads",1).toInt() : listenerSettings->minThreads;
    int idleCounter=0;
    mutex.lock();
    foreach(HttpConnectionHandler* handler, pool)
    {
        if (!handler->isBusy())
        {
            if (++idleCounter > maxIdleHandlers)
            {
                pool.removeOne(handler);
                qDebug("HttpConnectionHandlerPool: Removed connection handler (%p), pool size is now %i",handler,(int)pool.size());
                delete handler;
                break; // remove only one handler in each interval
            }
        }
    }
    mutex.unlock();
}


void HttpConnectionHandlerPool::loadSslConfig()
{
    // If certificate and key files are configured, then load them
    QString sslKeyFileName = useQtSettings ? settings->value("sslKeyFile","").toString() : listenerSettings->sslKeyFile;
    QString sslCertFileName = useQtSettings ? settings->value("sslCertFile","").toString() : listenerSettings->sslCertFile;
    if (!sslKeyFileName.isEmpty() && !sslCertFileName.isEmpty())
    {
        #ifdef QT_NO_OPENSSL
            qWarning("HttpConnectionHandlerPool::loadSslConfig: SSL is not supported");
        #else
            // Convert relative fileNames to absolute, based on the directory of the config file.
            QFileInfo configFile(settings->fileName());
            #ifdef Q_OS_WIN32
                if (QDir::isRelativePath(sslKeyFileName) && settings->format()!=QSettings::NativeFormat)
            #else
                if (QDir::isRelativePath(sslKeyFileName))
            #endif
            {
                sslKeyFileName=QFileInfo(configFile.absolutePath(),sslKeyFileName).absoluteFilePath();
            }
            #ifdef Q_OS_WIN32
                if (QDir::isRelativePath(sslCertFileName) && settings->format()!=QSettings::NativeFormat)
            #else
                if (QDir::isRelativePath(sslCertFileName))
            #endif
            {
                sslCertFileName=QFileInfo(configFile.absolutePath(),sslCertFileName).absoluteFilePath();
            }

            // Load the SSL certificate
            QFile certFile(sslCertFileName);
            if (!certFile.open(QIODevice::ReadOnly))
            {
                qCritical("HttpConnectionHandlerPool: cannot open sslCertFile %s", qPrintable(sslCertFileName));
                return;
            }
            QSslCertificate certificate(&certFile, QSsl::Pem);
            certFile.close();

            // Load the key file
            QFile keyFile(sslKeyFileName);
            if (!keyFile.open(QIODevice::ReadOnly))
            {
                qCritical("HttpConnectionHandlerPool: cannot open sslKeyFile %s", qPrintable(sslKeyFileName));
                return;
            }
            QSslKey sslKey(&keyFile, QSsl::Rsa, QSsl::Pem);
            keyFile.close();

            // Create the SSL configuration
            sslConfiguration=new QSslConfiguration();
            sslConfiguration->setLocalCertificate(certificate);
            sslConfiguration->setPrivateKey(sslKey);
            sslConfiguration->setPeerVerifyMode(QSslSocket::VerifyNone);
            sslConfiguration->setProtocol(QSsl::TlsV1_0);

            qDebug("HttpConnectionHandlerPool: SSL settings loaded");
         #endif
    }
}
