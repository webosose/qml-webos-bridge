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

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QGuiApplication>
#include <QPointer>
#include <QMutex>

#include "settingsservice.h"

static const QLatin1String serviceNameBootd("com.webos.bootManager");
static const QLatin1String strBootStatus("bootStatus");
static const QLatin1String methodGetBootStatus("/getBootStatus");

static const char* G_OPTION_FILE = "/var/luna/preferences/option";
static const char* G_LOCALE_INFO_FILE = "/var/luna/preferences/localeInfo";
static const QLatin1String strKeys("keys");
static const QLatin1String strCategory("category");
static const QLatin1String strOption("option");
static const QLatin1String strScreenRotation("screenRotation");
static const QLatin1String strLocaleInfo("localeInfo");
static const QLatin1String strLocales("locales");
static const QLatin1String strUi("UI");
static const QLatin1String strSettings("settings");
static const QLatin1String strUnderBar("_");
static const QLatin1String strHyphen("-");
static const QLatin1String strFileTypeQm(".qm");
static const QLatin1String strDot(".");
static const QLatin1String methodGetSystemSettings("/getSystemSettings");
static const QLatin1String serviceNameSettings("com.webos.settingsservice");

static QList<QTranslator *> translators = QList<QTranslator *>();

SettingsService::SettingsService(QObject * parent)
    : Service(parent)
    , m_cached(false)
    , m_tokenServerStatusBootd(LSMESSAGE_TOKEN_INVALID)
    , m_tokenServerStatusSettings(LSMESSAGE_TOKEN_INVALID)
    , m_tokenLocale(LSMESSAGE_TOKEN_INVALID)
    , m_tokenSystemSettings(LSMESSAGE_TOKEN_INVALID)
    , m_tokenBootd(LSMESSAGE_TOKEN_INVALID)
    , m_subscriptionRequested(false)
    , m_cacheRead(false)
    , m_connected(false)
    , m_pTranslators(QList<QTranslator *>())
{
}

SettingsService::~SettingsService()
{
    if (!m_pTranslators.isEmpty()) {
        QList<QTranslator*>::iterator iter;
        for (iter = m_pTranslators.begin(); iter != m_pTranslators.end(); iter++) {
            QGuiApplication::instance()->removeTranslator(*iter);
        }
        m_pTranslators.clear();
    }
}

void SettingsService::setAppId(const QString& appId)
{
    Service::setAppId(appId);

    if (m_tokenServerStatusBootd == LSMESSAGE_TOKEN_INVALID)
        m_tokenServerStatusBootd = registerServerStatus(serviceNameBootd);
    if (m_tokenServerStatusSettings == LSMESSAGE_TOKEN_INVALID)
        m_tokenServerStatusSettings = registerServerStatus(serviceNameSettings);
}

QString SettingsService::interfaceName() const
{
    // Return serviceNameSettings though we provide more
    return QString(serviceNameSettings);
}

void SettingsService::cancel(LSMessageToken token)
{
    Service::cancel(token);

    // the subscription to registerServerStatus is also cancelled this case, restore it
    if (token == LSMESSAGE_TOKEN_INVALID || token == m_tokenServerStatusBootd)
        m_tokenServerStatusBootd = registerServerStatus(serviceNameBootd);
    if (token == LSMESSAGE_TOKEN_INVALID || token == m_tokenServerStatusSettings)
        m_tokenServerStatusSettings = registerServerStatus(serviceNameSettings);
}

// Deprecated
bool SettingsService::subscribeForLocaleChange()
{
    return subscribe();
}

bool SettingsService::subscribeInternal()
{
    m_subscriptionRequested = true;

    if (m_tokenLocale != LSMESSAGE_TOKEN_INVALID)
        cancel(m_tokenLocale);

    m_tokenLocale = call(strURIScheme + serviceNameSettings,
            methodGetSystemSettings,
            QString(QLatin1String("{\"%1\":%2,\"%3\":[\"%4\"]}")).arg(strSubscribe).arg(strTrue).arg(strKeys).arg(strLocaleInfo));

    if (m_tokenLocale == LSMESSAGE_TOKEN_INVALID) {
        qWarning() << "SettingsService: Failed to subscribe to" << strLocaleInfo;
        return false;
    }

    if (m_tokenSystemSettings != LSMESSAGE_TOKEN_INVALID)
        cancel(m_tokenSystemSettings);

    m_tokenSystemSettings = call(strURIScheme + serviceNameSettings,
            methodGetSystemSettings,
            QString(QLatin1String("{\"%1\":%2,\"%3\":\"%4\",\"%5\":[\"%6\"]}")).arg(strSubscribe).arg(strTrue).arg(strCategory).arg(strOption).arg(strKeys).arg(strScreenRotation));

    if (m_tokenSystemSettings == LSMESSAGE_TOKEN_INVALID) {
        qWarning() << "SettingsService: Failed to subscribe to" << strScreenRotation;
        return false;
    }

    return true;
}

