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
    var jsonObject = JSON.parse(listModel.source);
        if(jsonObject.pincodePromptAction == "close") {
           for(var i = 0; i < listModel.count; i++) {
              var obj = listModel.get(i);
              if(obj.timestamp == jsonObject.pincodePromptInfo.timestamp) {
                    listModel.remove(i);
                    break;
              }
           }
           return;
        }
    listModel.append(jsonObject);
}
