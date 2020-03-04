# Copyright (c) 2013-2020 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

TEMPLATE = subdirs

MOC_DIR = .moc
OBJECTS_DIR = .obj

SUBDIRS = plugin

load(configure)
qtCompileTest(session)

QML_FILES +=  \
    $$PWD/qml/models/LaunchPointsModel.qml \
    $$PWD/qml/models/ApplicationListModel.qml \
    $$PWD/qml/models/PincodePromptModel.qml \
    $$PWD/qml/models/AlertModel.qml \
    $$PWD/qml/models/RunningModel.qml \
    $$PWD/qml/models/DB8.qml \
    $$PWD/qml/models/ToastModel.qml \
    $$PWD/qml/models/PackagesListModel.qml \
    $$PWD/qml/models/InputAlertModel.qml \
    $$PWD/qml/models/RunningModel.js \
    $$PWD/qml/models/PincodePromptModel.js \
    $$PWD/qml/models/PackagesListModel.js \
    $$PWD/qml/models/AlertModel.js \
    $$PWD/qml/models/ToastModel.js \
    $$PWD/qml/models/InputAlertModel.js \
    $$PWD/qml/models/ApplicationListModel.js \

# The WEBOS_INSTALL_QML Qt variable must be set in the build
# environment by qmake WEBOS_INSTALL_QML=<location>
qmlfiles.files = $$QML_FILES
qmlfiles.path = $$WEBOS_INSTALL_QML/WebOSServices
INSTALLS += qmlfiles
