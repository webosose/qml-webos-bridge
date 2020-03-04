// Copyright (c) 2012-2020 LG Electronics, Inc.
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

#include "lunaservicemgr.h"

#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <QDebug>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include "LSUtils.h"

namespace {

static QHash<void *, QPointer<LunaServiceManagerListener> > s_callbackContext;

/**
* @brief Internal callback for service responses.
*
* @param  sh
* @param  reply
* @param  ctx
*
* @retval
*/

bool message_filter(LSHandle *sh, LSMessage *reply, void *ctx)
{
    Q_UNUSED(sh)

    QString payload(LSMessageGetPayload(reply));

    LSMessageToken token = LSMessageGetResponseToken(reply);
    LunaServiceManagerListener *listener = s_callbackContext.value(ctx).data();

    if (listener == NULL || !listener->callInfos.contains(token)) {
        qWarning("Service Manager callback context is invalid: %p or token is invalid: %lu", ctx, token);
        return false;
    }

    CallInfo call = listener->callInfos[token];

    if (LSMessageIsHubErrorMessage(reply))
        listener->hubError(call.method, QString(LSMessageGetMethod(reply)), payload, token);
    else
        listener->serviceResponse(call.method, QString(payload), token);

    return true;
}
} // namespace


QHash<QString, LunaServiceManager*> s_instances;

static const QLatin1String strSubscribe("subscribe");
static const QLatin1String strWatch("watch");

LunaServiceManager* LunaServiceManager::instance(const QString& appId, ClientType clientType, const QString& roleType)
{
    if (appId.isEmpty()) {
        // FIXME: For historical reason, we should allow an empty appId though it is not correct.
        //qWarning() << "Unable to get an instance of LunaServiceManager with empty appId";
        //return NULL;
        qWarning() << "Attempting to get an instance of LunaServiceManager with empty appId";
    }

    LunaServiceManager* instance = s_instances[appId];

    if (!instance) {
        instance = new LunaServiceManager();
        if (!instance) {
            qWarning() << "Out of memory when creating an instance of LunaServiceManager for appId:" << appId;
            return NULL;
        }
        instance->m_appId = appId;
        instance->m_clientType = clientType;
        instance->m_roleType = roleType;

        if (!instance->init()) {
            qWarning() << "Failed to initialize LunaServiceManager instance for appId:" << appId;
            delete instance;
            return NULL;
        }

        s_instances[appId] = instance;
        qInfo() << "LunaServiceManager instance created for appId:" << appId << clientType;
    }

    return instance;
}

LunaServiceManager::LunaServiceManager(QObject *parent) :
      QObject(parent)
    , busHandle(NULL)
{
    PmLogContext libcontext = NULL;

    if (kPmLogErr_None == PmLogGetContext("qml-webos-bridge", &libcontext)) {
        PmLogSetLibContext(libcontext);
    }
}

LunaServiceManager::~LunaServiceManager()
{
    uninit();
}

bool LunaServiceManager::init()
{
    bool ret = true;
    LSErrorSafe lserror;

    if (m_clientType == ApplicationClient)
        ret = LSRegisterApplicationService(QString(QLatin1String("%1-%2")).arg(m_appId).arg(QCoreApplication::applicationPid()).toUtf8().data(),
                                            m_appId.toLatin1().constData(), &busHandle, &lserror);
    else
        ret = LSRegister(m_appId.toLatin1().constData(), &busHandle, &lserror);

    if (!ret || !busHandle) {
        qWarning("Failed at LSRegister/LSRegisterApplicationService for %s, ERROR %d: %s (%s @ %s:%d)",
                m_appId.toLatin1().constData(),
                lserror.error_code, lserror.message,
                lserror.func, lserror.file, lserror.line);
        busHandle = NULL;
        return false;
    }

    ret = LSGmainContextAttach(busHandle, g_main_context_default(), &lserror);
    if (!ret) {
        qWarning("Failed at LSGmainContextAttach for %s, ERROR %d: %s (%s @ %s:%d)",
                m_appId.toLatin1().constData(),
                lserror.error_code, lserror.message,
                lserror.func, lserror.file, lserror.line);
        busHandle = NULL;
        return false;
    }

    ret = LSGmainSetPriority(busHandle, G_PRIORITY_HIGH, &lserror);
    if (!ret){
        qWarning("Failed at LSGmainSetPriority for %s, ERROR %d: %s (%s @ %s:%d)",
                m_appId.toLatin1().constData(),
                lserror.error_code, lserror.message,
                lserror.func, lserror.file, lserror.line);
        busHandle = NULL;
        return false;
    }

    qInfo() << "LSRegister done for appId:" << m_appId << m_clientType;

    return true;
}

