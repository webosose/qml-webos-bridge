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

#include "service.h"

#include <stdio.h>
#include <stdlib.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QJsonObject>
#include <QSemaphore>

#include "lunaservicemgr.h"
#include "LSUtils.h"

class MessageSpreader: public QThread
{
public:
    static MessageSpreader *instance();
    MessageSpreader();
    ~MessageSpreader();
    void removeListener(MessageSpreaderListener *listener);
    void pushMessageResponse(MessageSpreaderListener *listener, const QString& method, const QString& payload, int token);
    void messageResponded(MessageSpreaderListener *listener);

    struct Response
    {
        QString method;
        QString payload;
        int token = 0;
        MessageSpreaderListener *listener = nullptr;
        size_t listenerHandle = 0;
    };

private:
    void run() override;
    void messageRespondedInternal(MessageSpreaderListener *listener);
    QMutex m_mutex;
    QSet<size_t> m_listeners;
    QQueue<Response> m_responses;
    QSemaphore m_semaphore;
    int m_postSleepMs = 0;
};

const QLatin1String Service::strURIScheme("luna://");
const QLatin1String Service::strURISchemeDeprecated("palm://");
const QLatin1String Service::strReturnValue("returnValue");
const QLatin1String Service::strSubscribe("subscribe");
const QLatin1String Service::strSubscribed("subscribed");
const QLatin1String Service::strErrorCode("errorCode");
const QLatin1String Service::strErrorText("errorText");
const QLatin1String Service::strErrorCodeJsonParse("-1000");
const QLatin1String Service::strErrorTextJsonParse("Json parse error");
const QLatin1String Service::strErrorCodeInvalidType("-1001");
const QLatin1String Service::strErrorTextInvalidType("Invalid parameter type");
const QLatin1String Service::strErrorMsg("errorMsg");
const QLatin1String Service::strServiceName("serviceName");
const QLatin1String Service::strConnected("connected");
const QLatin1String Service::strTrue("true");
const QLatin1String Service::strFalse("false");
const QLatin1String Service::strSessionId("sessionId");

Service::Service(QObject * parent)
    : LunaServiceManagerListener(parent)
    , m_serviceManager(0)
    , m_clientType(ServiceClient)
{
    const QByteArray &roleType = qgetenv("ROLE_TYPE");
    if (!roleType.isEmpty()) {
        Service::setRoleType(roleType);
    }

    const QByteArray &appId = qgetenv("APP_ID");
    if (appId.isEmpty()) {
        m_clientType = ServiceClient;
    } else {
        m_clientType = ApplicationClient;
        Service::setAppId(appId);
    }
    m_category = "/";
}

Service::~Service()
{
    cancel();
}

void Service::setAppId(const QString& appId)
{
    if (appId.isEmpty()) {
        qWarning() << "attempt to set null appId";
        return;
    }
    if (m_appId.isEmpty()) {
        m_appId = appId;
        m_serviceManager = LunaServiceManager::instance(appId, m_clientType, m_roleType);
        emit appIdChanged();
    } else if (m_appId != appId) {
        qWarning() << "attempt to change appId from" << m_appId << "to" << appId;
    }
}

void Service::setRoleType(const QString& roleType)
{
    if (roleType.isEmpty()) {
        qWarning() << "attempt to set null roleType";
        return;
    }
    if (m_roleType.isEmpty()) {
        if (roleType == "regular" || roleType == "privileged") {
            qDebug() << "Set roleType to " << roleType;
            m_roleType = roleType;
        }
    }
}

int Service::call(const QString& service, const QString& method, const QString& payload, const QJSValue& timeout, const QString& sessionId)
{
    QString effectiveSessionId(sessionId.isEmpty() ? m_sessionId: sessionId);
    if (effectiveSessionId == "no-session")
        effectiveSessionId = "";
    return callInternal(service, method, payload, timeout, effectiveSessionId);
}

