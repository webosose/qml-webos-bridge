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

#ifndef SERVICE_H
#define SERVICE_H

#include <QObject>
#include <QStringList>
#include <QDebug>
#include <QHash>
#include <QJSValue>
#include <QPointer>

#include "lunaservicemgr.h"

/*!
 * \class Service
 *
 * \brief QML Service element for the Universal API and
 *        base class for the service elements provided
 *        by the Simple API.
 *
 *    Service {
 *        id: myService
 *        appId: myAppId
 *        methods: ["myMethod"]
 *        sessionId: "ab5f918c-8260-45c8-acdc-d056d24866a0"
 *
 *        function myMethod(msg) {
 *            // The msg is a JSON object that contains all the data
 *            // that the caller provided
 *        }
 *    }
 *
 */

class Service : public LunaServiceManagerListener
{
    Q_OBJECT
    Q_DISABLE_COPY(Service)

    Q_PROPERTY(QString appId WRITE setAppId READ appId NOTIFY appIdChanged)
    Q_PROPERTY(QString category MEMBER m_category WRITE setCategory NOTIFY categoryChanged)
    Q_PROPERTY(QString sessionId MEMBER m_sessionId WRITE setSessionId NOTIFY sessionIdChanged)
    Q_PROPERTY(QStringList publicMethods MEMBER m_methods WRITE setPublicMethods NOTIFY publicMethodsChanged)
    Q_PROPERTY(QStringList privateMethods MEMBER m_methods WRITE setPrivateMethods NOTIFY privateMethodsChanged)
    Q_PROPERTY(QStringList methods MEMBER m_methods WRITE setMethods NOTIFY methodsChanged)

    /*!
     * \brief set to name of service to call using callService - "com.service",
     * "luna://com.service", or "luna://com.service/category" are all valid
     */
    Q_PROPERTY(QString service WRITE setCallServiceName READ callServiceName NOTIFY callServiceChanged)

    /*!
     * \brief set to method of service to call (ie, "launch", "open")
     */
    Q_PROPERTY(QString method WRITE setCallMethodName READ callMethodName NOTIFY callMethodChanged)

public:
    Service (QObject * parent = 0);
    virtual ~Service();

    void setCallServiceName(QString& newServiceName);
    QString callServiceName() { return m_callServiceName; }
    void setCallMethodName(QString& newMethodName) { m_callServiceMethod = newMethodName; }
    QString callMethodName() { return m_callServiceMethod; }

    /*!
     * \brief The most basic service request associated with a certain
     * interface name.
     * \param service The interface name of the service
              (e.g. "com.palm.systemservice")
     * \param method The method to be executed (e.g. "/getPreferences")
     * \param payload The method's parameters
     *        (e.g. "{\"keys\":[\"airplaneMode\"], \"subscribe\":true}")
     * \param timeout Allows to set timeout for the call.
     *        QT signal response (this class) with CancelMethodCall message would be called by timeout.
     * \param sessionId The session id of destination (override m_sessionId if not empty)
     *        (e.g. "ab5f918c-8260-45c8-acdc-d056d24866a0")
     * \return The token number that got assigned to this query call.
     *         In the event of a malfunction "0" is returned.
     */
    Q_INVOKABLE int call( const QString& service,
                          const QString& method,
                          const QString& payload = QLatin1String("{}"),
                          const QJSValue& timeout = QJSValue(),
                          const QString& sessionId = QLatin1String(""));

    /*!
     * \brief Call to service bus using service and method properties.
     * \param payload The Javascript object to use as the method call parameters.
     * \return The token number assigned to this query, 0 for error condition.
     */
    Q_INVOKABLE int callService(const QVariantMap& payload);

    int callWithRetry(const QString& service, const QString& method, const QString & payload, int retry = 5);

    /*!
     * \brief Terminates a call causing any subscription for
     * responses to end.
     */
    Q_INVOKABLE void cancel(LSMessageToken token = LSMESSAGE_TOKEN_INVALID);

    /*!
     * Push data to subscribers
     */
    Q_INVOKABLE void pushSubscription(const QString& method, const QString& param = QString(""), const QString& responseMethod = QString(""));

    /*!
     * Count of current subscribers
     */
    Q_INVOKABLE unsigned int subscribersCount(const QString &method);

    /*!
     * Register Server status whether it is connected or disconnected
     */
    Q_INVOKABLE int registerServerStatus(const QString &serviceName);

    /*!
     * \brief Derived classes reimplement the matching interface name
     * \return The name that identifies this service on the bus
     * (e.g. com.palm.systemservice)
     */
    virtual QString interfaceName() const;

    /*!
     * Return service name URIScheme prepended
     */
    virtual QString serviceUri() const;

    static bool callback(LSHandle *lshandle, LSMessage *msg, void *user_data);

    static bool callbackSubscriptionCancel(LSHandle *lshandle, LSMessage *msg, void *user_data);

