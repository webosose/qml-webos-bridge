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

#include "webosserviceplugin.h"

#include <QQmlComponent>

#include "applicationmanagerservice.h"
#include "systemservice.h"
#include "notificationservice.h"
#include "settingsservice.h"
#include "servicemodel.h"

void WebOSServicePlugin::registerTypes(const char *uri)
{
    Q_UNUSED(uri)
    qmlRegisterType<ApplicationManagerService>("WebOSServices", 1,0, "ApplicationManagerService");
    qmlRegisterType<SystemService>("WebOSServices", 1,0, "SystemService");
    qmlRegisterType<NotificationService>("WebOSServices", 1,0, "NotificationService");
    qmlRegisterType<SettingsService>("WebOSServices", 1,0, "LocaleService"); // superceded by SettingsService
    qmlRegisterType<SettingsService>("WebOSServices", 1,0, "SettingsService");
    qmlRegisterType<Service>("WebOSServices", 1,0, "Service");
    qmlRegisterUncreatableType<ServiceModel>("WebOSServices", 1,0, "ServiceModel", "Abstract type");
}