int Service::callInternal(const QString& service, const QString& method, const QString & payload, const QJSValue& timeout, const QString& sessionId)
{
    if (QGuiApplication::arguments().contains(QStringLiteral("criu_enable")) &&
        m_appId.isEmpty()) {
        qWarning() << "Disallow to register service status for empty appId on criu_enable";
        return LSMESSAGE_TOKEN_INVALID;
    }

    if (!m_serviceManager)
        m_serviceManager = LunaServiceManager::instance(m_appId);

    if (!m_serviceManager) return 0;

    auto token = m_serviceManager->call(service,
                                   method,
                                   payload,
                                   this,
                                   sessionId);

    if (token != LSMESSAGE_TOKEN_INVALID) {
        if (timeout.isNumber()) {
            m_serviceManager->setTimeout(token, timeout.toUInt());
        } else {
            bool isUndefined = timeout.isUndefined();
            if (!isUndefined)
                qWarning("Only integers are accepted to timeout parameter of Service::call.");
        }
    }

    return token;
}

int Service::callService(const QVariantMap& payload)
{
    return call(m_callServiceName, m_callServiceMethod, QJsonDocument::fromVariant(payload).toJson(QJsonDocument::Compact));
}

int Service::callWithRetry(const QString& service, const QString& method, const QString & payload, int retry)
{
    int i, token = LSMESSAGE_TOKEN_INVALID;

    for (i = 0; i < retry; i++) {
        token = call(service, method, payload);
        if (token != LSMESSAGE_TOKEN_INVALID)
            return token;

        qWarning() << "Call failed" << service << method << payload << "- retry in next event loop" << i;
        // Process events in between retries for some reasons:
        // 1) LS2 to process events to recover broken connections
        // 2) GUI not to be blocked due to this
        QCoreApplication::processEvents();
    }

    qWarning() << "Call failed finally" << service << method << "retry" << retry;

    return token;
}

void Service::cancel(LSMessageToken token)
{
    if (!m_serviceManager) return;

    if (token == LSMESSAGE_TOKEN_INVALID)
        m_serviceManager->cancel(this);
    else
        m_serviceManager->cancel(this, token);

    Q_EMIT cancelled(token);
}

void Service::serviceResponse(const QString& method, const QString& payload, int token)
{
    checkForErrors(payload, token);
    Q_EMIT response(method, payload, token);

    // NOTE:
    // It seems like a bug in Qt 5.9 where accessing "returnValue" key in obj
    // results in the key gets defined as "null". Due to this we have to keep
    // the original object untouched and use it when emitting signals.
    QJsonObject obj = QJsonDocument::fromJson(payload.toUtf8()).object();
    QVariantMap vmap = obj.toVariantMap();
    if (obj[strReturnValue].toBool())
        Q_EMIT callSuccess(vmap);
    else
        Q_EMIT callFailure(vmap);
    Q_EMIT callResponse(vmap);
}

void Service::hubError(const QString& method, const QString& error, const QString& payload, int token)
{
    qWarning() << "Hub error detected for token:" << token << method << error;

    checkForErrors(payload, token);
}

void Service::checkForErrors(const QJsonObject& rootObject, int token)
{
    int errorCode = 0;
    QString errorText;

    //by API convention errorCode is missing instead being set to zero
    if (!rootObject.contains(strErrorCode)) {
        Q_EMIT success(token);
        return;
    }

    errorCode = rootObject.find(strErrorCode).value().toInt();
    errorText = rootObject.find(strErrorText).value().toString();

    qWarning() << "Error response for token:" << token << errorCode << errorText;

    Q_EMIT error(errorCode, errorText, token);
}

void Service::checkForErrors(const QString& payload, int token)
{
    checkForErrors(QJsonDocument::fromJson(payload.toUtf8()).object(), token);
}

QString Service::interfaceName() const
{
    return QString();
}

QString Service::serviceUri() const
{
    return strURIScheme + interfaceName();
}

int Service::registerServerStatus(const QString &serviceName, bool useSession)
{
    QJsonObject obj;
    obj.insert(strServiceName, serviceName);
    obj.insert(strSubscribe, true);
    if (useSession && !m_sessionId.isEmpty())
        obj.insert(strSessionId, m_sessionId);
    QString params = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    int token = callInternal(QLatin1String("luna://com.webos.service.bus"),
                             QLatin1String("/signal/registerServerStatus"),
                             params, QJSValue(), QString());
    if (token == LSMESSAGE_TOKEN_INVALID)
        qWarning() << "registerServerStatus failed, serviceName:" << serviceName << "appId:" << m_appId << "sessionId:" << m_sessionId << "useSession:" << useSession;
    else
        qInfo() << "registerServerStatus for serviceName:" << serviceName << "appId:" << m_appId << "token:" << token << "sessionId:" << m_sessionId << "useSession:" << useSession;

    return token;
}

