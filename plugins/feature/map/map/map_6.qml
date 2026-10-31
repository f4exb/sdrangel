import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtPositioning 6.5
import QtLocation 6.5

Item {
    id: qmlMap
    property real mapZoomLevel: 11
    property string mapProvider: "osm"
    property variant mapPtr
    property variant guiPtr
    property bool smoothing

    function createMap(pluginParameters, gui) {
        guiPtr = gui
        var paramString = ""
        for (var prop in pluginParameters) {
            var parameter = 'PluginParameter { name: "' + prop + '"; value: "' + pluginParameters[prop] + '"}'
            paramString = paramString + parameter
        }
        var pluginString = 'import QtLocation 6.5; Plugin{ name:"' + mapProvider + '"; '  + paramString + '}'
        var plugin = Qt.createQmlObject (pluginString, qmlMap)

        if (mapPtr) {
            // Objects aren't destroyed immediately, so don't call findChild("map")
            mapPtr.destroy()
            mapPtr = null
        }
        mapPtr = actualMapComponent.createObject(page)
        mapPtr.map.plugin = plugin;
        mapPtr.map.forceActiveFocus()
        return mapPtr
    }

    function getMapTypes() {
        var mapTypes = []
        if (mapPtr) {
            for (var i = 0; i < mapPtr.map.supportedMapTypes.length; i++) {
                mapTypes[i] = mapPtr.map.supportedMapTypes[i].name
            }
        }
        return mapTypes
    }

    function setMapType(mapTypeIndex) {
        if (mapPtr && (mapTypeIndex < mapPtr.map.supportedMapTypes.length)) {
            if (mapPtr.map.supportedMapTypes[mapTypeIndex] !== undefined) {
                mapPtr.map.activeMapType = mapPtr.map.supportedMapTypes[mapTypeIndex]
            }
        }
    }

    Item {
        id: page
        anchors.fill: parent
    }

    Component {
        id: actualMapComponent

        ModifiedMapView {
            id: mapView
            objectName: "mapView"
            anchors.fill: parent
            map.center: QtPositioning.coordinate(51.5, 0.125) // London
            map.zoomLevel: 10
            map.objectName: "map"

            // not in 6
            //gesture.enabled: true
            //gesture.acceptedGestures: MapGestureArea.PinchGesture | MapGestureArea.PanGesture

            MapItemView {
                model: imageModelFiltered
                delegate: imageComponent
                parent: mapView.map
            }

            MapItemView {
                model: polygonModelFiltered
                delegate: polygonComponent
                parent: mapView.map
            }

            MapItemView {
                model: polygonModelFiltered
                delegate: polygonNameComponent
                parent: mapView.map
            }

            MapItemView {
                model: polylineModelFiltered
                delegate: polylineComponent
                parent: mapView.map
            }

            MapItemView {
                model: polylineModelFiltered
                delegate: polylineTrackComponent
                parent: mapView.map
            }

            MapItemView {
                model: polylineModelFiltered
                delegate: polylineNameComponent
                parent: mapView.map
            }

            // Tracks first, so drawn under other items
            MapItemView {
                model: mapModelFiltered
                delegate: groundTrack1Component
                parent: mapView.map
            }

            MapItemView {
                model: mapModelFiltered
                delegate: groundTrack2Component
                parent: mapView.map
            }

            MapItemView {
                model: mapModelFiltered
                delegate: groundTrackSegmentsComponent
                parent: mapView.map
            }

            MapItemView {
                model: mapModelFiltered
                delegate: predictedGroundTrack1Component
                parent: mapView.map
            }

            MapItemView {
                model: mapModelFiltered
                delegate: predictedGroundTrack2Component
                parent: mapView.map
            }

            MapItemView {
                // Each MapItemView is a MapItemGroup, so it owns its delegates as children - a
                // delegate's z only ranks it among that view's own items. Ordering icons above
                // ground tracks therefore has to happen between the VIEWS, not inside them.
                z: 1
                model: mapModelFiltered
                delegate: mapComponent
                parent: mapView.map
            }

            map.onZoomLevelChanged: {
                mapZoomLevel = map.zoomLevel
                viewCoordinator.viewChanged(map.visibleRegion.boundingGeoRectangle(), map.zoomLevel);
            }

            // The map displays MapPolyLines in the wrong place (+360 degrees) if
            // they start to the left of the visible region, so we need to
            // split them so they don't, each time the visible region is changed. meh.
            map.onCenterChanged: {
                viewCoordinator.viewChanged(map.visibleRegion.boundingGeoRectangle(), map.zoomLevel);
            }

            map.onSupportedMapTypesChanged : {
                guiPtr.supportedMapsChanged()
            }

        }
    }

    function mapRect() {
        if (mapPtr)
            return mapPtr.map.visibleRegion.boundingGeoRectangle();
        else
            return null;
    }

    Component {
        id: imageComponent
        MapQuickItem {
            coordinate: position
            anchorPoint.x: imageId.width/2
            anchorPoint.y: imageId.height/2
            zoomLevel: imageZoomLevel
            sourceItem: Image {
                id: imageId
                source: imageData
            }
            autoFadeIn: false
        }
    }

    Component {
        id: polygonComponent
        MapPolygon {
            border.width: 1
            border.color: borderColor
            color: fillColor
            path: polygon
            autoFadeIn: false
        }
    }

    Component {
        id: polygonNameComponent
        MapQuickItem {
            coordinate: position
            anchorPoint.x: polygonText.width/2
            anchorPoint.y: polygonText.height/2
            zoomLevel: mapZoomLevel > 11 ? mapZoomLevel : 11
            sourceItem: Grid {
                columns: 1
                Grid {
                    layer.enabled: smoothing
                    layer.smooth: smoothing
                    horizontalItemAlignment: Grid.AlignHCenter
                    Text {
                        id: polygonText
                        text: label
                        textFormat: TextEdit.RichText
                    }
                }
            }
        }
    }

    Component {
        id: polylineComponent
        MapPolyline {
            line.width: 1
            line.color: lineColor
            path: coordinates
            autoFadeIn: false
        }
    }

    Component {
        id: polylineNameComponent
        MapQuickItem {
            coordinate: position
            anchorPoint.x: polylineText.width/2
            anchorPoint.y: polylineText.height/2
            zoomLevel: mapZoomLevel > 11 ? mapZoomLevel : 11
            sourceItem: Grid {
                columns: 1
                Grid {
                    layer.enabled: smoothing
                    layer.smooth: smoothing
                    horizontalItemAlignment: Grid.AlignHCenter
                    Text {
                        id: polylineText
                        text: label
                        textFormat: TextEdit.RichText
                    }
                }
            }
        }
    }

    Component {
        id: mapComponent
        MapQuickItem {
            id: mapElement
            z: zOrder                       // Move to front / move to back
            anchorPoint.x: imageBounds.width/2
            anchorPoint.y: imageBounds.height/2
            coordinate: position
            // when zooming, mapImageMinZoom can be temporarily undefined. Not sure why
            zoomLevel: (typeof mapImageMinZoom !== 'undefined') ? (mapZoomLevel > mapImageMinZoom ? mapZoomLevel : mapImageMinZoom) : zoomLevel
            autoFadeIn: false               // not in 5.12


            // A context menu per item is most of what an item costs to show, and it is
            // only ever needed for the one item that gets right-clicked, so it is built
            // on demand rather than for every item on the map
            Component {
                id: contextMenuComponent
                Item {
                    property alias contextMenu: contextMenu
                    property alias menus: menus
                    property alias menuItems: menuItems
                    property alias coordsMenuItem: coordsMenuItem
                    ListModel {
                        id: menus
                    }
                    ListModel {
                        id: menuItems
                    }
                    Menu {
                        id: contextMenu
                        MenuItem {
                            text: "Set as target"
                            onTriggered: target = true
                        }
                        MenuItem {
                            text: "Move to front"
                            onTriggered: mapModel.moveToFront(mapModelFiltered.mapRowToSource(index))
                        }
                        MenuItem {
                            text: "Move to back"
                            onTriggered: mapModel.moveToBack(mapModelFiltered.mapRowToSource(index))
                        }
                        MenuItem {
                            text: "Track on 3D map"
                            onTriggered: mapModel.track3D(mapModelFiltered.mapRowToSource(index))
                        }
                        MenuItem {
                            id: coordsMenuItem
                            text: ""
                        }
                        Instantiator {
                            model: menus
                            delegate: Menu {
                                //cascade: true
                                id: contextSubMenu
                                title: model.title
                            }
                            onObjectAdded: function(index, object) {
                                contextMenu.insertMenu(index, object)
                            }
                            onObjectRemoved: function(index, object) {
                                contextMenu.removeMenu(object)
                            }
                        }
                        Instantiator {
                            model: menuItems
                            delegate: MenuItem {
                                text: model.text
                                onTriggered: mapModel.setFrequency(model.frequency, model.deviceSet)
                            }
                            onObjectAdded: function(index, object) {
                                // index is index in to menuItems model
                                // object is the MenuItem
                                var menuItem = menuItems.get(index)
                                var menu = menus.get(menuItem.menuIndex)
                                contextMenu.menuAt(menuItem.menuIndex).insertItem(index, object)
                            }
                            onObjectRemoved: function(index, object) {
                                // Can't use menuItems.get(index) here, as already removed from model
                                object.menu.removeItem(object)
                            }
                        }
                    }
                }
            }

            Loader {
                id: contextMenuLoader
                active: false
            }

            // Populate and show the menu for this item, creating it the first time
            function showContextMenu(item, mouseX, mouseY) {
                if (!contextMenuLoader.active) {
                    contextMenuLoader.sourceComponent = contextMenuComponent
                    contextMenuLoader.active = true
                }
                var menu = contextMenuLoader.item
                menu.menuItems.clear()
                menu.menus.clear()
                if (frequencies.length > 0) {
                    var deviceSets = mapModel.getDeviceSets()
                    for (var i = 0; i < deviceSets.length; i++) {
                        menu.menus.append({
                            title: "Set " + deviceSets[i] + " to...",
                            deviceSet: i
                        })
                        for (var j = 0; j < frequencies.length; j++) {
                            menu.menuItems.append({
                                text: frequencyStrings[j],
                                frequency: frequencies[j],
                                deviceSet: deviceSets[i],
                                menuIndex: i
                            })
                        }
                    }
                }
                var c = mapPtr.map.toCoordinate(Qt.point(mouseX, mouseY))
                menu.coordsMenuItem.text = "Coords: " + c.latitude.toFixed(6) + ", " + c.longitude.toFixed(6)
                var menuParent = menu.contextMenu.parent
                var p = menuParent ? menuParent.mapFromItem(item, mouseX, mouseY)
                                   : item.mapToItem(null, mouseX, mouseY)
                menu.contextMenu.popup(p.x, p.y)
            }

            sourceItem: Grid {
                id: gridItem
                columns: 1
                Grid {
                    horizontalItemAlignment: Grid.AlignHCenter
                    columnSpacing: 5
                    layer.enabled: smoothing
                    layer.smooth: smoothing
                    // Sized to the rotated image's bounding box, so the smoothing layer
                    // doesn't clip wingtips and the bubble sits close at every rotation
                    Item {
                        id: imageBounds
                        width: image.implicitWidth * Math.abs(Math.cos(mapImageRotation * Math.PI / 180)) + image.implicitHeight * Math.abs(Math.sin(mapImageRotation * Math.PI / 180))
                        height: image.implicitWidth * Math.abs(Math.sin(mapImageRotation * Math.PI / 180)) + image.implicitHeight * Math.abs(Math.cos(mapImageRotation * Math.PI / 180))
                        Image {
                            id: image
                            anchors.centerIn: parent
                            rotation: mapImageRotation
                            source: mapImage
                            visible: mapImageVisible
                            MouseArea {
                                id: imageMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: (mouse) => {
                                    if (mouse.button === Qt.LeftButton) {
                                        selected = !selected
                                        if (selected) {
                                            mapModel.moveToFront(mapModelFiltered.mapRowToSource(index))
                                        }
                                    } else if (mouse.button === Qt.RightButton) {
                                        mapElement.showContextMenu(imageMouseArea, mouse.x, mouse.y)
                                    }
                                }
                            }
                        }
                    }
                    Rectangle {
                        id: bubble
                        color: bubbleColour
                        border.width: 1
                        width: text.width + 5
                        height: text.height + 5
                        radius: 5
                        visible: mapTextVisible
                        MouseArea {
                            id: bubbleMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: (mouse) => {
                                if (mouse.button === Qt.LeftButton) {
                                    selected = !selected
                                    if (selected) {
                                        mapModel.moveToFront(mapModelFiltered.mapRowToSource(index))
                                    }
                                } else if (mouse.button === Qt.RightButton) {
                                    mapElement.showContextMenu(bubbleMouseArea, mouse.x, mouse.y)
                                }
                            }
                        }
                        // Have Text after MouseArea, so links can be clicked
                        Text {
                            id: text
                            anchors.centerIn: parent
                            text: mapText
                            textFormat: TextEdit.RichText
                            onLinkActivated: {
                                console.log("Link", link);
                                mapModel.link(link);
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: predictedGroundTrack1Component
        MapPolyline {
            z: -0.5                         // Under every icon, which sit at 0 or above
            line.width: 2
            line.color: predictedGroundTrackColor
            path: predictedGroundTrack1
            autoFadeIn: false
        }
    }

    // Part of the line that crosses edge of map
    Component {
        id: predictedGroundTrack2Component
        MapPolyline {
            z: -0.5                         // Under every icon, which sit at 0 or above
            line.width: 2
            line.color: predictedGroundTrackColor
            path: predictedGroundTrack2
            autoFadeIn: false
        }
    }

    Component {
        id: groundTrack1Component
        MapPolyline {
            z: -0.5                         // Under every icon, which sit at 0 or above
            line.width: 2
            line.color: groundTrackColor
            path: groundTrack1
            autoFadeIn: false
        }
    }

    // Part of the line that crosses edge of map
    Component {
        id: groundTrack2Component
        MapPolyline {
            z: -0.5                         // Under every icon, which sit at 0 or above
            line.width: 2
            line.color: groundTrackColor
            path: groundTrack2
            autoFadeIn: false
        }
    }

    // A MapPolyline can only be one colour, so a ground track that's coloured according
    // to altitude is drawn as a series of them
    Component {
        id: groundTrackSegmentComponent
        MapPolyline {
            z: -0.5                         // Under every icon, which sit at 0 or above
            line.width: 2
            line.color: segmentColor
            path: segmentCoordinates
            autoFadeIn: false
        }
    }

    Component {
        id: groundTrackSegmentsComponent
        MapItemGroup {
            z: -0.5                         // Under every icon, which sit at 0 or above
            MapItemView {
                model: groundTrackModel
                delegate: groundTrackSegmentComponent
            }
        }
    }

    // A line sent as a track (item type 4) is coloured by the altitude of its points, so
    // like a ground track it has to be drawn as a series of lines rather than as one
    Component {
        id: polylineTrackComponent
        MapItemGroup {
            z: -0.5                         // Under every icon, which sit at 0 or above
            MapItemView {
                model: polylineTrackModel
                delegate: groundTrackSegmentComponent
            }
        }
    }

}