bool SettingsService::tryToSubscribe()
{
    if (m_connected && m_cacheRead && m_subscriptionRequested) {
        qInfo() << "Subscribing to settings";
        return subscribeInternal();
    }

    // Treat delayed subscription as success
    qWarning() << "Subscription deferred, requested:" << m_subscriptionRequested << "cacheRead:" << m_cacheRead << "connected:" << m_connected;
    return true;
}

bool SettingsService::subscribe()
{
    m_subscriptionRequested = true;

    return tryToSubscribe();
}

bool SettingsService::subscribeBootdInternal()
{
    if (m_tokenBootd != LSMESSAGE_TOKEN_INVALID)
        cancel(m_tokenBootd);

    m_tokenBootd = call(strURIScheme + serviceNameBootd,
            methodGetBootStatus,
            QString(QLatin1String("{\"%1\":%2}")).arg(strSubscribe).arg(strTrue));

    if (m_tokenBootd == LSMESSAGE_TOKEN_INVALID) {
        qWarning() << "SettingsService: Failed to subscribe to" << methodGetBootStatus;
        return false;
    }

    return true;
}

void SettingsService::serviceResponse(const QString& method, const QString& payload, int token)
{
    checkForErrors(payload, token);
    emit response(method, payload, token);

    QJsonObject rootObject = QJsonDocument::fromJson(payload.toUtf8()).object();
    if (token == m_tokenServerStatusBootd && rootObject.find(strServiceName).value().toString() == serviceNameBootd) {
        bool connected = rootObject.find(strConnected).value().toBool();
        if (connected)
            subscribeBootdInternal();
    } else if (token == m_tokenBootd && method == methodGetBootStatus) {
        if (!rootObject.find(strReturnValue).value().toBool())
            return; //Ignore the subscription failed response

        setBootStatus(rootObject.value(strBootStatus).toString());

        // Read settings from file caches at very first
        if (!m_cacheRead) {
            // Mark cached as true indicating following reads are performed from the file cache
            setCached(true);

            QFile fileLocale(G_LOCALE_INFO_FILE);
            if (fileLocale.open(QFile::ReadOnly | QFile::Text)) {
                QTextStream in(&fileLocale);
                QJsonObject object = QJsonDocument::fromJson(in.readAll().toUtf8()).object();
                if (!object.isEmpty()) {
                    QString s = object.value(strLocaleInfo).toObject().value(strLocales).toObject().value(strUi).toString();
                    qInfo() << "Set currentLocale from" << G_LOCALE_INFO_FILE << ":" << s;
                    setCurrentLocale(s);
                }
            }
            fileLocale.close();

            QFile fileOption(G_OPTION_FILE);
            if (fileOption.open(QFile::ReadOnly | QFile::Text)) {
                QTextStream in(&fileOption);
                QJsonObject object = QJsonDocument::fromJson(in.readAll().toUtf8()).object();
                if (!object.isEmpty()) {
                    // screenRotation
                    QString s = object.value(strScreenRotation).toString();
                    qInfo() << "Set screenRotation from" << G_OPTION_FILE << ":" << s;
                    setScreenRotation(s);
                }
            }
            fileOption.close();

            m_cacheRead = true;
            tryToSubscribe();
        }
    } else if (token == m_tokenServerStatusSettings && rootObject.find(strServiceName).value().toString() == serviceNameSettings) {
        m_connected = rootObject.find(strConnected).value().toBool();
        if (m_connected)
            tryToSubscribe();
    } else if (method == methodGetSystemSettings) {
        if (!rootObject.find(strReturnValue).value().toBool())
            return; //Ignore the subscription failed response

        setCached(false);

        if (token == m_tokenLocale) {
            QString s = rootObject.value(strSettings).toObject().value(strLocaleInfo).toObject().value(strLocales).toObject().value(strUi).toString();
            qInfo() << "Set currentLocale from LS2 response:" << s;
            setCurrentLocale(s);
        } else if (token == m_tokenSystemSettings) {
            QString s = rootObject.value(strSettings).toObject().value(strScreenRotation).toString();
            qInfo() << "Set screenRotation from LS2 response:" << s;
            setScreenRotation(s);
        }
    }
}

void SettingsService::hubError(const QString& method, const QString& error, const QString& payload, int token)
{
    qWarning() << "SettingsService: Hub error:" << error;

    checkForErrors(payload, token);

    if (error == LUNABUS_ERROR_SERVICE_DOWN) {
        if (m_subscriptionRequested) {
            qWarning() << "SettingsService: Hub error:" << error << "- recover subscriptions";
            cancel();
            tryToSubscribe();
        }
    }
}

// translations are found in
// /.../locales/full-package-name/last-dotted-part-of-component-name

