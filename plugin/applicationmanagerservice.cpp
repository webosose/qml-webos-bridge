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

#include "applicationmanagerservice.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

static const QLatin1String strLeftBrace("{");
static const QLatin1String strRightBrace("}");
static const QLatin1String strSectionSeparator(" -");
static const QLatin1String strAppId("appId");
static const QLatin1String strTitle("title");
static const QLatin1String strNoSplash("noSplash");
static const QLatin1String strSplashBackGround("splashBackground");
static const QLatin1String strStatus("status");
static const QLatin1String strProcessId("processId");
static const QLatin1String strExtraInfo("extraInfo");
static const QLatin1String strEvent("event");
static const QLatin1String strShowSpinner("showSpinner");
static const QLatin1String strShowSplash("showSplash");
static const QLatin1String methodLaunch("/launch");
static const QLatin1String methodClose("/close");
static const QLatin1String methodRemoveLaunchPoint("/removeLaunchPoint");
static const QLatin1String methodMoveLaunchPoint("/moveLaunchPoint");
static const QLatin1String methodListLaunchPoints("/listLaunchPoints");
static const QLatin1String methodListApps("/listApps");
static const QLatin1String methodRunning("/running");
static const QLatin1String methodOnLaunch("/onLaunch");
static const QLatin1String methodGetAppLifeStatus("/getAppLifeStatus");
static const QLatin1String methodGetAppLifeEvents("/getAppLifeEvents");
static const QLatin1String serviceName("com.webos.applicationManager");

ApplicationManagerService::ApplicationManagerService(QObject * parent)
    : MessageSpreaderListener(parent)
    , m_connected(false)
    , m_tokenServerStatus(LSMESSAGE_TOKEN_INVALID)
{
    connect(this, &Service::sessionIdChanged, this, &ApplicationManagerService::resetSubscription);
    m_spreadEvents = qgetenv("WEBOS_QML_WEBOSSERVICES_SPREAD_EVENTS").split(',').contains("ApplicationManagerService");
}

void ApplicationManagerService::setAppId(const QString& appId)
{
    Service::setAppId(appId);

    if (m_tokenServerStatus == LSMESSAGE_TOKEN_INVALID)
        m_tokenServerStatus = registerServerStatus(interfaceName(), true);
}

void ApplicationManagerService::cancel(LSMessageToken token)
{
    Service::cancel(token);

    // the subscription to registerServerStatus is also cancelled this case, restore it
    if (token == LSMESSAGE_TOKEN_INVALID || token == m_tokenServerStatus)
        m_tokenServerStatus = registerServerStatus(interfaceName(), true);
}

int ApplicationManagerService::launch(const QString& identifier, const QString& params, const bool checkUpdateOnLaunch, const bool autoInstallation, const QString& reason)
{
    int token = 0;

    if (!QFile::exists(identifier.section(strSectionSeparator, 0, 0))) {
        QString paramsJSON = params.simplified();
        if (!paramsJSON.startsWith(strLeftBrace) && !paramsJSON.endsWith(strRightBrace)) {
            paramsJSON.prepend(strLeftBrace);
            paramsJSON.append(strRightBrace);
        }

        QString methodParams = "{\"id\":\"" + identifier
            + "\",\"params\":" + paramsJSON
            + ",\"checkUpdateOnLaunch\":" + (checkUpdateOnLaunch ? "true" : "false")
            + ",\"autoInstallation\":" + (autoInstallation ? "true" : "false")
            + ",\"reason\":\"" + reason + "\"}";

        token = call(serviceUri(),
              methodLaunch, methodParams);
        m_launchCalls[token] = identifier;
    }
    // Let's keep this in for demo purposes for now:
    else {
        QProcess * myProcess = new QProcess(this);
        myProcess->start(identifier);
    }
    return token;
}

int ApplicationManagerService::removeLaunchPoint(const QString& identifier)
{
    return call(serviceUri(),
            methodRemoveLaunchPoint,
            QString(QLatin1String("{\"launchPointId\":\"%1\"}")).arg(identifier));
}

int ApplicationManagerService::close(const QString& processId)
{
    int token = 0;
    token = call(serviceUri(),
          methodClose,
          QString(QLatin1String("{\"processId\":\"%1\"}")).arg(processId));
    m_closeCalls[token] = processId;
    return token;
}

int ApplicationManagerService::moveLaunchPoint(int index, int to)
{
    return call(serviceUri(),
          methodMoveLaunchPoint,
          QString(QLatin1String("{ \"index\": %1, \"to\": %2 }")).arg(index).arg(to));
}

QString ApplicationManagerService::runningList()
{
    call(serviceUri(),
          methodRunning,
          QString(QLatin1String("{\"%1\":%2}")).arg(strSubscribe).arg(strTrue));

    return m_runningList;
}

int ApplicationManagerService::subscribeLaunchedAppId()
{
    return callWithRetry(serviceUri(),
            methodOnLaunch,
            QString(QLatin1String("{\"%1\":%2}")).arg(strSubscribe).arg(strTrue));
}

int ApplicationManagerService::subscribeAppLifeStatus()
{
    return callWithRetry(serviceUri(),
            methodGetAppLifeStatus,
            QString(QLatin1String("{\"%1\":%2}")).arg(strSubscribe).arg(strTrue));
}

