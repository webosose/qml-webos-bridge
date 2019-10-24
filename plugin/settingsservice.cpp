// Copyright (c) 2012-2019 LG Electronics, Inc.
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
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QGuiApplication>
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
static const QLatin1String strStt("STT");
static const QLatin1String strSettings("settings");
static const QLatin1String strUnderBar("_");
static const QLatin1String strHyphen("-");
static const QLatin1String strFileTypeQm(".qm");
static const QLatin1String strDot(".");
static const QLatin1String methodGetSystemSettings("/getSystemSettings");
static const QLatin1String serviceNameSettings("com.webos.settingsservice");

/* NOTE
   Translators are cached throughout process alive under same locale information. if the locale is
   changed, the cached are cleared and re-created as the changed locale.
*/
static QList<QSharedPointer<QTranslator>> s_cachedTranslators = QList<QSharedPointer<QTranslator>>();

class WebOSTranslator : public QTranslator
{
    Q_OBJECT
public:
    static bool isInstalledTranslator(const QLocale &locale, const QString &comp,
                                      const QString &l10n, const QString &dir)
    {
        qDebug () << "s_cachedTranslators.length=" << s_cachedTranslators.size();
        for (int i = 0; i < s_cachedTranslators.size(); i++) {
            WebOSTranslator *wtr = reinterpret_cast<WebOSTranslator*>(s_cachedTranslators.at(i).data());
            if (wtr->isEqualSource(locale, comp, l10n, dir)) {
                qDebug() << "existed translator: qmDir=" << wtr->qmDir() << ", qmComp=" << wtr->qmComp()
                         << ", qmL10n=" << wtr->qmL10n() << ", qmLocale=" << wtr->qmLocale()
                         << ", WebOSTranslator=" << wtr;
                return true;
            }
        }
        return false;
    }