    static const QLatin1String strURIScheme;
    static const QLatin1String strURISchemeDeprecated;
    static const QLatin1String strReturnValue;
    static const QLatin1String strSubscribe;
    static const QLatin1String strSubscribed;
    static const QLatin1String strErrorCode;
    static const QLatin1String strErrorText;
    static const QLatin1String strErrorCodeJsonParse;
    static const QLatin1String strErrorTextJsonParse;
    static const QLatin1String strErrorCodeInvalidType;
    static const QLatin1String strErrorTextInvalidType;
    static const QLatin1String strErrorMsg;
    static const QLatin1String strServiceName;
    static const QLatin1String strConnected;
    static const QLatin1String strTrue;
    static const QLatin1String strFalse;
    static const QLatin1String strSessionId;

public slots:
    /*!
     * \brief Announces the application ID to the luna bus. Needs to be
     * called at the top of the QML root document.
     */
    virtual void setAppId(const QString& appId);
    virtual void setRoleType(const QString& appId);

    QString appId() const { return m_appId; }
    QString roleType() const { return m_roleType; }

    /*!
     * \brief set the public bus methods
     *
     * These methods will be registered into to bus with the
     * category and the appId provided. The names must be unique
     * compared to the private bus methods. The methods can only
     * be set once, current implementatino does not support dynamic
     * methods.
     *
     * \deprecated Use \ref setMethods
     */
    void setPublicMethods(QStringList methods);

    /*!
     * \brief set the private bus methods
     *
     * These methods will be registered into to bus with the
     * category and the appId provided. The names must be unique
     * compared to the public bus methods. The methods can only
     * be set once, current implementatino does not support dynamic
     * methods.
     *
     * \deprecated Use \ref setMethods
     */
    void setPrivateMethods(QStringList methods);

    /*!
     * \brief set the methods
     *
     * These methods will be registered into to bus with the
     * category and the appId provided. The methods can only
     * be set once, current implementation does not support dynamic
     * methods.
     */
    void setMethods(QStringList methods);

    /*!
     * \brief set the category for the methods.
     *
     * This category is applied to both the private and the public
     * bus.
     */
     void setCategory(const QString& category);

    /*!
     * \brief set the sessionId.
     */
     void setSessionId(const QString& sessionId);

Q_SIGNALS:

    /*!
     * \brief Emits all replies from the bus verbatim
     * \param payload The raw JSON reply from the bus
     * \param token Provides the caller's token that is answered by this reply
     */
    void response(const QString& method, const QString& payload, int token);

    /*!
     * \brief Indicates that the query has been processed without errors.
     * \param token Provides the caller's token that is answered by this reply
     */
    void success(int token);

    /*!
     * \brief Is emitted if the call couldn't be processed correctly.
     * \param errorCode Error indentification number
     * \param errorText Describes what the error is all about
     * \param token Provides the caller's token that is answered by this reply
     */
    void error(int errorCode, const QString& errorText, int token);

    /*!
     * \brief Emitted when a service call response includes returnValue: true
     * \param response Javascript object with complete response
     */
    void callSuccess(QVariantMap response);

    /*!
     * \brief Emitted when a service call response includes returnValue: false
     * \param response Javascript object with complete response
     */
    void callFailure(QVariantMap response);

    /*!
     * \brief Emitted for any service call response -- differs from onResponse
     * in that the method is presumed to be the "method" property, and the
     * response is a Javascript object, rather than a string.
     * \param response Javascript object with complete response
     */
    void callResponse(QVariantMap response);

    /*!
     * \brief Indicates that the call has been cancelled.
     * \param token Provides the token that is cancelled.
     */
    void cancelled(int token);

    /*!
     * \brief Indicates that a subscription is about to cancel.
     * \param method Provides the method name that is being cancelled.
     */
    void subscriptionAboutToCancel(const QString& method);

    void appIdChanged();
    void publicMethodsChanged();
    void privateMethodsChanged();
    void methodsChanged();
    void categoryChanged();
    void sessionIdChanged();
    void callServiceChanged();
    void callMethodChanged();

protected:

    /*!
     * \brief Is used to process the reply from the bus. Emits the response signal.
     * \param method The method of the service that has been called
              (e.g. "/getPreferences")
     * \param payload The raw JSON reply from the bus
     * \param token Provides the caller's token that is answered by this reply
     */
    virtual void serviceResponse( const QString& method, const QString& payload, int token );

    virtual void hubError(const QString& method, const QString& error, const QString& payload, int token);

    /*!
     * \brief Checks for errors in the given payload and emits the
     *        success() and error() Qt signals.
     * \param payload the raw JSON reply from the bus
     * \param token provides the caller's token that is answered by this reply
     */
    void checkForErrors( const QString& payload, int token );

private:
    LunaServiceManager* m_serviceManager;
    QString m_appId;
    QString m_roleType;
    QString m_category;
    QString m_sessionId;
    QStringList m_methods;

    QString m_callServiceName;
    QString m_callServiceMethod;

    ClientType m_clientType;

    void registerMethods(const QStringList &methods);
    int callInternal(const QString& service,
                     const QString& method,
                     const QString& payload,
                     const QJSValue& timeout,
                     const QString& sessionId);
};

#endif // SERVICE_H
