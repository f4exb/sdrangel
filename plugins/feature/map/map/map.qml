import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtLocation 5.14
import QtPositioning 5.14

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
        var pluginString = 'import QtLocation 5.14; Plugin{ name:"' + mapProvider + '"; '  + paramString + '}'
        var plugin = Qt.createQmlObject (pluginString, qmlMap)

        if (mapPtr) {
            // Objects aren't destroyed immediately, so don't call findChild("map")
            mapPtr.destroy()
            mapPtr = null
        }
        mapPtr = actualMapComponent.createObject(page)
        mapPtr.plugin = plugin;
        mapPtr.forceActiveFocus()
        return mapPtr
    }

    function getMapTypes() {
        var mapTypes = []
        if (mapPtr) {
            for (var i = 0; i < mapPtr.supportedMapTypes.length; i++) {
                mapTypes[i] = mapPtr.supportedMapTypes[i].name
            }
        }
        return mapTypes
    }

    function setMapType(mapTypeIndex) {
        if (mapPtr && (mapTypeIndex < mapPtr.supportedMapTypes.length)) {
            if (mapPtr.supportedMapTypes[mapTypeIndex] !== undefined) {
                mapPtr.activeMapType = mapPtr.supportedMapTypes[mapTypeIndex]
            }
        }
    }

    Item {
        id: page
        anchors.fill: parent
    }

    Component {
        id: actualMapComponent

        Map {
            id: map
            objectName: "map"
            anchors.fill: parent
            center: QtPositioning.coordinate(51.5, 0.125) // London
            zoomLevel: 10
            gesture.enabled: true
            gesture.acceptedGestures: MapGestureArea.PinchGesture | MapGestureArea.PanGesture

            MapItemView {
                model: imageModelFiltered
                delegate: imageComponent
            }

            MapItemView {
                model: polygonModelFiltered
                delegate: polygonComponent
            }

            MapItemView {
                model: polygonModelFiltered
                delegate: polygonNameComponent
            }

            MapItemView {
                model: polylineModelFiltered
                delegate: polylineComponent
            }

            MapItemView {
                model: polylineModelFiltered
                delegate: polylineNameComponent
            }

            // Tracks first, so drawn under other items
            MapItemView {
                model: mapModelFiltered
                delegate: groundTrack1Component
            }

            MapItemView {
                model: mapModelFiltered
                delegate: groundTrack2Component
            }

            MapItemView {
                model: mapModelFiltered
                delegate: predictedGroundTrack1Component
            }

            MapItemView {
                model: mapModelFiltered
                delegate: predictedGroundTrack2Component
            }

            MapItemView {
                model: mapModelFiltered
                delegate: mapComponent
            }

            onZoomLevelChanged: {
                mapZoomLevel = zoomLevel
                mapModelFiltered.viewChanged(visibleRegion.boundingGeoRectangle().topLeft.longitude, visibleRegion.boundingGeoRectangle().topLeft.latitude, visibleRegion.boundingGeoRectangle().bottomRight.longitude, visibleRegion.boundingGeoRectangle().bottomRight.latitude, zoomLevel);
                imageModelFiltered.viewChanged(visibleRegion.boundingGeoRectangle().topLeft.longitude, visibleRegion.boundingGeoRectangle().topLeft.latitude, visibleRegion.boundingGeoRectangle().bottomRight.longitude, visibleRegion.boundingGeoRectangle().bottomRight.latitude, zoomLevel);
                polygonModelFiltered.viewChanged(visibleRegion.boundingGeoRectangle().topLeft.longitude, visibleRegion.boundingGeoRectangle().topLeft.latitude, visibleRegion.boundingGeoRectangle().bottomRight.longitude, visibleRegion.boundingGeoRectangle().bottomRight.latitude, zoomLevel);
                polylineModelFiltered.viewChanged(visibleRegion.boundingGeoRectangle().topLeft.longitude, visibleRegion.boundingGeoRectangle().topLeft.latitude, visibleRegion.boundingGeoRectangle().bottomRight.longitude, visibleRegion.boundingGeoRectangle().bottomRight.latitude, zoomLevel);
            }

            // The map displays MapPolyLines in the wrong place (+360 degrees) if
            // they start to the left of the visible region, so we need to
            // split them so they don't, each time the visible region is changed. meh.
            onCenterChanged: {
                polylineModelFiltered.viewChanged(visibleRegion.boundingGeoRectangle().topLeft.longitude, visibleRegion.boundingGeoRectangle().topLeft.latitude, visibleRegion.boundingGeoRectangle().bottomRight.longitude, visibleRegion.boundingGeoRectangle().bottomRight.latitude, zoomLevel);
                polygonModelFiltered.viewChanged(visibleRegion.boundingGeoRectangle().topLeft.longitude, visibleRegion.boundingGeoRectangle().topLeft.latitude, visibleRegion.boundingGeoRectangle().bottomRight.longitude, visibleRegion.boundingGeoRectangle().bottomRight.latitude, zoomLevel);
                imageModelFiltered.viewChanged(visibleRegion.boundingGeoRectangle().topLeft.longitude, visibleRegion.boundingGeoRectangle().topLeft.latitude, visibleRegion.boundingGeoRectangle().bottomRight.longitude, visibleRegion.boundingGeoRectangle().bottomRight.latitude, zoomLevel);
                mapModelFiltered.viewChanged(visibleRegion.boundingGeoRectangle().topLeft.longitude, visibleRegion.boundingGeoRectangle().topLeft.latitude, visibleRegion.boundingGeoRectangle().bottomRight.longitude, visibleRegion.boundingGeoRectangle().bottomRight.latitude, zoomLevel);
                mapModel.viewChanged(visibleRegion.boundingGeoRectangle().bottomLeft.longitude, visibleRegion.boundingGeoRectangle().bottomRight.longitude);
            }

            onSupportedMapTypesChanged : {
                guiPtr.supportedMapsChanged()
            }

        }
    }

    function mapRect() {
        if (mapPtr)
            return mapPtr.visibleRegion.boundingGeoRectangle();
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
                                    var c = mapPtr.toCoordinate(Qt.point(mouse.x, mouse.y))
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
                                    var c = mapPtr.toCoordinate(Qt.point(mouse.x, mouse.y))
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
