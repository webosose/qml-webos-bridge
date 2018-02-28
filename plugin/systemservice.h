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

#ifndef SYSTEMSERVICE_H
#define SYSTEMSERVICE_H

#include "service.h"
#include <QDateTime>
#include <QJsonObject>
#include <QUrl>

    /*!
     * \class SystemService
     *
     * \brief Provides QML property bindings for the service com.palm.systemservice
     *
     * This class provides
     * \li Properties and methods for the associated service.
     * \li Qt types for properties, parameters and return values.
     *
     * \see Service
     */

class SystemService : public Service
{
    Q_OBJECT
    Q_DISABLE_COPY(SystemService)

    // properties exposed to qml and javascript
    Q_PROPERTY(QUrl wallpaper READ wallpaper WRITE setWallpaper NOTIFY wallpaperChanged)
    Q_PROPERTY(QString timeFormat READ timeFormat WRITE setTimeFormat NOTIFY timeFormatChanged)
    Q_PROPERTY(bool airplaneMode READ airplaneMode WRITE setAirplaneMode NOTIFY airplaneModeChanged)
    Q_PROPERTY(bool muteSound READ muteSound WRITE setMuteSound NOTIFY muteSoundChanged)
    Q_PROPERTY(bool rotationLock READ rotationLock WRITE setRotationLock NOTIFY rotationLockChanged)
    Q_PROPERTY(int lockTimeout READ lockTimeout WRITE setLockTimeout NOTIFY lockTimeoutChanged)
    Q_PROPERTY(QDateTime systemTime READ systemTime NOTIFY systemTimeChanged)

public:
    SystemService(QObject * parent = 0);;

    QUrl wallpaper();
    QString timeFormat();
    bool airplaneMode();
    bool muteSound();
    bool rotationLock();
    int lockTimeout();
    QDateTime systemTime();

    void serviceResponse(const QString& method, const QString& payload, int token);

    QString interfaceName() const;

Q_SIGNALS:
    void wallpaperChanged();
    void timeFormatChanged();
    void airplaneModeChanged();
    void muteSoundChanged();
    void rotationLockChanged();
    void lockTimeoutChanged();
    void systemTimeChanged();

public Q_SLOTS:
    void setWallpaper(const QUrl& url);
    void setTimeFormat(const QString& timeFormat);
    void setAirplaneMode(bool enabled);
    void setMuteSound(bool enabled);
    void setRotationLock(bool enabled);
    void setLockTimeout(int seconds);

private:
    /*!
     * \brief Get the QJsonValue for a certain key
     * (e.g. getPreference("timeFormat"))
     */
    void getPreference(const QString& key);

    /*!
     * \brief Get the QJsonValue for a certain key and subkey
     * (e.g. getPreference("wallpaper","wallpaperFile")
     */
    void getPreference(const QString& key, const QString& subKey);

    /*!
     * \brief Get the QJsonValue for a certain key and subkey
     * (e.g. getPreference("wallpaper","wallpaperFile")
     */
    void setPreference(const QString& key, const QString& value);

    QUrl m_wallpaper;
    QString m_timeFormat;
    bool m_airplaneMode;
    bool m_muteSound;
    bool m_rotationLock;
    int m_lockTimeout;
    QDateTime m_systemTime;
};

#endif // SYSTEMSERVICE_H