void LunaServiceManager::uninit()
{
    if (busHandle) {
        LSErrorSafe lserror;

        if (!LSUnregister(busHandle, &lserror)) {
            qWarning("Failed at LSUnregister for %s, ERROR %d: %s (%s @ %s:%d)",
                    m_appId.toLatin1().constData(),
                    lserror.error_code, lserror.message,
                    lserror.func, lserror.file, lserror.line);
        }

        busHandle = NULL;
    }
}

LSHandle* LunaServiceManager::getServiceHandle()
{
    if (!busHandle)
        init();

    return busHandle;
}

LSMessageToken LunaServiceManager::call( const QString& service, const QString& method,
                                         const QString& payload, LunaServiceManagerListener* inListener,
                                         const QString& sessionId
                                       )
{
    qDebug() << "LunaServiceManager" << service << method << payload << inListener << sessionId;
    LSHandle *serviceHandle = getServiceHandle();

    if (!serviceHandle) {
        qWarning() << "Unable to invoke call for" << service << method << "due to invalid handle for appId" << m_appId;
        return LSMESSAGE_TOKEN_INVALID;
    }

    bool retVal;
    LSErrorSafe lserror;
    LSMessageToken token = LSMESSAGE_TOKEN_INVALID;
    CallInfo call = {QString(), false};

    if (m_appId.isEmpty())
        qWarning() << "Application ID hasn't been set.";

    void *key = inListener;
    LSFilterFunc callback = NULL;
    if (key) {
        s_callbackContext[key] = QPointer<LunaServiceManagerListener>(inListener);
        callback = message_filter;
    }

    QJsonObject obj = QJsonDocument::fromJson(payload.toUtf8()).object();
    if (obj[strSubscribe].toBool() || obj[strWatch].toBool()) {
        call.subscription = true;

        /* check m_appId for some serviceClient which want to use LSCallFromApplication function.
         * Note: it is possible to use custom appId with ApplicationClient also, but our Service
         * implementation doesn't allow to change appId after the registration, only set custom appId
         * for ServiceClient. */

        if (m_clientType == ApplicationClient || m_appId.isEmpty() || m_roleType == "regular") {
#ifdef USE_LUNA_SERVICE2_SESSION_API
            if (!sessionId.isEmpty())
                retVal = LSCallSession(serviceHandle, (service + method).toUtf8().data(), payload.toUtf8().data(),
                                       sessionId.toUtf8().data(), callback, key, &token, &lserror);
            else
#endif
                retVal = LSCall(serviceHandle, (service + method).toUtf8().data(), payload.toUtf8().data(),
                                callback, key, &token, &lserror);
        } else {
#ifdef USE_LUNA_SERVICE2_SESSION_API
            if (!sessionId.isEmpty())
                retVal = LSCallSessionFromApplication(serviceHandle, (service + method).toUtf8().data(),
                                                      payload.toUtf8().data(), sessionId.toUtf8().data(),
                                                      m_appId.toUtf8().data(), callback, key, &token, &lserror);
            else
#endif
                retVal = LSCallFromApplication(serviceHandle, (service + method).toUtf8().data(),
                                               payload.toUtf8().data(), m_appId.toUtf8().data(),
                                               callback, key, &token, &lserror);
        }
    } else {
        // check m_appId for some serviceClient which want to use LSCallFromApplication function
        if (m_clientType == ApplicationClient || m_appId.isEmpty() || m_roleType == "regular") {
#ifdef USE_LUNA_SERVICE2_SESSION_API
            if (!sessionId.isEmpty())
                retVal = LSCallSessionOneReply(serviceHandle, (service + method).toUtf8().data(),
                                               payload.toUtf8().data(), sessionId.toUtf8().data(),
                                               callback, key, &token, &lserror);
            else
#endif
                retVal = LSCallOneReply(serviceHandle, (service + method).toUtf8().data(),
                                        payload.toUtf8().data(), callback, key, &token, &lserror);
        } else {
#ifdef USE_LUNA_SERVICE2_SESSION_API
            if (!sessionId.isEmpty())
                retVal = LSCallSessionFromApplicationOneReply(serviceHandle, (service + method).toUtf8().data(),
                                                              payload.toUtf8().data(), sessionId.toUtf8().data(),
                                                              m_appId.toUtf8().data(),
                                                              callback, key, &token, &lserror);
            else
#endif
                retVal = LSCallFromApplicationOneReply(serviceHandle, (service + method).toUtf8().data(),
                                                       payload.toUtf8().data(), m_appId.toUtf8().data(),
                                                       callback, key, &token, &lserror);
        }
    }

    if (!retVal) {
        qWarning("LSCall %s%s failed for %s, ERROR %d: %s (%s @ %s:%d)",
                service.toLatin1().constData(),
                method.toLatin1().constData(),
                m_appId.toLatin1().constData(),
                lserror.error_code, lserror.message,
                lserror.func, lserror.file, lserror.line);

        return LSMESSAGE_TOKEN_INVALID;
    }

    if (inListener) {
        call.method = method;
        inListener->callInfos[token] = call;
    }

    return token;
}