void Service::setPublicMethods(QStringList methods)
{
    qWarning() << "The property publicMethods is deprecated. Use property methods.";
    m_methods += methods;
    registerMethods(methods);
    emit publicMethodsChanged();
}

void Service::setPrivateMethods(QStringList methods)
{
    qWarning() << "The property privateMethods is deprecated. Use property methods.";
    m_methods += methods;
    registerMethods(methods);
    emit privateMethodsChanged();
}

void Service::setMethods(QStringList methods)
{
    m_methods += methods;
    registerMethods(methods);
    emit methodsChanged();
}

bool Service::callback(LSHandle *lshandle, LSMessage *msg, void *user_data)
{
    Service* s = static_cast<Service *>(user_data);

    if (s == NULL) {
        qWarning("Service callback context is invalid %p", user_data);
        return false;
    }

    QString method(LSMessageGetMethod(msg));
    QString payload(LSMessageGetPayload(msg));
#ifdef USE_LUNA_SERVICE2_SESSION_API
    QString sessionId(LSMessageGetSessionId(msg));
#endif
    bool success = false;

    QJsonObject returnObject;
    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(payload.toUtf8(), &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        returnObject.insert(strErrorCode, strErrorCodeJsonParse);
        returnObject.insert(strErrorText, strErrorTextJsonParse);
        returnObject.insert(strReturnValue, success);
        LSErrorSafe lsError;
        success = LSMessageReply(lshandle, msg, QJsonDocument(returnObject).toJson().data(), &lsError);
        return success;
    }

    QJsonObject message = jsonDoc.object();
#ifdef USE_LUNA_SERVICE2_SESSION_API
    if (!sessionId.isEmpty())
        message.insert(strSessionId, sessionId);
#endif

    if (message.contains(strSubscribe) && !message.value(strSubscribe).isBool()) {
        returnObject.insert(strErrorCode, strErrorCodeInvalidType);
        returnObject.insert(strErrorText, strErrorTextInvalidType);
        returnObject.insert(strReturnValue, success);
        LSErrorSafe lsError;
        success = LSMessageReply(lshandle, msg, QJsonDocument(returnObject).toJson().data(), &lsError);
        return success;
    }

    QVariant returnedValue;
    bool retVal = QMetaObject::invokeMethod(const_cast<Service*>(s), method.toUtf8().constData(),
                                            Q_RETURN_ARG(QVariant, returnedValue),
                                            Q_ARG(QVariant, QVariant::fromValue(message)));

    QJsonObject retObj = QJsonDocument::fromJson(returnedValue.toString().toUtf8()).object();
    if (!retObj.contains(strErrorCode)) {
        success = true;
    } else {
        success = false;
    }

    bool subscribed = false;
    if (!success) {
        returnObject.insert(strErrorCode, retObj[strErrorCode]);
        returnObject.insert(strErrorText, retObj[strErrorText]);
        //TODO: This should be removed after checking that there is no component which relies on this parameter
        if (!retObj[strErrorMsg].isNull())
            returnObject.insert(strErrorMsg, retObj[strErrorMsg]);
    } else {
        QStringList keys = retObj.keys();
        QString key;
        for(int ind = 0; ind < keys.length(); ind++) {
            key = keys.at(ind);
            returnObject.insert(key, retObj[key]);
        }
        LSErrorSafe lserror;
        if (LSMessageIsSubscription(msg)) {
            subscribed = LSSubscriptionAdd(lshandle, method.toUtf8().constData(), msg, &lserror);
            returnObject.insert(strSubscribed, subscribed);
            if (subscribed)
                LSSubscriptionSetCancelFunction(lshandle, &Service::callbackSubscriptionCancel, (void*)s, &lserror);
        }
    }
    returnObject.insert(strReturnValue, success);
    QJsonDocument doc(returnObject);

    LSErrorSafe lsError;
    success = LSMessageReply(lshandle, msg, doc.toJson().data(), &lsError);
    return retVal;
}

