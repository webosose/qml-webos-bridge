// Copyright (c) 2012-2022 LG Electronics, Inc.
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

#ifndef APPLICATIONMANAGERSERVICE_H
#define APPLICATIONMANAGERSERVICE_H

#include "service.h"
#include <QUrl>
#include <QHash>
#include <QVariant>

    /*!
     * \class ApplicationManagerService
     * \brief Provides QML property bindings for the service
     * com.webos.applicationManager
     *
     * This class provides
     * \li Properties and methods for the associated service.
     * \li The type of properties, parameters and return values
     * matches Qt types.
     *
     * \see Service
     */

class ApplicationManagerService : public MessageSpreaderListener
{
    Q_OBJECT

    Q_PROPERTY(QString applicationList READ applicationList NOTIFY applicationListChanged)
    Q_PROPERTY(QString launchPointsList READ launchPointsList NOTIFY launchPointsListChanged)
    Q_PROPERTY(QString runningList READ runningList NOTIFY runningListChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QVariant jsonApplicationList READ jsonApplicationList NOTIFY jsonApplicationListChanged)
    Q_PROPERTY(QVariant jsonLaunchPointsList READ jsonLaunchPointsList NOTIFY jsonLaunchPointsListChanged)

Q_SIGNALS:
    /*!
     * \brief Indicates that lauching the application with
     * the given identifier (e.g. com.palm.app.calendar)
     * has been done successfully.
     */
    void launched(const QString& identifier, int token);
    void launchFailed(const QString& identifier, int token, int errorCode);
    void closed(const QString& processId, int token);
    void appLaunched(const QString& appId, const QString& title, bool noSplash, const QString& splashBackground);
    void appLifeStatusChanged(const QString& appId, const QString& status, const QString& processId, const QString& extraInfo);
    void appLifeEventsChanged(const QString& appId, const QString& event, const QString& title, bool showSpinner, bool showSplash, const QString& splashBackground);

    void applicationListChanged();
    void jsonApplicationListChanged();
    void launchPointsListChanged();
    void jsonLaunchPointsListChanged();
    void runningListChanged();
    void connectedChanged();
    void sameLaunchPointsListPublished();

public:
    ApplicationManagerService(QObject * parent = 0);

    Q_INVOKABLE void cancel(LSMessageToken token = LSMESSAGE_TOKEN_INVALID);

    /*!
     * \brief Launches an application with the given
     * identifier (e.g. com.palm.app.calendar).
     * \return Returns the token number assigned to the call
     */
    Q_INVOKABLE int launch(const QString& identifier, const QString& params = "{}", const bool checkUpdateOnLaunch = false, const bool autoInstallation = false, const QString& reason = "");
    Q_INVOKABLE int removeLaunchPoint(const QString& identifier);
    Q_INVOKABLE int close(const QString& processId);
    Q_INVOKABLE int moveLaunchPoint(int index, int to);
    Q_INVOKABLE int subscribeAppLifeStatus();
    Q_INVOKABLE int subscribeAppLifeEvents();
    Q_INVOKABLE int subscribeApplicationList();
    Q_INVOKABLE int subscribeLaunchPointsList();

    void setAppId(const QString& appId);

    QString applicationList() { return m_applicationList; };
    QVariant jsonApplicationList() { return m_jsonApplicationList; };
    QString launchPointsList() { return m_launchPointsList; };
    QVariant jsonLaunchPointsList() { return m_jsonLaunchPointsList; };
    QString runningList();
    bool connected() { return m_connected; }

    QString interfaceName() const;

protected:
    /*!
     * \brief Is used to process the reply from the bus. Emits
     * the response signal.
     * \param method The method of the service that has been called
       (e.g. "/getPreferences")
     * \param payload The raw JSON reply from the bus
     * \param token Provides the caller's token that is answered
     * by this reply
     */
    void serviceResponseDelayed(const QString& method, const QString& payload, int token, const QJsonObject &jsonPayload) override;

    void hubError(const QString& method, const QString& error, const QString& payload, int token);

protected slots:
    void resetSubscription();

private:
    bool m_connected;
    LSMessageToken m_tokenServerStatus;
    QString m_applicationList;
    QVariant m_jsonApplicationList;
    QString m_launchPointsList;
    QVariant m_jsonLaunchPointsList;
    QString m_runningList;
    QHash<int, QString> m_launchCalls;
    QHash<int, QString> m_closeCalls;
};

#endif // APPLICATIONMANAGERSERVICE_H
