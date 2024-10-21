///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifdef ANDROID

#include <QDebug>

#include <android/log.h>

#include "android.h"

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))

#include <QtCore/private/qandroidextras_p.h>
#include <QJniObject>
#include <QJniEnvironment>

void Android::sendIntent()
{
    QJniObject url = QJniObject::fromString("iqsrc://-f 1090000000 -p 1234 -s 2000000 -a 127.0.0.1 -g 100");
    QJniObject intent = QJniObject::callStaticObjectMethod("android/content/Intent", "parseUri", "(Ljava/lang/String;I)Landroid/content/Intent;", url.object<jstring>(), 0x00000001);   // Creates Intent(ACTION_VIEW, url)
    QtAndroidPrivate::startActivity(intent, 0, [](int requestCode, int resultCode, const QJniObject &data) {
        (void) data;

        qDebug() << "MainCore::sendIntent " << requestCode << resultCode;
    });
}

QStringList Android::listUSBDeviceSerials(int vid, int pid)
{
    QStringList serials;
    QJniEnvironment jniEnv;

    QJniObject activity = QJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid())
    {
        QJniObject serialsObj = activity.callObjectMethod("listUSBDeviceSerials", "(II)[Ljava/lang/String;", vid, pid);
        int serialsLen = jniEnv->GetArrayLength(serialsObj.object<jarray>());
        for (int i = 0; i < serialsLen; i++)
        {
            QJniObject arrayElement = jniEnv->GetObjectArrayElement(serialsObj.object<jobjectArray>(), i);
            QString serial = arrayElement.toString();
            serials.append(serial);
        }
    }

    return serials;
}

int Android::openUSBDevice(const QString &serial)
{
    int fd = -1;
    QJniObject activity = QJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid())
    {
        QJniObject serialsObj = QJniObject::fromString(serial);
        fd = activity.callMethod<jint>("openUSBDevice", "(Ljava/lang/String;)I", serialsObj.object<jstring>());
    }

    return fd;
}

void Android::closeUSBDevice(int fd)
{
    if (fd >= 0)
    {
        QJniObject activity = QJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
        if (activity.isValid()) {
            activity.callMethod<void>("closeUSBDevice", "(I)V", fd);
        } else {
            qCritical() << "MainCore::closeUSBDevice: activity is not valid.";
        }
    }
}

void Android::moveTaskToBack()
{
    QJniObject activity = QJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid()) {
        activity.callMethod<jboolean>("moveTaskToBack", "(Z)Z", true);
    }
}

void Android::acquireWakeLock()
{
    QJniObject activity = QJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid()) {
        activity.callMethod<void>("acquireWakeLock");
    }
}

void Android::releaseWakeLock()
{
    QJniObject activity = QJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid()) {
        activity.callMethod<void>("releaseWakeLock");
    }
}

void Android::acquireScreenLock()
{
    QJniObject activity = QJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid()) {
        activity.callMethod<void>("acquireScreenLock");
    }
}

void Android::releaseScreenLock()
{
    QJniObject activity = QJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid()) {
        activity.callMethod<void>("releaseScreenLock");
    }
}

#else // QT_VERSION

#include <QtAndroid>
#include <QAndroidIntent>
#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>
#include <QAndroidActivityResultReceiver>

void Android::sendIntent()
{
    QAndroidJniObject url = QAndroidJniObject::fromString("iqsrc://-f 1090000000 -p 1234 -s 2000000 -a 127.0.0.1 -g 100");
    QAndroidJniObject intent = QAndroidJniObject::callStaticObjectMethod("android/content/Intent", "parseUri", "(Ljava/lang/String;I)Landroid/content/Intent;", url.object<jstring>(), 0x00000001);   // Creates Intent(ACTION_VIEW, url)
    QtAndroid::startActivity(intent, 0, [](int requestCode, int resultCode, const QAndroidJniObject &data) {
        (void) data;

        qDebug() << "MainCore::sendIntent " << requestCode << resultCode;
    });
}

QStringList Android::listUSBDeviceSerials(int vid, int pid)
{
    QStringList serials;
    QAndroidJniEnvironment jniEnv;

    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid())
    {
        QAndroidJniObject serialsObj = activity.callObjectMethod("listUSBDeviceSerials", "(II)[Ljava/lang/String;", vid, pid);
        int serialsLen = jniEnv->GetArrayLength(serialsObj.object<jarray>());
        for (int i = 0; i < serialsLen; i++)
        {
            QAndroidJniObject arrayElement = jniEnv->GetObjectArrayElement(serialsObj.object<jobjectArray>(), i);
            QString serial = arrayElement.toString();
            serials.append(serial);
        }
    }

    return serials;
}

int Android::openUSBDevice(const QString &serial)
{
    int fd = -1;
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid())
    {
        QAndroidJniObject serialsObj = QAndroidJniObject::fromString(serial);
        fd = activity.callMethod<jint>("openUSBDevice", "(Ljava/lang/String;)I", serialsObj.object<jstring>());
    }

    return fd;
}

void Android::closeUSBDevice(int fd)
{
    if (fd >= 0)
    {
        QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
        if (activity.isValid()) {
            activity.callMethod<void>("closeUSBDevice", "(I)V", fd);
        }
    }
}

void Android::moveTaskToBack()
{
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid()) {
        activity.callMethod<jboolean>("moveTaskToBack", "(Z)Z", true);
    }
}

void Android::acquireWakeLock()
{
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid()) {
        activity.callMethod<void>("acquireWakeLock");
    }
}

void Android::releaseWakeLock()
{
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid()) {
        activity.callMethod<void>("releaseWakeLock");
    }
}

void Android::acquireScreenLock()
{
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid()) {
        activity.callMethod<void>("acquireScreenLock");
    }
}

void Android::releaseScreenLock()
{
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid()) {
        activity.callMethod<void>("releaseScreenLock");
    }
}

#endif // QT6

// Redirect qDebug/qWarning to Android log, so we can view remotely with adb
void Android::messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    QString report = msg;
    if (context.file && !QString(context.file).isEmpty())
    {
        report += " in file ";
        report += QString(context.file);
        report += " line ";
        report += QString::number(context.line);
    }
    if (context.function && !QString(context.function).isEmpty())
    {
        report += +" function ";
        report += QString(context.function);
    }
    const char * const local = report.toLocal8Bit().constData();
    const char * const applicationName = "sdrangel";
    int ret;
    switch (type)
    {
    case QtDebugMsg:
        ret = __android_log_write(ANDROID_LOG_DEBUG, applicationName, local);
        break;
    case QtInfoMsg:
        ret = __android_log_write(ANDROID_LOG_INFO, applicationName, local);
        break;
    case QtWarningMsg:
        ret = __android_log_write(ANDROID_LOG_WARN, applicationName, local);
        break;
    case QtCriticalMsg:
        ret = __android_log_write(ANDROID_LOG_ERROR, applicationName, local);
        break;
    case QtFatalMsg:
    default:
        ret = __android_log_write(ANDROID_LOG_FATAL, applicationName, local);
        abort();
    }
    if (ret < 0) {
        __android_log_write(ANDROID_LOG_ERROR, applicationName, "Error writing to log");
    }
}

#endif // ANDROID