void LunaServiceManager::cancelInternal(LSHandle *sh, LSMessageToken token)
{
    LSErrorSafe lserror;

    if (!LSCallCancel(sh, token, &lserror)) {
        qWarning("LSCallCancel for token %d, ERROR %d: %s (%s @ %s:%d)",
                token,
                lserror.error_code, lserror.message,
                lserror.func, lserror.file, lserror.line);
    }
}

void LunaServiceManager::cancel(LunaServiceManagerListener* inListener)
{
    if (!inListener)
        return;

    QMap<LSMessageToken, CallInfo> &callMap = inListener->callInfos;
    LSHandle *lshandle = getServiceHandle();
    if (lshandle) {
        for (auto i = callMap.begin();i != callMap.end();i = callMap.erase(i)) {
            LSMessageToken token = i.key();
            cancelInternal(lshandle, token);
        }
    }

    s_callbackContext.remove(inListener);
}

void LunaServiceManager::cancel(LunaServiceManagerListener* inListener, LSMessageToken token)
{
    if (!inListener)
        return;

    QMap<LSMessageToken, CallInfo> &callMap = inListener->callInfos;

    if (!callMap.contains(token))
        return;

    callMap.take(token);
    LSHandle *lshandle = getServiceHandle();
    if (lshandle)
        cancelInternal(lshandle, token);

    if (callMap.isEmpty())
        s_callbackContext.remove(inListener);
}

void LunaServiceManager::setTimeout(LSMessageToken token, int timeout)
{
    LSHandle *serviceHandle = getServiceHandle();

    if (!serviceHandle) {
        qWarning() << "Unable to set timeout for token" << token << "due to invalid handle for appId" << m_appId;
        return;
    }

    LSErrorSafe lserror;
    auto retVal = LSCallSetTimeout(serviceHandle, token, timeout, &lserror);
    if (!retVal) {
        qWarning("LSCallSetTimeout for token %d, ERROR %d: %s (%s @ %s:%d)",
                token,
                lserror.error_code, lserror.message,
                lserror.func, lserror.file, lserror.line);
    }
}
