/*
 * httpsessionssettings.h
 *
 *  Created on: Nov 13, 2017
 *      Author: f4exb
 */

#ifndef HTTPSERVER_HTTPSESSIONSSETTINGS_H_
#define HTTPSERVER_HTTPSESSIONSSETTINGS_H_

namespace qtwebapp {

struct HttpSessionsSettings
{
    int expirationTime;
    QString cookieName;
    QString cookiePath;
    QString cookieComment;
    QString cookieDomain;

    HttpSessionsSettings() {
        resetToDefaults();
    }

    void resetToDefaults()
    {
        expirationTime = 3600000;
        cookieName = "sessionid";
        cookiePath = "";
        cookieComment = "";
        cookieDomain = "";
    }
};

} // end of namespace




#endif /* HTTPSERVER_HTTPSESSIONSSETTINGS_H_ */