bool Service::callbackSubscriptionCancel(LSHandle *lshandle, LSMessage *msg, void *user_data)
{
    Service* s = static_cast<Service *>(user_data);

    if (s == NULL) {
        qWarning("Subscription cancel callback context is invalid %p", user_data);
        return false;
    }

    QString method(LSMessageGetMethod(msg));

    Q_EMIT s->subscriptionAboutToCancel(method);

    return true;
}

void Service::pushSubscription(const QString& method, const QString& param, const QString& responseMethod)
{
    if (!m_serviceManager)
        m_serviceManager = LunaServiceManager::instance(m_appId);

    if (!m_serviceManager) {
        qWarning() << "appId is undefined, ignore this pushSubscription: method=" << method
                   << ", payload=" << param;
        return;
    }

    LSHandle *serviceHandle = m_serviceManager->getServiceHandle();

    if (!serviceHandle) {
        qWarning() << "Failed at pushSubscription for method" << method << "due to invalid handle";
        return;
    }

    if (!m_methods.contains(method)) {
        qWarning() << "No method " << method << "for service" << appId();
        return;
    }

    // Use method if responseMethod is not set
    QString member = responseMethod.isEmpty() ? method : responseMethod;
    QJsonObject arg = QJsonDocument::fromJson(param.length() == 0 ? "{}" : param.toUtf8()).object();
    QVariant returnedValue;

    if (QMetaObject::invokeMethod(this,
                member.toUtf8().constData(),
                Q_RETURN_ARG(QVariant, returnedValue),
                Q_ARG(QVariant, QVariant::fromValue(arg)))) {
        QJsonObject retObj = QJsonDocument::fromJson(returnedValue.toString().toUtf8()).object();
        QJsonObject returnObject;
        if (!retObj.contains(strErrorCode)) {
            QStringList keys = retObj.keys();
            QString key;
            for(int ind = 0; ind < keys.length(); ind++) {
                key = keys.at(ind);
                returnObject.insert(key, retObj[key]);
            }
            returnObject.insert(strReturnValue, true);
            LSErrorSafe lserror;
            QJsonDocument doc(returnObject);
            LSSubscriptionReply(serviceHandle, method.toUtf8().constData(), doc.toJson().data(), &lserror);
        } else {
            qWarning() << "Nothing to push for method " << method << "for service" << appId();
        }
    } else {
        qWarning() << "Failed to invoke response method " << responseMethod << "for service" << appId();
    }
}

unsigned int Service::subscribersCount(const QString& method)
{
    LSHandle *serviceHandle = m_serviceManager->getServiceHandle();

    if (!serviceHandle) {
        qWarning() << "Failed at subscribersCount for method" << method << "due to invalid handle";
        return 0;
    }

    return LSSubscriptionGetHandleSubscribersCount(m_serviceManager->getServiceHandle(), method.toUtf8().constData());
}

void Service::registerMethods(const QStringList &methods)
{
    if (!m_serviceManager)
        m_serviceManager = LunaServiceManager::instance(m_appId);

    if (!m_serviceManager) {
        qWarning() << "appId is undefined, ignore this registerMethods:"
                   << "methods=" << methods.join(",");
        return;
    }

    if (m_serviceManager->getClientType() == ApplicationClient)
    {
        qWarning() << "ApplicationClient can't register methods";
        return;
    }

    LSHandle *serviceHandle = m_serviceManager->getServiceHandle();

    if (!serviceHandle) {
        qWarning() << "Failed at registerMethods due to invalid handle";
        return;
    }

    for (const auto &method : methods) {
        auto methodName = method.toUtf8();

        LSMethod methodMap[] = {
            {methodName.constData(), &Service::callback, LUNA_METHOD_FLAGS_NONE},
            {nullptr, nullptr, LUNA_METHOD_FLAGS_NONE}
        };

        LSErrorSafe lsError;
        if (!LSRegisterCategoryAppend(serviceHandle, m_category.toUtf8().data(),
                                      methodMap, NULL, &lsError)) {
            qWarning() << "LS2 error in registering methods" << lsError.message;
        }
    }

    LSErrorSafe lsError;
    if (!LSCategorySetData(serviceHandle, m_category.toUtf8().data(), this, &lsError)) {
        qWarning() << "LS2 error in setting category data" << lsError.message;
    }
}

