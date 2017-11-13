/*
 * httpdocrootsettings.h
 *
 *  Created on: Nov 13, 2017
 *      Author: f4exb
 */

#ifndef HTTPSERVER_HTTPDOCROOTSETTINGS_H_
#define HTTPSERVER_HTTPDOCROOTSETTINGS_H_

namespace qtwebapp {

struct HttpDocrootSettings
{
    QString path;
    QString encoding;
    int maxAge;
    int cacheTime;
    int cacheSize;
    int maxCachedFileSize;

    HttpDocrootSettings() {
        resetToDefaults();
    }

    void resetToDefaults()
    {
        path = ".";
        encoding = "UTF-8";
        maxAge = 60000;
        cacheTime = 60000;
        cacheSize = 1000000;
        maxCachedFileSize = 65536;
    }
};

} // end of namespace

#endif /* HTTPSERVER_HTTPDOCROOTSETTINGS_H_ */
