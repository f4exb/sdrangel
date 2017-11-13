/*
 * httplistenersettings.h
 *
 *  Created on: Nov 13, 2017
 *      Author: f4exb
 */

#ifndef HTTPSERVER_HTTPLISTENERSETTINGS_H_
#define HTTPSERVER_HTTPLISTENERSETTINGS_H_

namespace qtwebapp {

struct HttpListenerSettings
{
    QString host;
    int port;
    int minThreads;
    int maxThreads;
    int cleanupInterval;
    int readTimeout;
    QString sslKeyFile;
    QString sslCertFile;
    int maxRequestSize;
    int maxMultiPartSize;

    HttpListenerSettings() {
        resetToDefaults();
    }

    void resetToDefaults()
    {
        host = "192.168.0.100";
        port = 8080;
        minThreads = 1;
        maxThreads = 100;
        cleanupInterval = 1000;
        readTimeout = 10000;
        sslKeyFile = "";
        sslCertFile = "";
        maxRequestSize = 16000;
        maxMultiPartSize = 1000000;
    }
};

} // end of namespace

#endif /* HTTPSERVER_HTTPLISTENERSETTINGS_H_ */