int ApplicationManagerService::subscribeAppLifeEvents()
{
    return callWithRetry(serviceUri(),
            methodGetAppLifeEvents,
            QString(QLatin1String("{\"%1\":%2}")).arg(strSubscribe).arg(strTrue));
}

int ApplicationManagerService::subscribeApplicationList()
{
    return callWithRetry(serviceUri(),
          methodListApps,
          QString(QLatin1String("{\"%1\":%2}")).arg(strSubscribe).arg(strTrue));
}

int ApplicationManagerService::subscribeLaunchPointsList()
{
    return callWithRetry(serviceUri(),
          methodListLaunchPoints,
          QString(QLatin1String("{\"%1\":%2}")).arg(strSubscribe).arg(strTrue));
}

void ApplicationManagerService::serviceResponseDelayed(const QString& method, const QString& payload, int token, const QJsonObject &rootObject)
{
    checkForErrors(rootObject, token);
    Q_EMIT response(method, payload, token);

    if (token == m_tokenServerStatus && rootObject.value(strServiceName).toString() == interfaceName()) {
        bool connected = rootObject.value(strConnected).toBool();
        if (m_connected != connected) {
            m_connected = connected;
            Q_EMIT connectedChanged();
        }
    }
    else if (method == methodListApps) {
        if (payload == m_applicationList) return;
        m_applicationList = payload;
        m_jsonApplicationList = QVariant(rootObject);
        Q_EMIT(applicationListChanged());
        Q_EMIT(jsonApplicationListChanged());
    }
    else if (method == methodListLaunchPoints) {
        if ( payload == m_launchPointsList ) {
            Q_EMIT(sameLaunchPointsListPublished());
            return;
        }
        m_launchPointsList = payload;
        m_jsonLaunchPointsList = QVariant(rootObject);
        Q_EMIT(launchPointsListChanged());
        Q_EMIT(jsonLaunchPointsListChanged());
    }
    else if (method == methodRunning) {
        if (payload == m_runningList) return;
        m_runningList = payload;
        Q_EMIT(runningListChanged());
    }
    else if (method == methodLaunch) {
        bool returnValue = rootObject.value(strReturnValue).toBool();
        QString identifier = m_launchCalls.value(token);
        if (returnValue == true) {
            Q_EMIT(launched(identifier, token));
        } else {
            int errorCode = rootObject.value(strErrorCode).toInt();
            Q_EMIT(launchFailed(identifier, token, errorCode));
        }
    }
    else if (method == methodClose) {
        bool returnValue = rootObject.value(strReturnValue).toBool();
        QString processId = m_closeCalls.value(token);
        if (returnValue == true) {
            Q_EMIT(closed(processId, token));
        }
    }
    else if (method == methodOnLaunch) {
        QString appId = rootObject.value(strAppId).toString();
        if (!appId.isEmpty()) {
            QString title = rootObject.value(strTitle).toString();
            bool noSplash = rootObject.value(strNoSplash).toBool();
            QString splashBackground = rootObject.value(strSplashBackGround).toString();
            Q_EMIT(appLaunched(appId, title, noSplash, splashBackground));
        }
    }
    else if (method == methodGetAppLifeStatus) {
        QString appId = rootObject.value(strAppId).toString();
        if (!appId.isEmpty()) {
            QString status = rootObject.value(strStatus).toString();
            QString processId = rootObject.value(strProcessId).toString();
            QString extraInfo;
            QJsonObject extraInfoObject = rootObject.value(strExtraInfo).toObject();
            if (!extraInfoObject.isEmpty()) {
                QJsonDocument doc(extraInfoObject);
                extraInfo = doc.toJson(QJsonDocument::Compact);
            }
            Q_EMIT(appLifeStatusChanged(appId, status, processId, extraInfo));
        }
    }
    else if (method == methodGetAppLifeEvents) {
        QString appId = rootObject.value(strAppId).toString();
        if (!appId.isEmpty()) {
            QString event = rootObject.value(strEvent).toString();
            QString title = rootObject.value(strTitle).toString();
            bool showSpinner = rootObject.value(strShowSpinner).toBool();
            bool showSplash = rootObject.value(strShowSplash).toBool();
            QString splashBackground = rootObject.value(strSplashBackGround).toString();
            Q_EMIT(appLifeEventsChanged(appId, event, title, showSpinner, showSplash, splashBackground));
        }
    }
    else qWarning() << "ApplicationManagerService: Unknown method:"<<method;
}

void ApplicationManagerService::hubError(const QString& method, const QString& error, const QString& payload, int token)
{
    checkForErrors(payload, token);

    if (error == LUNABUS_ERROR_SERVICE_DOWN) {
        qWarning() << "ApplicationManagerService: Hub error:" << error << "- recover subscriptions";
        resetSubscription();
    }
}

QString ApplicationManagerService::interfaceName() const
{
    return QString(serviceName);
}

void ApplicationManagerService::resetSubscription()
{
    qWarning() << __PRETTY_FUNCTION__;
    if (m_connected) {
        m_connected = false;
        Q_EMIT connectedChanged();
    }
    cancel();
}
