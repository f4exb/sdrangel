import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtPositioning 6.5
import QtLocation 6.5

Item {
    id: qmlMap
    property int mapZoomLevel: 11
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
                delegate: predictedGroundTrack1Component
                parent: mapView.map
            }

            MapItemView {
                model: mapModelFiltered
                delegate: predictedGroundTrack2Component
                parent: mapView.map
            }

            MapItemView {
                model: mapModelFiltered
                delegate: mapComponent
                parent: mapView.map
            }

            map.onZoomLevelChanged: {
                mapZoomLevel = map.zoomLevel
                mapModelFiltered.viewChanged(map.visibleRegion.boundingGeoRectangle().topLeft.longitude, map.visibleRegion.boundingGeoRectangle().topLeft.latitude, map.visibleRegion.boundingGeoRectangle().bottomRight.longitude, map.visibleRegion.boundingGeoRectangle().bottomRight.latitude, map.zoomLevel);
                imageModelFiltered.viewChanged(map.visibleRegion.boundingGeoRectangle().topLeft.longitude, map.visibleRegion.boundingGeoRectangle().topLeft.latitude, map.visibleRegion.boundingGeoRectangle().bottomRight.longitude, map.visibleRegion.boundingGeoRectangle().bottomRight.latitude, map.zoomLevel);
                polygonModelFiltered.viewChanged(map.visibleRegion.boundingGeoRectangle().topLeft.longitude, map.visibleRegion.boundingGeoRectangle().topLeft.latitude, map.visibleRegion.boundingGeoRectangle().bottomRight.longitude, map.visibleRegion.boundingGeoRectangle().bottomRight.latitude, map.zoomLevel);
                polylineModelFiltered.viewChanged(map.visibleRegion.boundingGeoRectangle().topLeft.longitude, map.visibleRegion.boundingGeoRectangle().topLeft.latitude, map.visibleRegion.boundingGeoRectangle().bottomRight.longitude, map.visibleRegion.boundingGeoRectangle().bottomRight.latitude, map.zoomLevel);
            }

            // The map displays MapPolyLines in the wrong place (+360 degrees) if
            // they start to the left of the visible region, so we need to
            // split them so they don't, each time the visible region is changed. meh.
            map.onCenterChanged: {
                polylineModelFiltered.viewChanged(map.visibleRegion.boundingGeoRectangle().topLeft.longitude, map.visibleRegion.boundingGeoRectangle().topLeft.latitude, map.visibleRegion.boundingGeoRectangle().bottomRight.longitude, map.visibleRegion.boundingGeoRectangle().bottomRight.latitude, map.zoomLevel);
                polygonModelFiltered.viewChanged(map.visibleRegion.boundingGeoRectangle().topLeft.longitude, map.visibleRegion.boundingGeoRectangle().topLeft.latitude, map.visibleRegion.boundingGeoRectangle().bottomRight.longitude, map.visibleRegion.boundingGeoRectangle().bottomRight.latitude, map.zoomLevel);
                imageModelFiltered.viewChanged(map.visibleRegion.boundingGeoRectangle().topLeft.longitude, map.visibleRegion.boundingGeoRectangle().topLeft.latitude, map.visibleRegion.boundingGeoRectangle().bottomRight.longitude, map.visibleRegion.boundingGeoRectangle().bottomRight.latitude, map.zoomLevel);
                mapModelFiltered.viewChanged(map.visibleRegion.boundingGeoRectangle().topLeft.longitude, map.visibleRegion.boundingGeoRectangle().topLeft.latitude, map.visibleRegion.boundingGeoRectangle().bottomRight.longitude, map.visibleRegion.boundingGeoRectangle().bottomRight.latitude, map.zoomLevel);
                mapModel.viewChanged(map.visibleRegion.boundingGeoRectangle().bottomLeft.longitude, map.visibleRegion.boundingGeoRectangle().bottomRight.longitude);
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
            anchorPoint.x: image.width/2
            anchorPoint.y: image.height/2
            coordinate: position
            // when zooming, mapImageMinZoom can be temporarily undefined. Not sure why
            zoomLevel: (typeof mapImageMinZoom !== 'undefined') ? (mapZoomLevel > mapImageMinZoom ? mapZoomLevel : mapImageMinZoom) : zoomLevel
            autoFadeIn: false               // not in 5.12

            sourceItem: Grid {
                id: gridItem
                columns: 1
                Grid {
                    horizontalItemAlignment: Grid.AlignHCenter
                    columnSpacing: 5
                    layer.enabled: smoothing
                    layer.smooth: smoothing
                    Image {
                        id: image
                        rotation: mapImageRotation
                        source: mapImage
                        visible: mapImageVisible
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: {
                                if (mouse.button === Qt.LeftButton) {
                                    selected = !selected
                                    if (selected) {
                                        mapModel.moveToFront(mapModelFiltered.mapRowToSource(index))
                                    }
                                } else if (mouse.button === Qt.RightButton) {
                                    if (frequency > 0) {
                                        freqMenuItem.text = "Set frequency to " + frequencyString
                                        freqMenuItem.enabled = true
                                    } else {
                                        freqMenuItem.text = "No frequency available"
                                        freqMenuItem.enabled = false
                                    }
                                    var c = mapPtr.map.toCoordinate(Qt.point(mouse.x, mouse.y))
                                    coordsMenuItem.text = "Coords: " + c.latitude.toFixed(6) + ", " + c.longitude.toFixed(6)
                                    contextMenu.popup()
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
                        Text {
                            id: text
                            anchors.centerIn: parent
                            text: mapText
                            textFormat: TextEdit.RichText
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: {
                                if (mouse.button === Qt.LeftButton) {
                                    selected = !selected
                                    if (selected) {
                                        mapModel.moveToFront(mapModelFiltered.mapRowToSource(index))
                                    }
                                } else if (mouse.button === Qt.RightButton) {
                                    if (frequency > 0) {
                                        freqMenuItem.text = "Set frequency to " + frequencyString
                                        freqMenuItem.enabled = true
                                    } else {
                                        freqMenuItem.text = "No frequency available"
                                        freqMenuItem.enabled = false
                                    }
                                    var c = mapPtr.map.toCoordinate(Qt.point(mouse.x, mouse.y))
                                    coordsMenuItem.text = "Coords: " + c.latitude.toFixed(6) + ", " + c.longitude.toFixed(6)
                                    contextMenu.popup()
                                }
                            }
                            Menu {
                                id: contextMenu
                                MenuItem {
                                    text: "Set as target"
                                    onTriggered: target = true
                                }
                                MenuItem {
                                    id: freqMenuItem
                                    text: "Not set"
                                    onTriggered: mapModel.setFrequency(frequency)
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
            line.width: 2
            line.color: predictedGroundTrackColor
            path: predictedGroundTrack2
            autoFadeIn: false
        }
    }

    Component {
        id: groundTrack1Component
        MapPolyline {
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
            line.width: 2
            line.color: groundTrackColor
            path: groundTrack2
            autoFadeIn: false
        }
    }

}
