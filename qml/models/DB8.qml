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

import QtQuick 2.0
import WebOSServices 1.0

Service {
    id:db8
    property var callbacks: []
    onResponse: handleResponse(method, payload, token)

    function handleResponse(method, payload, token)
    {
        var json = JSON.parse(payload);
        var callback;

        for (var i = 0; i < callbacks.length; i++) {
            if (callbacks[i].token == token) {
                callback = callbacks.splice(i, 1)[0];
                break;
            }
        }

        if(callback) {
            if(json.returnValue == true && callback.success) {
                callback.success(json);
            } else if(json.returnValue == false && callback.failure) {
                callback.failure(json);
            } else {
                console.log("DB8 " +appId+ " unhandled response from call to "+method+":"+payload);
            }
        }
    }

    function callDB(func, args, onSuccess, onFailure) {
        var ret = call("luna://com.palm.db",
        func,
        JSON.stringify(args));
        callbacks.push({token:ret, success:onSuccess, failure:onFailure});
    }

    function delKind(args, onSuccess, onFailure) {
        callDB("/delKind", args, onSuccess, onFailure);
    }

    function putPermissions(args, onSuccess, onFailure) {
        callDB("/putPermissions", args, onSuccess, onFailure);
    }

    function putKind(args, onSuccess, onFailure) {
        callDB("/putKind", args, onSuccess, onFailure);
    }

    function put(data, onSuccess, onFailure) {
        callDB("/put", data, onSuccess, onFailure);
    }

    function merge(args, onSuccess, onFailure) {
        callDB("/merge", args, onSuccess, onFailure);
    }

    function find(query, onSuccess, onFailure) {
        callDB("/find", query, onSuccess, onFailure);
    }

    function get(ids, onSuccess, onFailure) {
        callDB("/get", ids, onSuccess, onFailure);
    }

    function batch(ops, onSuccess, onFailure) {
        callDB("/batch", ops, onSuccess, onFailure);
    }

    function del(query, onSuccess, onFailure) {
        callDB("/del", query, onSuccess, onFailure);
    }

    function search(query, onSuccess, onFailure) {
        callDB("/search", query, onSuccess, onFailure);
    }

    function collate(args, onSuccess, onFailure) {
        callDB("/collate", args, onSuccess, onFailure);
    }
}