void Service::setCategory(const QString& category)
{
    Q_ASSERT(m_category.length() == 0);
    m_category = category;
    emit categoryChanged();
}

void Service::setSessionId(const QString& sessionId)
{
    if (m_sessionId != sessionId) {
        m_sessionId = sessionId;
        emit sessionIdChanged();
    }
}

void Service::setCallServiceName(QString& newServiceName) {
    if (!newServiceName.startsWith(strURISchemeDeprecated) && !newServiceName.startsWith(strURIScheme)) {
        newServiceName.prepend(strURIScheme);
    }
    if (!newServiceName.endsWith(QChar('/'))) {
        newServiceName.append(QChar('/'));
    }
    m_callServiceName = newServiceName;
}

static QSharedPointer<MessageSpreader> s_messageSpreader;

MessageSpreader *MessageSpreader::instance()
{
    if (!s_messageSpreader.get())
        s_messageSpreader.reset(new MessageSpreader());
    return s_messageSpreader.get();
}

MessageSpreader::MessageSpreader()
{
    m_postSleepMs = qgetenv("WEBOS_QML_WEBOSSERVICES_SPREAD_EVENTS_WAIT_AFTER_RESPONSE").toInt();
}

MessageSpreader::~MessageSpreader()
{
    QThread::wait();
}

void MessageSpreader::removeListener(MessageSpreaderListener *listener)
{
    QMutexLocker locker(&m_mutex);
    m_listeners.remove(listener->m_handle);
    messageRespondedInternal(listener);
}

void MessageSpreader::messageResponded(MessageSpreaderListener *listener)
{
    QMutexLocker locker(&m_mutex);
    messageRespondedInternal(listener);
}

void MessageSpreader::messageRespondedInternal(MessageSpreaderListener *listener)
{
    if (listener->m_emitted) {
        listener->m_emitted = false;
        m_semaphore.release();
    }
}

void MessageSpreader::pushMessageResponse(MessageSpreaderListener *listener, const QString& method, const QString& payload, int token)
{
    Response response;
    response.method = method;
    response.listenerHandle = listener->m_handle;
    response.payload = payload;
    response.token = token;
    response.listener = listener;

    QMutexLocker locker(&m_mutex);
    m_responses.enqueue(response);
    m_listeners.insert(response.listenerHandle);

    QThread::start();
}

void MessageSpreader::run()
{
    Response response;
    while (true) {
        bool emitted = false;
        {
            QMutexLocker locker(&m_mutex);
            if (m_responses.empty())
                return;
            response = m_responses.dequeue();
            if (m_listeners.contains(response.listenerHandle)) {
                response.listener->m_emitted = emitted = true;
                const QJsonObject &jsonPayload = QJsonDocument::fromJson(response.payload.toUtf8()).object();
                emit response.listener->serviceResponseSignal(response.method, response.payload, response.token, jsonPayload);
            }
        }
        if (emitted) {
            m_semaphore.acquire();
            QThread::msleep(m_postSleepMs);
        }
    }
}

MessageSpreaderListener::MessageSpreaderListener(QObject *parent)
    :Service(parent)
{
    static size_t s_handle = 0;
    m_handle = ++s_handle;

    connect(this, &MessageSpreaderListener::serviceResponseSignal,
            this, &MessageSpreaderListener::serviceResponseSlot);
}

MessageSpreaderListener::~MessageSpreaderListener()
{
    MessageSpreader::instance()->removeListener(this);
}

void MessageSpreaderListener::serviceResponse(const QString& method, const QString& payload, int token)
{
    if (m_spreadEvents) {
        // TODO: Consider spread response automatically when service reply with heavy payload(payload.size() is greater than some threshold)
        MessageSpreader::instance()->pushMessageResponse(this, method, payload, token);
    } else {
        const QJsonObject &jsonPayload = QJsonDocument::fromJson(payload.toUtf8()).object();
        serviceResponseDelayed(method, payload, token, jsonPayload);
    }
}

void MessageSpreaderListener::serviceResponseSlot(const QString& method, const QString& payload, int token, const QJsonObject &jsonPayload)
{
    serviceResponseDelayed(method, payload, token, jsonPayload);
    MessageSpreader::instance()->messageResponded(this);
}
