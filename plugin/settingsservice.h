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

#ifndef SETTINGSSERVICE_H
#define SETTINGSSERVICE_H

#include "service.h"
#include <QList>
#include <QTranslator>
#include <QSharedPointer>

/*!
 * \class SettingsService
 * \brief Provides QML property bindings for the service
 * com.webos.settingsservice
 *
 * This class provides
 * \li Properties and methods for the associated service.
 * \li The type of properties, parameters and return values
 * matches Qt types.
 *
 * \see Service
 */

class SettingsService : public MessageSpreaderListener
{
    Q_OBJECT

    Q_PROPERTY(bool cached READ cached NOTIFY cachedChanged)
    Q_PROPERTY(bool speechToTextLocaleMode READ speechToTextLocaleMode WRITE setSpeechToTextLocaleMode NOTIFY speechToTextLocaleModeChanged)
    Q_PROPERTY(QString currentLocale READ currentLocale WRITE setCurrentLocale NOTIFY currentLocaleChanged)
    Q_PROPERTY(QString l10nFileNameBase READ l10nFileNameBase WRITE setl10nFileNameBase NOTIFY l10nFileNameBaseChanged)
    Q_PROPERTY(QString l10nDirName READ l10nDirName WRITE setl10nDirName NOTIFY l10nDirNameChanged)
    Q_PROPERTY(QVariantList l10nPluginImports READ l10nPluginImports WRITE setl10nPluginImports NOTIFY l10nPluginImportsChanged)
    Q_PROPERTY(QString screenRotation READ screenRotation NOTIFY screenRotationChanged)
    Q_PROPERTY(QString bootStatus READ bootStatus NOTIFY bootStatusChanged)

    // As per "http://qt-project.org/wiki/How_to_do_dynamic_translation_in_QML"
    Q_PROPERTY(QString emptyString READ getEmptyString NOTIFY currentLocaleChanged)

public:
    SettingsService(QObject * parent = 0);
    virtual ~SettingsService();

    void setAppId(const QString& appId);
    QString interfaceName() const;
    Q_INVOKABLE void cancel(LSMessageToken token = LSMESSAGE_TOKEN_INVALID);

    Q_INVOKABLE bool subscribeForLocaleChange(); // Deprecated
    Q_INVOKABLE bool subscribe();
    void setSpeechToTextLocale(const QString& speechToTextLocale);

    Q_INVOKABLE QString getEmptyString() const { return QLatin1String(""); }

    bool cached() const { return m_cached; };
    QString currentLocale() const { return m_currentLocale; }
    bool speechToTextLocaleMode() const { return m_speechToTextLocaleMode; }
    QString speechToTextLocale() const { return m_speechToTextLocale; }
    QString l10nFileNameBase() const { return m_l10nFileNameBase; }
    QString l10nDirName() const { return m_l10nDirName; }
    QVariantList l10nPluginImports() const { return m_l10nPluginImports; }
    QString screenRotation() const { return m_screenRotation; }
    QString bootStatus() const { return m_bootStatus; };

signals:
    void cachedChanged();
    void currentLocaleChanged();
    void speechToTextLocaleModeChanged();
    void speechToTextLocaleChanged();
    void l10nFileNameBaseChanged();
    void l10nDirNameChanged();
    void l10nLoadSucceeded(const QString& file);
    void l10nLoadFailed(const QString& file);
    void l10nInstallSucceeded(const QString& file);
    void l10nInstallFailed(const QString& file);

    void l10nFileNameChanged();
    void l10nPluginImportsChanged();

    void screenRotationChanged();
    void bootStatusChanged();

public slots:
    void handleLocaleChange();
    void setCurrentLocale(const QString& currentLocale);
    void setSpeechToTextLocaleMode(bool speechToTextLocaleMode);
    void setl10nFileNameBase(const QString& l10nFileNameBase);
    void setl10nDirName(const QString& l10nDirName);
    void setl10nPluginImports(const QVariantList& l10nPluginImports);

protected:
    /*!
     * \brief Is used to process the reply from the bus. Emits
     * the response signal.
     * \param method The method of the service that has been called
       (e.g. "/getSystemSettings")
     * \param payload The raw JSON reply from the bus
     * \param token Provides the caller's token that is answered
     * by this reply
     */
    void serviceResponseDelayed(const QString& method, const QString& payload, int token, const QJsonObject &jsonPayload) override;

    void hubError(const QString& method, const QString& error, const QString& payload, int token);

    bool findl10nFileName(const QString& dir, const QString& file, QString &rFilename);

protected slots:
    void resetSubscription();

private:
    LSMessageToken m_tokenServerStatusBootd;
    LSMessageToken m_tokenServerStatusSettings;
    LSMessageToken m_tokenLocale;
    LSMessageToken m_tokenSystemSettings;
    LSMessageToken m_tokenBootd;

    void installTranslator(const QString& dir, const QString& file);
    void uninstallTranslator(const QString& dir, const QString& file);

    bool subscribeInternal();
    bool tryToSubscribe();
    bool subscribeBootdInternal();

    void setCached(const bool cached);
    void setScreenRotation(const QString& screenRotation);
    void setBootStatus(const QString& bootStatus);

    bool m_cached;
    QString m_currentLocale;
    QString m_speechToTextLocale;
    QString m_l10nFileNameBase;
    QString m_l10nDirName;
    /*!
     * \brief array of [ "pluginName1", ... "pluginNameN" ]
     * will assume it is in parent of m_l10nDirName
     */
    QVariantList m_l10nPluginImports;

    QString m_screenRotation;
    QString m_bootStatus;

    bool m_subscriptionRequested;
    bool m_speechToTextLocaleMode;
    bool m_cacheRead;
    bool m_connected;

    QList<QSharedPointer<QTranslator>> m_translators;
};

#endif // SETTINGSSERVICE_H
