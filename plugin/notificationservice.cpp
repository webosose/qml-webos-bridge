// Copyright (c) 2013-2020 LG Electronics, Inc.
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

#include "notificationservice.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

static const QLatin1String strMessage("message");
static const QLatin1String methodGetToastNotification("/getToastNotification");
static const QLatin1String methodGetAlertNotification("/getAlertNotification");
static const QLatin1String methodGetInputAlertNotification("/getInputAlertNotification");
static const QLatin1String methodGetPincodePromptNotification("/getPincodePromptNotification");
static const QLatin1String serviceName("com.webos.notification");

NotificationService::NotificationService(QObject * parent)
    : Service(parent)
    , m_tokenServerStatus(LSMESSAGE_TOKEN_INVALID)
    , m_tokenToastList(LSMESSAGE_TOKEN_INVALID)
    , m_tokenAlertList(LSMESSAGE_TOKEN_INVALID)
    , m_tokenInputAlertList(LSMESSAGE_TOKEN_INVALID)
    , m_tokenPincodePromptList(LSMESSAGE_TOKEN_INVALID)
    , m_toastRequested(false)
    , m_alertRequested(false)
    , m_inputAlertRequested(false)
    , m_pincodePromptRequested(false)
{
    connect(this, &Service::sessionIdChanged, this, &NotificationService::resetSubscription);
}

void NotificationService::setAppId(const QString& appId)
{
    Service::setAppId(appId);

    if (m_tokenServerStatus == LSMESSAGE_TOKEN_INVALID)
        m_tokenServerStatus = registerServerStatus(interfaceName(), true);
}

void NotificationService::cancel(LSMessageToken token)
{
    Service::cancel(token);

    // the subscription to registerServerStatus is also cancelled this case, restore it
    if (token == LSMESSAGE_TOKEN_INVALID || token == m_tokenServerStatus)
        m_tokenServerStatus = registerServerStatus(interfaceName(), true);
}

void NotificationService::initSubscriptionCalls()
{
    if (m_toastRequested) {
        if (m_tokenToastList != LSMESSAGE_TOKEN_INVALID)
            cancel(m_tokenToastList);
        m_tokenToastList = callWithRetry(serviceUri(),
              methodGetToastNotification, QString(QLatin1String("{\"%1\":%2}")).arg(strSubscribe).arg(strTrue));
    }

    if (m_alertRequested) {
        if (m_tokenAlertList != LSMESSAGE_TOKEN_INVALID)
            cancel(m_tokenAlertList);
        m_tokenAlertList = callWithRetry(serviceUri(),
              methodGetAlertNotification, QString(QLatin1String("{\"%1\":%2}")).arg(strSubscribe).arg(strTrue));
    }

    if (m_inputAlertRequested) {
        if (m_tokenInputAlertList != LSMESSAGE_TOKEN_INVALID)
            cancel(m_tokenInputAlertList);
        m_tokenInputAlertList = callWithRetry(serviceUri(),
              methodGetInputAlertNotification, QString(QLatin1String("{\"%1\":%2}")).arg(strSubscribe).arg(strTrue));
    }

    if (m_pincodePromptRequested) {
        if (m_tokenPincodePromptList != LSMESSAGE_TOKEN_INVALID)
            cancel(m_tokenPincodePromptList);
        m_tokenPincodePromptList = callWithRetry(serviceUri(),
              methodGetPincodePromptNotification, QString(QLatin1String("{\"%1\":%2}")).arg(strSubscribe).arg(strTrue));
    }
}

QString NotificationService::toastList()
{
    if (m_toastRequested == false) {
        m_toastRequested = true;
        initSubscriptionCalls();
    }

    return m_toastList;
}

QString NotificationService::alertList()
{
    if (m_alertRequested == false) {
        m_alertRequested = true;
        initSubscriptionCalls();
    }

    return m_alertList;
}

QString NotificationService::inputAlertList()
{
    if (m_inputAlertRequested == false) {
        m_inputAlertRequested = true;
        initSubscriptionCalls();
    }

    return m_inputAlertList;
}

QString NotificationService::pincodePromptList()
{
    if (m_pincodePromptRequested == false) {
        m_pincodePromptRequested = true;
        initSubscriptionCalls();
    }

    return m_pincodePromptList;
}

void NotificationService::serviceResponse(const QString& method, const QString& payload, int token)
{
    checkForErrors(payload, token);
    Q_EMIT response(method, payload, token);
    qDebug() << "Notification Service Response " << method << payload << token;
    QJsonObject rootObject = QJsonDocument::fromJson(payload.toUtf8()).object();

    if (token == m_tokenServerStatus && rootObject.value(strServiceName).toString() == interfaceName()) {
        bool connected = rootObject.value(strConnected).toBool();
        if (connected) {
            initSubscriptionCalls();
            return;
        }
    }
    bool subscribedValue = rootObject.value(strSubscribed).toBool();
    bool returnValue =  rootObject.value(strReturnValue).toBool() ||
                       !rootObject.value(strMessage).toString().isEmpty();

    if (subscribedValue || returnValue == false) {
        return; //Ignore the subscription confirmation response
    }

    if (token == m_tokenToastList && method == methodGetToastNotification) {
         if (payload == m_toastList) return;
         m_toastList = payload;
         Q_EMIT(toastListChanged());
    }
    else if (token == m_tokenAlertList && method == methodGetAlertNotification) {
        if (payload == m_alertList) return;
        m_alertList = payload;
        Q_EMIT(alertListChanged());
    }
    else if (token == m_tokenInputAlertList && method == methodGetInputAlertNotification) {
        if (payload == m_inputAlertList) return;
        m_inputAlertList = payload;
        Q_EMIT(inputAlertListChanged());
    }
    else if (token == m_tokenPincodePromptList && method == methodGetPincodePromptNotification) {
        if (payload == m_pincodePromptList) return;
        m_pincodePromptList = payload;
        Q_EMIT(pincodePromptListChanged());
    }
    else qWarning() << "Unknown method";
}

void NotificationService::hubError(const QString& method, const QString& error, const QString& payload, int token)
{
    checkForErrors(payload, token);

    if (error == LUNABUS_ERROR_SERVICE_DOWN) {
        qWarning() << "NotificationService: Hub error:" << error << "- recover subscriptions";
        resetSubscription();
    }
}

QString NotificationService::interfaceName() const
{
    return QString(serviceName);
}

void NotificationService::resetSubscription()
{
    qWarning() << __PRETTY_FUNCTION__;
    cancel();
}