    static void dropCachedTranslator(const QLocale &locale)
    {
        QList<QSharedPointer<QTranslator>>::iterator it = s_cachedTranslators.begin();
        while (it != s_cachedTranslators.end()) {
            WebOSTranslator *wtr = reinterpret_cast<WebOSTranslator*>(it->data());
            if (!wtr->isEqualLocale(locale)) {
                qDebug() << "drop cached translator: qmDir=" << wtr->qmDir() << ", qmComp=" << wtr->qmComp()
                         << ", qmL10n=" << wtr->qmL10n() << ", qmLocale=" << wtr->qmLocale()
                         << ", WebOSTranslator=" << wtr;
                it = s_cachedTranslators.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    static void dropCachedTranslator(const QLocale &locale, const QString &comp,
                                     const QString &l10n, const QString &dir)
    {
        QList<QSharedPointer<QTranslator>>::iterator it = s_cachedTranslators.begin();
        while (it != s_cachedTranslators.end()) {
            WebOSTranslator *wtr = reinterpret_cast<WebOSTranslator*>(it->data());
            if (!wtr->isEqualLocale(locale) && wtr->isEqualComp(comp, dir)) {
                qDebug() << "drop cached translator: qmDir=" << wtr->qmDir() << ", qmComp=" << wtr->qmComp()
                         << ", qmL10n=" << wtr->qmL10n() << ", qmLocale=" << wtr->qmLocale()
                         << ", WebOSTranslator=" << wtr;
                it = s_cachedTranslators.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    static void appendCachedTranslator(QSharedPointer<QTranslator> &tr)
    {
        s_cachedTranslators.append(tr);
    }

public:
    explicit WebOSTranslator(QObject *parent = Q_NULLPTR)
        : QTranslator(parent)
        , m_installed(false)
    {
        qDebug() << "translator is created: WebOSTranslator=" << this;
    }
    ~WebOSTranslator()
    {
        uninstall();
        qDebug() << "translator is destroyed: qmDir=" << m_qmDir << ", m_qmL10n=" << m_qmL10n
                << ", m_qmComp=" << m_qmComp << ", qmLocale=" << m_qmLocale
                << ", WebOSTranslator=" << this;
    }

    bool loadSource(const QLocale &locale, const QString &comp, const QString &l10n, const QString &dir,
                    const QString &search_delimiters = strHyphen,
                    const QString &format = strFileTypeQm,
                    const QString &prefix = strUnderBar)
    {
        if (!QTranslator::load(l10n, dir, search_delimiters, format)) {
            qWarning() << "failure in loading translator file: l10n=" << l10n
                       << ", dir=" << dir << ", search_delimiters=" << search_delimiters
                       << ", format=" << format;

            if (QLocale(locale) == QLocale::C) {
                qWarning() << "failure in loading translator file: locale=" << locale;
                return false;
            }
            if (!QTranslator::load(locale, comp, prefix, dir)) {
                qWarning() << "failure in loading translator file: locale=" << locale
                           << ", comp=" << comp << ", prefix=" << prefix << ", dir=" << dir;
                return false;
            }
        }

        m_qmLocale = locale;
        m_qmComp = comp;
        m_qmL10n = l10n;
        m_qmDir = QDir::cleanPath(dir);
        qInfo() << "translator is loaded: qmDir=" << m_qmDir << ", m_qmL10n=" << m_qmL10n
                << ", m_qmComp=" << m_qmComp << ", qmLocale=" << m_qmLocale
                << ", WebOSTranslator=" << this;
        return true;
    }

    bool install()
    {
        if (!m_installed) {
            m_installed = QGuiApplication::instance()->installTranslator(this);
            if (!m_installed)
                qWarning() << "failure in translator install: WebOSTranslator=" << this;
        }
        return m_installed;
    }
    bool uninstall()
    {
        if (m_installed) {
            if (!QGuiApplication::instance()->removeTranslator(this))
                qWarning() << "failure in translator uninstall: WebOSTranslator=" << this;
            m_installed = false;
        }
        return m_installed;
    }

    bool isEqualSource(const QLocale &locale, const QString &comp, const QString &l10n, const QString &dir)
    {
        return m_qmLocale == locale && m_qmComp == comp && m_qmL10n == l10n && m_qmDir == QDir::cleanPath(dir);
    }
    bool isEqualLocale(const QLocale &locale)
    {
        return m_qmLocale == locale;
    }
    bool isEqualComp(const QString &comp, const QString &dir)
    {
        return m_qmComp == comp && m_qmDir == QDir::cleanPath(dir);
    }

    QLocale qmLocale() const { return m_qmLocale; }
    QString qmComp() const { return m_qmComp; }
    QString qmL10n() const { return m_qmL10n; }
    QString qmDir() const { return m_qmDir; }

private:
    QLocale m_qmLocale;
    QString m_qmComp;
    QString m_qmL10n;
    QString m_qmDir;
    bool m_installed;

private:
    Q_DISABLE_COPY(WebOSTranslator)
};

SettingsService::SettingsService(QObject * parent)
    : Service(parent)
    , m_cached(false)
    , m_tokenServerStatusBootd(LSMESSAGE_TOKEN_INVALID)
    , m_tokenServerStatusSettings(LSMESSAGE_TOKEN_INVALID)
    , m_tokenLocale(LSMESSAGE_TOKEN_INVALID)
    , m_tokenSystemSettings(LSMESSAGE_TOKEN_INVALID)
    , m_tokenBootd(LSMESSAGE_TOKEN_INVALID)
    , m_subscriptionRequested(false)
    , m_speechToTextLocaleMode(false)
    , m_cacheRead(false)
    , m_connected(false)
{
}

SettingsService::~SettingsService()
{
    m_translators.clear();
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

                    QString speechToTextLocale = object.value(strLocaleInfo).toObject().value(strLocales).toObject().value(strStt).toString();
                    setSpeechToTextLocale(speechToTextLocale);
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

            QString speechToTextLocale = rootObject.value(strSettings).toObject().value(strLocaleInfo).toObject().value(strLocales).toObject().value(strStt).toString();
            setSpeechToTextLocale(speechToTextLocale);
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
void SettingsService::uninstallTranslator(const QString& dir, const QString& file)
{
    QString compName = file.right(file.size() - (file.lastIndexOf(strDot)+1));
    QString l10nName;
    if (!findl10nFileName(dir, compName, l10nName))
        qDebug() << "failure in finding l10n file: file=" << file << ", dir=" << dir;
    WebOSTranslator::dropCachedTranslator(QLocale(m_currentLocale), compName, l10nName, dir);
}

void SettingsService::installTranslator(const QString& dir, const QString& file)
{
    QString compName = file.right(file.size() - (file.lastIndexOf(strDot)+1));
    QString l10nName;
    if (!findl10nFileName(dir, compName, l10nName))
        qDebug() << "failure in finding l10n file: file=" << file << ", dir=" << dir;

    if (WebOSTranslator::isInstalledTranslator(QLocale(m_currentLocale), compName, l10nName, dir)) {
        emit l10nLoadSucceeded(file);
        emit l10nInstallSucceeded(file);
        return;
    }

    QSharedPointer<QTranslator> tr(new WebOSTranslator(QGuiApplication::instance()));
    if (!reinterpret_cast<WebOSTranslator*>(tr.data())->loadSource(QLocale(m_currentLocale),
                                                                   compName, l10nName, dir,
                                                                   strHyphen, strFileTypeQm, strUnderBar)) {
        emit l10nLoadFailed(file);
        return;
    }
    emit l10nLoadSucceeded(file);

    if (!reinterpret_cast<WebOSTranslator*>(tr.data())->install()) {
        emit l10nInstallFailed(file);
        return;
    }
    emit l10nInstallSucceeded(file);

    m_translators.append(tr);
    WebOSTranslator::appendCachedTranslator(tr);
}

void SettingsService::handleLocaleChange()
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);

    m_translators.clear();
    WebOSTranslator::dropCachedTranslator(QLocale(m_currentLocale));

    uninstallTranslator(m_l10nDirName, m_l10nFileNameBase);
    installTranslator(m_l10nDirName, m_l10nFileNameBase);

    QVariantList::iterator i;
    for (i = m_l10nPluginImports.begin(); i != m_l10nPluginImports.end(); i++) {
        QString index = (*i).toString();
        if (index.size()) {
            uninstallTranslator(QString(QLatin1String("%1/../%2")).arg(m_l10nDirName).arg(index), index);
            installTranslator(QString(QLatin1String("%1/../%2")).arg(m_l10nDirName).arg(index), index);
        }
    }
}

bool SettingsService::findl10nFileName(const QString& dir, const QString& file, QString& rFilename)
{
    rFilename = speechToTextLocaleMode() ? m_speechToTextLocale : m_currentLocale;

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

void SettingsService::setSpeechToTextLocaleMode(bool speechToTextLocaleMode) {
    if (speechToTextLocaleMode != m_speechToTextLocaleMode) {
        m_speechToTextLocaleMode = speechToTextLocaleMode;
        handleLocaleChange();
        emit speechToTextLocaleModeChanged();
    }
}

void SettingsService::setSpeechToTextLocale(const QString& speechToTextLocale) {
    if (!speechToTextLocale.isEmpty() && speechToTextLocale != m_speechToTextLocale) {
        m_speechToTextLocale = speechToTextLocale;
        handleLocaleChange();
        emit speechToTextLocaleChanged();
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

#include "settingsservice.moc"