// Copyright (c) 2013-2022 LG Electronics, Inc.
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

function load() {
    listModel.clear();
    var jsonObject = listModel.jsonSource;
    var apps = jsonObject.apps
    for (var app in apps) {
        // Provide the file scheme to the icon for reliable loading of the icon. Without
        // it the path can be interpreted as a relative path to a resource file bundled
        // with the binary
        listModel.append({icon: "file://" + apps[app].icon, appId: apps[app].id});
    }
}
