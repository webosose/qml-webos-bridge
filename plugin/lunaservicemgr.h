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

#ifndef LUNASERVICEMGR_H
#define LUNASERVICEMGR_H

#include <QObject>
#include <QMap>
#include <luna-service2/lunaservice.h>

enum ClientType {
    ServiceClient,
    ApplicationClient
};

    /*!
     * \class LunaServiceManagerListener
     * \brief Base class for all service classes that processes replies
     *
     * This class is used to process all service bus replies from a
     * LunaServiceManager::call(). It also translates the callbacks
     * into signals that can be used in a Qt/QML style API.
     *
     * \see LunaServiceManager
     * \see Service
     */

typedef struct _CallInfo
{
    QString method;
    bool subscription;
} CallInfo;

class LunaServiceManagerListener : public QObject
{
public:
    LunaServiceManagerListener(QObject * parent) : QObject(parent) { }
    virtual ~LunaServiceManagerListener() {}

    /*!
     * \brief Is used to process the reply from the bus. Emits the response signal.
     */
    virtual void serviceResponse(const QString& method, const QString& payload, int token) = 0;

    virtual void hubError(const QString& method, const QString& error, const QString& payload, int token) = 0;

    bool isSubscription(LSMessageToken token) { return callInfos.contains(token) && callInfos[token].subscription; }

    QMap<LSMessageToken, CallInfo> callInfos;
};

    /*!
     * \class LunaServiceManager
     * \brief This class is a singleton which handles all the client requests
     *
     * This is the only instance that manages all the communication with
     * the Luna Bus using the LS2 C-API. The LS2 C-API requires
     * the creation of a custom glib eventloop. To ensure that Qt events
     * can still be processed this class inherits from ServiceEventDispatcher
     * which manages both eventloops.
     *
     * \see ServiceEventDispatcher
     */

class LunaServiceManager : QObject
{
public:
    ~LunaServiceManager();

    /*!
     * \brief Obtains the singleton LunaServiceManager.
     *
     * \return The LunaServiceManager instance
     */
    static LunaServiceManager*  instance(const QString& appId, ClientType clientType = ServiceClient, const QString& roleType = "");

    /*!
     * \brief client type
     *
     * \return ServiceClient or ApplicationClient
     */
    ClientType getClientType() const { return m_clientType; }

    /*!
     * \brief The most basic service request associated with a certain
     * interface name.
     * \param service The interface name of the service
       (e.g. "com.palm.systemservice")
     * \param method The method to be executed (e.g. "/getPreferences")
     * \param payload The method's parameters
     * (e.g. "{\"keys\":[\"airplaneMode\"], \"subscribe\":true}")
     * \param listener The object that is supposed to receive the reply
       from the bus.
     * \param sessionId The session id of destination
     * (e.g. "ab5f918c-8260-45c8-acdc-d056d24866a0")
     * \return The token number that got assigned to this query call.
     * In the event of a malfunction "0" is returned.
     */
    LSMessageToken call(const QString& service,
                        const QString& method,
                        const QString& servicePayload,
                        LunaServiceManagerListener * listener,
                        const QString& sessionId = QLatin1String(""));

    LSMessageToken callForApplication(const QString& service,
                                      const QString& method,
                                      const QString& payload,
                                      const QString& appId,
                                      LunaServiceManagerListener * listener);

    /*!
     * \brief Terminates a call causing any subscription for
     * responses to end.
     *
     * \param listener The object that is supposed to receive
     * the reply from the bus.
     */
    void cancel(LunaServiceManagerListener * listener);

    /*!
     * \brief Terminates a call for specific subscription using token
     */
    void cancel(LunaServiceManagerListener * listener, LSMessageToken token);

    void setTimeout(LSMessageToken token, int timeout);

    LSHandle* getServiceHandle();

private:
    /*!
     * \brief Private constructor to enforce singleton.
     */
    LunaServiceManager(QObject * parent = 0);

    void cancelInternal(LSHandle *sh, LSMessageToken token);

    bool               init();
    void               uninit();
    QString            m_appId;
    QString            m_roleType;
    ClientType         m_clientType;

    LSHandle           *busHandle;
};

Q_DECLARE_METATYPE(LSMessageToken)

#endif // LUNASERVICEMGR_H
