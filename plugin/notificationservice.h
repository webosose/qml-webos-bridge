// Copyright (c) 2013-2018 LG Electronics, Inc.
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

#ifndef NOTIFICATIONSERVICE_H
#define NOTIFICATIONSERVICE_H

#include "service.h"
#include <QUrl>
#include <QHash>

/*!
 * \class NotificationService
 * \brief Provides QML property bindings for the service
 * com.webos.notification
 *
 * This class provides
 * \li Properties and methods for the associated service.
 * \li The type of properties, parameters and return values
 * matches Qt types.
 *
 * \see Service
 */

class NotificationService : public Service
{
    Q_OBJECT

    Q_PROPERTY(QString toastList READ toastList NOTIFY toastListChanged)
    Q_PROPERTY(QString alertList READ alertList NOTIFY alertListChanged)
    Q_PROPERTY(QString inputAlertList READ inputAlertList NOTIFY inputAlertListChanged)
    Q_PROPERTY(QString pincodePromptList READ pincodePromptList NOTIFY pincodePromptListChanged)

Q_SIGNALS:
    void toastListChanged();
    void alertListChanged();
    void inputAlertListChanged();
    void pincodePromptListChanged();

public:
    NotificationService(QObject * parent = 0);

    void setAppId(const QString& appId);
    void initSubscriptionCalls();

    QString toastList();
    QString alertList();
    QString inputAlertList();
    QString pincodePromptList();

    QString interfaceName() const;

    Q_INVOKABLE void cancel(LSMessageToken token = LSMESSAGE_TOKEN_INVALID);

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
    void serviceResponse(const QString& method, const QString& payload, int token);

    void hubError(const QString& method, const QString& error, const QString& payload, int token);

private:
    LSMessageToken m_tokenServerStatus;
    LSMessageToken m_tokenToastList;
    LSMessageToken m_tokenAlertList;
    LSMessageToken m_tokenInputAlertList;
    LSMessageToken m_tokenPincodePromptList;
    QString m_toastList;
    QString m_alertList;
    QString m_inputAlertList;
    QString m_pincodePromptList;

    bool m_toastRequested;
    bool m_alertRequested;
    bool m_inputAlertRequested;
    bool m_pincodePromptRequested;
};

#endif // NOTIFICATIONSERVICE_H
