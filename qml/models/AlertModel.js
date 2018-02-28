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

    if(jsonObject.alertAction == "closeAll") {
        listModel.clear();
        return;
    }

    if(jsonObject.alertAction == "close") {
       for(var i = 0; i < listModel.count; i++) {
          var obj = listModel.get(i);
          if(obj.timestamp == jsonObject.alertInfo.timestamp) {
              listModel.remove(i);
              break;
          }
       }
       return;
    }

    // Alert stacking logic is first come first served basis. But modal alert should jump ahead the queue and be placed at top.
    if (listModel.count > 0 && isValidModalAlert(jsonObject)) {
        var i;
        for (i = 0; i < listModel.count; i++) {
            if (!isValidModalAlert(listModel.get(i))) {
                listModel.insert(i, jsonObject);
                break;
            }
        }
        if (i == listModel.count)
            listModel.append(jsonObject);
    }
    else {
        listModel.append(jsonObject);
    }

    function isValidModalAlert(jsonModelData) {
        return (jsonModelData.alertInfo.modal === true);
    }
}
