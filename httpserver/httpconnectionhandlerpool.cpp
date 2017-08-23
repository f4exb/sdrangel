#ifndef QT_NO_OPENSSL
    #include <QSslSocket>
    #include <QSslKey>
    #include <QSslCertificate>
    #include <QSslConfiguration>
#endif
#include <QDir>
#include "httpconnectionhandlerpool.h"

using namespace stefanfrings;

HttpConnectionHandlerPool::HttpConnectionHandlerPool(QSettings* settings, HttpRequestHandler* requestHandler)
    : QObject()
{
    Q_ASSERT(settings!=0);
    this->settings=settings;
    this->requestHandler=requestHandler;
    this->sslConfiguration=NULL;
    loadSslConfig();
    cleanupTimer.start(settings->value("cleanupInterval",1000).toInt());
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
        int maxConnectionHandlers=settings->value("maxThreads",100).toInt();
        if (pool.count()<maxConnectionHandlers)
        {
            freeHandler=new HttpConnectionHandler(settings,requestHandler,sslConfiguration);
            freeHandler->setBusy();
            pool.append(freeHandler);
        }
    }
    mutex.unlock();
    return freeHandler;
}


void HttpConnectionHandlerPool::cleanup()
{
    int maxIdleHandlers=settings->value("minThreads",1).toInt();
    int idleCounter=0;
    mutex.lock();
    foreach(HttpConnectionHandler* handler, pool)
    {
        if (!handler->isBusy())
        {
            if (++idleCounter > maxIdleHandlers)
            {
                delete handler;
                pool.removeOne(handler);
                qDebug("HttpConnectionHandlerPool: Removed connection handler (%p), pool size is now %i",handler,pool.size());
                break; // remove only one handler in each interval
            }
        }
    }
    mutex.unlock();
}


void HttpConnectionHandlerPool::loadSslConfig()
{
    // If certificate and key files are configured, then load them
    QString sslKeyFileName=settings->value("sslKeyFile","").toString();
    QString sslCertFileName=settings->value("sslCertFile","").toString();
    if (!sslKeyFileName.isEmpty() && !sslCertFileName.isEmpty())
    {
        #ifdef QT_NO_OPENSSL
            qWarning("HttpConnectionHandlerPool: SSL is not supported");
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
            sslConfiguration->setProtocol(QSsl::TlsV1SslV3);

            qDebug("HttpConnectionHandlerPool: SSL settings loaded");
         #endif
    }
}