bool SettingsService::loadTranslator(const QPointer<QTranslator> tr, const QString& dir, const QString& file)
{
    QString componentName = file.right(file.size() - (file.lastIndexOf(strDot)+1));
    QString outFilename;
    if (findl10nFileName(dir, componentName, outFilename) && tr->load(outFilename, dir, strHyphen, strFileTypeQm)) {
        return true;
    }
    if (QLocale(m_currentLocale) != QLocale::C && tr->load(QLocale(m_currentLocale), componentName, strUnderBar, dir)) {
        return true;
    }
    qWarning() << "QLocale Data not matched" << m_currentLocale << dir << file;
    return false;
}

QPointer<QTranslator> SettingsService::installTranslator(const QString& dir, const QString& file)
{
    QPointer<QTranslator> tr = new QTranslator(QGuiApplication::instance());
    if (!loadTranslator(tr, dir, file)) {
        emit l10nLoadFailed(file);
        delete tr;
    } else {
        emit l10nLoadSucceeded(file);
        if (!QGuiApplication::instance()->installTranslator(tr)) {
            emit l10nInstallFailed(file);
            delete tr;
        } else {
            emit l10nInstallSucceeded(file);
        }
    }
    return tr;
}

void SettingsService::handleLocaleChange()
{
    static QString currentLocale("");
    static QMutex mutex;

    QMutexLocker locker(&mutex);
    if (currentLocale != m_currentLocale) {
        QList<QTranslator*>::iterator iter;
        for (iter = translators.begin(); iter != translators.end(); iter++)
            QGuiApplication::instance()->removeTranslator(*iter);
        translators.clear();
        currentLocale = m_currentLocale;
    }

    QTranslator *translator = installTranslator(m_l10nDirName, m_l10nFileNameBase);
    m_pTranslators.append(translator);
    translators.append(translator);

    QVariantList::iterator i;
    for (i = m_l10nPluginImports.begin(); i != m_l10nPluginImports.end(); i++) {
        QString index = (*i).toString();
        if (index.size()) {
            translator = installTranslator(QString(QLatin1String("%1/../%2")).arg(m_l10nDirName).arg(index), index);
            m_pTranslators.append(translator);
            translators.append(translator);
        }
    }
}

bool SettingsService::findl10nFileName(const QString& dir, const QString& file, QString& rFilename)
{
    rFilename = m_currentLocale;

    QString firstGuessl10nFileName = QString(QLatin1String("%1_%2")).arg(file).arg(rFilename.replace(strHyphen,strUnderBar));
    QString secondGuessl10nFileName = firstGuessl10nFileName.left(firstGuessl10nFileName.lastIndexOf(strUnderBar));
    QString thirdGuessl10nFileName = secondGuessl10nFileName.left(secondGuessl10nFileName.lastIndexOf(strUnderBar));

    if (QFile::exists(QString(QLatin1String("%1/%2%3")).arg(dir).arg(firstGuessl10nFileName).arg(strFileTypeQm))) {
        rFilename = firstGuessl10nFileName;
    } else if (QFile::exists(QString(QLatin1String("%1/%2%3")).arg(dir).arg(secondGuessl10nFileName).arg(strFileTypeQm))) {
        rFilename = secondGuessl10nFileName;
    } else if (QFile::exists(QString(QLatin1String("%1/%2%3")).arg(dir).arg(thirdGuessl10nFileName).arg(strFileTypeQm))) {
        rFilename = thirdGuessl10nFileName;
    }
    else {
        qWarning() << "can not find .qm files(findl10nFileName)" << firstGuessl10nFileName << "and" << secondGuessl10nFileName << "and" << thirdGuessl10nFileName;
        rFilename = file;
        return false;
    }
   return true;
}

void SettingsService::setCurrentLocale(const QString& currentLocale) {
    if (!currentLocale.isEmpty() && currentLocale != m_currentLocale) {
        m_currentLocale = currentLocale;
        QLocale::setDefault(QLocale(m_currentLocale));
        handleLocaleChange();
        emit currentLocaleChanged();
    }
}

void SettingsService::setl10nFileNameBase(const QString& l10nFileNameBase)
{
    m_l10nFileNameBase = l10nFileNameBase;
    emit l10nFileNameBaseChanged();
}

void SettingsService::setl10nDirName(const QString& l10nDirName)
{
    m_l10nDirName = l10nDirName;
    emit l10nDirNameChanged();
}

void SettingsService::setl10nPluginImports(const QVariantList& l10nPluginImports)
{
    m_l10nPluginImports = l10nPluginImports;
    emit l10nPluginImportsChanged();
}

void SettingsService::setCached(const bool cached)
{
    if (m_cached != cached) {
        m_cached = cached;
        emit cachedChanged();
    }
}

void SettingsService::setScreenRotation(const QString& screenRotation)
{
    if (!screenRotation.isEmpty() && m_screenRotation != screenRotation) {
        m_screenRotation = screenRotation;
        emit screenRotationChanged();
    }
}

void SettingsService::setBootStatus(const QString& bootStatus)
{
    if (!bootStatus.isEmpty() && m_bootStatus != bootStatus) {
        qInfo() << "bootStatus:" << m_bootStatus << "->" << bootStatus;
        m_bootStatus = bootStatus;
        emit bootStatusChanged();
    }
}
