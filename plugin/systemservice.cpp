// Copyright (c) 2012-2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "systemservice.h"

#include <QJsonDocument>
#include <QStringList>
#include <QDebug>

static const QLatin1String strKeys("keys");
static const QLatin1String strWallPaper("wallpaper");
static const QLatin1String strWallPaperFile("wallpaperFile");
static const QLatin1String strTimeFormat("timeFormat");
static const QLatin1String strAirPlane("airplaneMode");
static const QLatin1String strRotationLock("rotationLock");
static const QLatin1String strMuteSound("muteSound");
static const QLatin1String strLockTimeOut("lockTimeout");
static const QLatin1String strUtc("utc");
static const QLatin1String methodGetPreferences("/getPreferences");
static const QLatin1String methodSetPreferences("/setPreferences");
static const QLatin1String methodTimeGetSystemTime("/time/getSystemTime");
static const QLatin1String serviceName("com.palm.systemservice");

SystemService::SystemService(QObject * parent)
    : Service(parent)
{
}

QUrl SystemService::wallpaper()
{
    getPreference(strWallPaper);
    return m_wallpaper;
}

QString SystemService::timeFormat()
{
    getPreference(strTimeFormat);
    return m_timeFormat;
}

bool SystemService::airplaneMode()
{
    getPreference(strAirPlane);
    return m_airplaneMode;
}

bool SystemService::rotationLock()
{
    getPreference(strRotationLock);
    return m_rotationLock;
}

bool SystemService::muteSound()
{
    getPreference(strMuteSound);
    return m_muteSound;
}

int SystemService::lockTimeout()
{
    getPreference(strLockTimeOut);
    return m_lockTimeout;
}

QDateTime SystemService::systemTime()
{
    call(serviceUri(),
         methodTimeGetSystemTime,
         QString(QLatin1String("{\"%1\":%2}")).arg(strSubscribe).arg(strTrue));
    return m_systemTime;
}

void SystemService::setWallpaper(const QUrl& url)
{
    qWarning() << "Not implemented yet!";
    Q_UNUSED(url)
}

void SystemService::setTimeFormat(const QString& timeFormat)
{
    setPreference(strTimeFormat, timeFormat);
}

void SystemService::setAirplaneMode(bool enabled)
{
    setPreference(strAirPlane, enabled ? strTrue : strFalse);
}

void SystemService::setRotationLock(bool enabled)
{
    setPreference(strRotationLock, enabled ? strTrue : strFalse);
}

void SystemService::setMuteSound(bool enabled)
{
    setPreference(strMuteSound, enabled ? strTrue : strFalse);
}

void SystemService::setLockTimeout(int seconds)
{
    setPreference(strLockTimeOut, QString::number(seconds));
}

void SystemService::getPreference(const QString& key)
{
    call(serviceUri(),
         methodGetPreferences,
         QString(QLatin1String("{\"%1\":[\"%2\"], \"%3\":%4}")).arg(strKeys).arg(key).arg(strSubscribe).arg(strTrue));
}

void SystemService::setPreference(const QString& key, const QString& value)
{
    call(serviceUri(),
         methodSetPreferences,
         QString(QLatin1String("{\"%1\":%2}")).arg(key).arg(value));
}

void SystemService::serviceResponse( const QString& method, const QString& payload, int token )
{
    checkForErrors(payload, token);
    Q_EMIT response(method, payload, token);

    // qDebug() << Q_FUNC_INFO << "objectName: " << objectName() << "method: " <<  method << "payload: " << payload;

    if ( method == methodGetPreferences || method == methodSetPreferences) {
        QJsonObject rootObject = QJsonDocument::fromJson(payload.toUtf8()).object();

        rootObject.take(strReturnValue);

        if ( rootObject.count() == 0 ) {
            return;
        }

        // TODO: Needs refactoring (queueing, call tracking)
        QString key = rootObject.begin().key();
        QJsonValue value = rootObject.begin().value();

        if ( key == strWallPaper) { QString wallpaper = value.toObject().find(strWallPaperFile).value().toString();
                                           if (m_wallpaper == wallpaper) return;
                                           m_wallpaper = QUrl(wallpaper);
                                           emit wallpaperChanged(); }
        else if ( key == strTimeFormat) { QString timeFormat = value.toString();
                                           if (m_timeFormat == timeFormat) return;
                                           m_timeFormat = timeFormat;
                                           emit timeFormatChanged(); }
        else if ( key == strAirPlane) { bool airplaneMode = value.toBool();
                                           if (m_airplaneMode == airplaneMode) return;
                                           m_airplaneMode = airplaneMode;
                                           emit airplaneModeChanged(); }
        else if ( key == strRotationLock) { bool rotationLock = value.toBool();
                                           if (m_rotationLock == rotationLock) return;
                                           m_rotationLock = rotationLock;
                                           emit rotationLockChanged(); }
        else if ( key == strMuteSound) { bool muteSound = value.toBool();
                                           if (m_muteSound == muteSound) return;
                                           m_muteSound = muteSound;
                                           emit muteSoundChanged(); }
        else if ( key == strLockTimeOut) { int lockTimeout = value.toDouble();
                                           if (m_lockTimeout == lockTimeout) return;
                                           m_lockTimeout = lockTimeout;
                                           emit lockTimeoutChanged(); }
    }
    else if ( method == methodTimeGetSystemTime ) {
        QJsonObject rootObject = QJsonDocument::fromJson(payload.toUtf8()).object();

        int systemSeconds = rootObject.find(strUtc).value().toDouble();
        QDateTime systemTime = QDateTime::fromTime_t(systemSeconds);
        m_systemTime = systemTime;
        emit systemTimeChanged();
    }
    else {
        qWarning() << "Unknown method";
    }
}

QString SystemService::interfaceName() const
{
    return QString(serviceName);
}
