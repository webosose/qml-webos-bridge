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

import QtQuick 2.0
import WebOSServices 1.0
import PmLog 1.0

ListModel {
    id: listModel
    property string appId
    property var applicationManagerService: ApplicationManagerService {
        appId: listModel.appId
        property bool ready: connected && db8.didReadAppOrder

        Component.onCompleted: {
            listModel.status = ServiceModel.Loading;
        }

        onReadyChanged: {
            if (ready)
                applicationManagerService.subscribeLaunchPointsList();
        }

        onSameLaunchPointsListPublished: {
            var res = listModel.jsonSource;
            if (res.caseDetail !== undefined && res.caseDetail.change !== undefined && res.caseDetail.change.indexOf(updateAppsInSameListsAt) !== -1) {
                console.log("update contents of the appList with given same app lists when res.caseDetail.changes includes updateAppsInSameListsAt");
                appList = filter && res.launchPoints.filter(filter) || res.launchPoints;
                sortApps();
            }
        }
    }

    property string source: applicationManagerService.launchPointsList
    property var jsonSource: applicationManagerService.jsonLaunchPointsList

    property int status: ServiceModel.Null

    property var appList: [ ]
    property int defaultNewAppsIndex: 9 // default 10th position (zero-based)
    property int newAppsIndex: defaultNewAppsIndex
    property string updateAppsInSameListsAt: ""
    signal markedToKill(variant lpitem)
    signal updatedByLocaleChanged()
    signal updatedByAppTitleChanged(variant appId, variant newTitle)
    signal sorted()

    property variant pmLogLPM: PmLog { context: "LSM" }

    // user callback object (function)
    property var filter

    property var db8: DB8 {
        appId: listModel.appId
        id: db8
        property var kindId: "com.webos.launcher.appordering:1"
        property var record: { "apps": [], "useredit": undefined }
        property bool didReadAppOrder: false

        function initKind() {
            putKind({
                "id"      :  kindId,
                "owner"   :  appId,
                "indexes" :  [
                    { "name":"apps",
                        "props":[
                            {"name":"appId"},
                            {"name":"position"} ]
                    },
                    { "name":"useredit",
                        "props":[
                            {"name":"flag"} ]
                    }
                ]
            });

            var callerIds = ["com.webos.app.magicnum", "com.webos.app.tvquery", "com.webos.app.lpm.simulator"];
            for (var index in callerIds) {
                putPermissions({
                    "permissions" : [{
                        "operations" : { "read":"allow" },
                        "object"     : kindId,
                        "type"       : "db.kind",
                        "caller"     : callerIds[index]
                    } ]
                });
            }
        }

        function getAppOrder() {
            console.log("getAppOrder");

            function gotAppOrder(response) {
                listModel.pmLogLPM.debug("LAUNCHPOINTSORDER_RECEIVED_FROM_DB", {}, "", true);
                // keep our database clean: we only need one sort entry.
                if (response.results.length > 1) {
                    var toDelete = [];
                    for (var i = 1; i < response.results.length; i++) {
                        toDelete.push(response.results[i]._id);
                    }
                    console.info("deleting extra sort entries: " + JSON.stringify(toDelete));
                    del({"ids":toDelete}, function(){console.info("deleted extra entry.")});
                }
                if (response.results.length > 0) {
                    console.log("launchpoint order contains "+response.results[0].apps.length + " launchpoints")
                    record = response.results[0];
                    delete record._rev;
                    listModel.sortApps();
                } else {
                    console.warn("did not get launchPoint order from db8!");
                    storeAppOrder(); // Make sure "_id" field exist
                }
                // mark the app order as read
                didReadAppOrder = true;
            }

            function logError(response) {
                console.warn("error fetching app order: "+logError.errorText);
                // mark the app order as read
                didReadAppOrder = true;
            }

            find({ "query":{"from":kindId} }, gotAppOrder, logError);
        }

        function storeAppOrder(apps, useredit) {
            console.log("storeAppOrder record id: "+record._id);
            record._kind = kindId;
            if (apps != undefined)
                record.apps = apps;
            if (useredit != undefined)
                record.useredit = useredit;

            function logError(response) {
                console.warn("error storing app order: "+response.errorText);
            }

            function didMerge(response) {
                // remember the id so we can update this exact record later
                if (record._id == undefined) {
                    record._id = response.results[0].id;
                    console.info("new id: " + record._id);
                } else if (record._id != response.results[0].id) { // Paranoidal check
                    // Ignore different id
                    console.info("different id: " + record._id + " != " + response.results[0].id);
                }
            }
            // merge will update an existing entry or create a new one, returning the id of that record
            merge({ "objects":[record] }, didMerge, logError);
        }

        Component.onCompleted:{
            initKind();
            getAppOrder();
        }
    }

    function moveLaunchPoint(index, to) {
        move(index, to, 1);
    }

    function removeLaunchPoint(index) {
        remove(index);
        applicationManagerService.removeLaunchPoint(get(index).launchPointId);
    }

    function commitAppOrder(useredit) {
        var order = [];
        var i;
        for (i = 0; i < count; i++)
            order.push(get(i).launchPointId);

        console.log("commiting order: " +JSON.stringify(order));
        db8.storeAppOrder(order, useredit);
    }

    function resetAppOrder() {
        console.log("resetting order");
        db8.storeAppOrder([], false);
    }

    // qt problem? crash in qt5.2 qqmllistmodel.cpp:490 ListModel::set when we append
    // an object that contains an array received from luna-service, so we need to convert
    // it to an object, so it becomes it's own sub-ListModel.  This also creates a structure
    // that matches the simulation in DummyLaunchPointsModel.

    function normalizeApp(app) {
        if (app.bgImages && app.bgImages.length && !app.bgImages.count) {
            var tmp = [];
            for (var x = 0; x < app.bgImages.length; x++) {
                if (typeof app.bgImages[x] !== 'object')
                    tmp.push({ bgImage: app.bgImages[x] });
            }
            if (tmp.length)
                app.bgImages = tmp;
        }
        return app;
    }

    function sortModelByList(orderedList) {
        var i, j;
        for (i = 0; i < orderedList.length; i++) {
            for (j = i; j < count; j++) {
                if (get(j).launchPointId == orderedList[i].launchPointId) {
                    // existing lp: move it to the ordered position and overwrite properties
                    if (i != j) {
                        console.log("move " + orderedList[i].launchPointId + " from " + j + " to " + i);
                        move(j, i, 1);
                    }
                    set(i, orderedList[i]);
                    break;
                }
            }
            if (j >= count) {
                // not found lp: insert it
                console.log("insert " + orderedList[i].launchPointId + " at " + i);
                insert(i, orderedList[i]);
            }
        }

        // remove items not in orderedList
        if (count > i) {
            console.log("remove data from index " + i);
            remove(i, count-i);
        }
    }

    function sortApps() {
        if (appList.length == 0) {
            console.warn("did not get app list - nothing to sort yet.");
            return;
        }

        appList = appList.map(normalizeApp);

        var appOrder = db8.record.apps;
        if (appOrder.length == 0) {
            console.warn("did not get order - nothing to sort yet.");
            sortModelByList(appList);
            sorted(); // emit a signal to indicate the model is sorted
            // NOTE: Do not commitAppOrder()
            return;
        }

        var appById={};
        var sortedApps=[];
        var i;

        // put launchpoints in hash map by unique launchPoint id (not appId)
        for (i = 0; i < appList.length; i++)
            appById[appList[i].launchPointId] = appList[i];

        // for each sorted app id append launch point object
        for (i = 0; i < appOrder.length; i++) {
            if (!appById[appOrder[i]]) {
                // apps that went missing from SAMs list but are in the last sort list
                // they will not be in the list stored back to the DB
                console.log("launchPoint " + appOrder[i] + " not in SAM launchpoint list.");

                continue;
            }

            sortedApps.push(appById[  appOrder[i] ])
            appById[ appOrder[i] ] = undefined; // it works faster and delete sometimes gives corrupted appById
        }
        console.log("sorted " + sortedApps.length + " launchPoints ");

        sortModelByList(sortedApps);

        // apps that are not in the ordered list, but in sams list will be appended
        var indexToInsert = newAppsIndex;
        for (i in appById) {
            if (i === 'length' || appById[i] === undefined || !appById.hasOwnProperty(i))
                continue;

            console.log("app not in db8 sort list: " + i);
            insert(indexToInsert, appById[i]);
            indexToInsert++;
        }

        sorted(); // emit a signal to indicate the model is sorted

        commitAppOrder(); // sync to database to preserve current order (and forget removed items)
    }

    function getLaunchPointPositionByLaunchPointId(lpId) {
        for (var i = 0; i < count; i++) {
            var launchPoint = get(i);
            if (launchPoint.launchPointId !== lpId)
                continue;
            return i;
        }
        return -1;
    }

    function getNumber(n) {
        if (isNaN(n))
            return defaultNewAppsIndex;

        return parseInt(n);
    }

    function getValidPosition(pos, minValue, maxValue) {
        // Correct minValue and maxValue if needed
        if (minValue < 0)
            minValue = 0;
        if (maxValue > count)
            maxValue = count;
        if (pos !== undefined) {
            pos = getNumber(pos);
            // Correct pos if needed
            if (pos < minValue || pos >= maxValue) {
                pos = maxValue;
            } else if (get(pos) && get(pos).unmovable) {
                for (; pos < maxValue; ++pos) {
                    if (!get(pos).unmovable)
                        break;
                }
            }
        } else {
            if (defaultNewAppsIndex < minValue)
                pos = minValue;
            else if (defaultNewAppsIndex > maxValue)
                pos = maxValue;
            else
                pos = defaultNewAppsIndex;
        }
        return pos;
    }

    onJsonSourceChanged: {
        var res = listModel.jsonSource;
        var updateByLocaleChanged = false;
        var updateByAppTitleChanged = false;

        if (res.launchPoints === undefined) {
            // SAM may send differences between old and new app info
            // we should add or remove app info
            if (res.change === "added" || res.change === "removed") {
                // NOTE: No need to sync for "updated" case
                commitAppOrder(); // sync to database to preserve current order (and forget removed items)
            }

            if (res.change === "added") {
                newAppsIndex = listModel.getValidPosition(res.position, 0, appList.length);

                console.log("Add LaunchPoint : " + res.id);
                pmLogLPM.info("APPADDED_TO_LAUNCHER", {"APP_ID": res.id, "LOCATION": (newAppsIndex+1)}, "", true);

                // add app info
                appList.push(res);
            } else if (res.change === "removed") {
                // We got "launch point removed" notification
                console.log("Remove LaunchPoint : " + res.id);
                var locNmb = getLaunchPointPositionByLaunchPointId(res.launchPointId);

                markedToKill(res.id);

                // remove app info
                appList = appList.filter(function (launchPoint) {
                    return launchPoint.launchPointId !== res.launchPointId;
                });
            } else if (res.change === "updated") {
                console.log("Update LaunchPoint : " + res.id);
                var locNmb = getLaunchPointPositionByLaunchPointId(res.launchPointId);
                pmLogLPM.info("APPUPDATED_FROM_LAUNCHER", {"APP_ID": res.id, "LOCATION": locNmb}, "", true);
                if (res.position !== undefined && res.position !== locNmb) {
                    newAppsIndex = listModel.getValidPosition(res.position, 0, appList.length-1);
                    if (locNmb !== newAppsIndex) {
                        moveLaunchPoint(locNmb, newAppsIndex);
                        commitAppOrder();
                    }
                }
                // update app info
                appList.forEach(function (launchPoint, index, ar) {
                    if (launchPoint.launchPointId === res.launchPointId) {
                        if (ar[index].title !== res.title)
                            updateByAppTitleChanged = true;
                        ar[index] = res;
                    }
                });

                if (updateByAppTitleChanged)
                    updatedByAppTitleChanged(res.id, res.title);

            } else {
                console.warn("unhandled LaunchPointsList change : " + listModel.source);
                return;
            }
        } else {
            // SAM sends the full list *only* when: (per Nathan Seo)
            // 1) changed service country setting
            // 2) changed broadcast country setting
            // 3) changed language
            // 4) as the first response of listLaunchPoints request
            // For #1 and #2, we need to reset lp ordering with the full list.
            // For #3 and #4, we should keep lp ordering in db if exists.
            // So in this case we merge the full list with the lp ordering in db.

            appList = filter && res.launchPoints.filter(filter) || res.launchPoints;

            var needResetOrder = false;

            if (res.caseDetail !== undefined) {
                if (res.caseDetail.change !== undefined) {
                    if (res.caseDetail.change.indexOf("BROADCAST_COUNTRY") !== -1 ||
                            res.caseDetail.change.indexOf("SERVICE_COUNTRY") !== -1 ||
                            res.caseDetail.change.indexOf("NEWLIST_FROM_SDP") !== -1 )
                        needResetOrder = true;
                    else if (res.caseDetail.change.indexOf("LANG") !== -1)
                        updateByLocaleChanged = true;
                } else if (res.caseDetail.code !== undefined) {
                    console.warn("Fall-back to legacy code");
                    // NOTE: Legacy code cannot distinguash between broadcast and service country.

                    // code    description
                    // ----    -----------
                    // 1       first return of request
                    // 1001    change of language
                    // 1002    change of country
                    // 1003    change of both language and country
                    if (res.caseDetail.code === 1002 || res.caseDetail.code === 1003)
                        needResetOrder = true;
                    else if (res.caseDetail.code === 1001)
                        updateByLocaleChanged = true;
                }
            }

            if (needResetOrder) {
                if (db8.record.useredit !== true) {
                    // Case: User has never edited the app ordering.
                    // -> Clear app order information.
                    console.warn("db will be reset");
                    resetAppOrder();
                } else {
                    // Case: User has ever edited the app ordering.
                    // -> Do nothing.
                }
            }

            pmLogLPM.debug("LAUNCHPOINTSLIST_RECEIVED_FROM_SAM", {}, "", true);
        }

        console.log("received app list - sorting.");
        sortApps();

        if (listModel.status !== ServiceModel.Ready)
            listModel.status = ServiceModel.Ready;

        if (updateByLocaleChanged)
            updatedByLocaleChanged();
    }
}
