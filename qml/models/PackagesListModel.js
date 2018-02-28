// Copyright (c) 2013-2018 LG Electronics, Inc.
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

    var jsonObject = JSON.parse(listModel.source);
    var packages = jsonObject.packages
    for (var package in packages) {
        // Provide the file scheme to the icon for reliable loading of the icon. Without
        // it the path can be interpreted as a relative path to a resource file bundled
        // with the binary
        listModel.append({   packageId: packages[package].id,
                             version: packages[package].version,
                             size: packages[package].size,
                             loc_name: packages[package].loc_name,
                             vendor: packages[package].vendor,
                             vendorUrl: packages[package].vendorUrl,
                             icon: "file://" + packages[package].icon,
                             miniIcon: "file://" + packages[package].miniIcon,
                             userInstalled: packages[package].userInstalled
                         });
    }
}
