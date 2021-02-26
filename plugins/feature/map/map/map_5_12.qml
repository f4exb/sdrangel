import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtLocation 5.12
import QtPositioning 5.12

Item {
    id: qmlMap
    property int mapZoomLevel: 11
    property string mapProvider: "osm"
    property variant mapParameters
    property variant mapPtr

    function createMap(pluginParameters) {
        var parameters = new Array()
        for (var prop in pluginParameters) {
            var parameter = Qt.createQmlObject('import QtLocation 5.6; PluginParameter{ name: "'+ prop + '"; value: "' + pluginParameters[prop]+'"}', qmlMap)
            parameters.push(parameter)
        }
        qmlMap.mapParameters = parameters

        var plugin
        if (mapParameters && mapParameters.length > 0)
            plugin = Qt.createQmlObject ('import QtLocation 5.12; Plugin{ name:"' + mapProvider + '"; parameters: qmlMap.mapParameters}', qmlMap)
        else
            plugin = Qt.createQmlObject ('import QtLocation 5.12; Plugin{ name:"' + mapProvider + '"}', qmlMap)
        if (mapPtr) {
            // Objects aren't destroyed immediately, so rename the old
            // map, so any C++ code that calls findChild("map") doesn't find
            // the old map
            mapPtr.objectName = "oldMap";
            mapPtr.destroy()
        }
        mapPtr = actualMapComponent.createObject(page)
        mapPtr.plugin = plugin;
        mapPtr.forceActiveFocus()
        mapPtr.objectName = "map";
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
        if (mapPtr)
            mapPtr.activeMapType = mapPtr.supportedMapTypes[mapTypeIndex]
    }

    Item {
        id: page
        anchors.fill: parent
    }

    Component {
        id: actualMapComponent

        Map {
            id: map
            anchors.fill: parent
            center: QtPositioning.coordinate(51.5, 0.125) // London
            zoomLevel: 10

            // Tracks first, so drawn under other items
            MapItemView {
                model: mapModel
                delegate: groundTrack1Component
            }

            MapItemView {
                model: mapModel
                delegate: groundTrack2Component
            }

            MapItemView {
                model: mapModel
                delegate: predictedGroundTrack1Component
            }

            MapItemView {
                model: mapModel
                delegate: predictedGroundTrack2Component
            }

            MapItemView {
                model: mapModel
                delegate: mapComponent
            }

            onZoomLevelChanged: {
                mapZoomLevel = zoomLevel
            }

            // The map displays MapPolyLines in the wrong place (+360 degrees) if
            // they start to the left of the visible region, so we need to
            // split them so they don't, each time the visible region is changed. meh.
            onCenterChanged: {
                mapModel.viewChanged(visibleRegion.boundingGeoRectangle().bottomLeft.longitude, visibleRegion.boundingGeoRectangle().bottomRight.longitude);
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
        id: mapComponent
        MapQuickItem {
            id: mapElement
            anchorPoint.x: image.width/2
            anchorPoint.y: image.height/2
            coordinate: position
            zoomLevel: mapZoomLevel > mapImageMinZoom ? mapZoomLevel : mapImageMinZoom
            //autoFadeIn: false               // not in 5.12

            sourceItem: Grid {
                id: gridItem
                columns: 1
                Grid {
                    horizontalItemAlignment: Grid.AlignHCenter
                    columnSpacing: 5
                    layer.enabled: true
                    layer.smooth: true
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
                                        mapModel.moveToFront(index)
                                    }
                                } else if (mouse.button === Qt.RightButton) {
                                    if (frequency > 0) {
                                        freqMenuItem.text = "Set frequency to " + frequencyString
                                        freqMenuItem.enabled = true
                                    } else {
                                        freqMenuItem.text = "No frequency available"
                                        freqMenuItem.enabled = false
                                    }
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
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: {
                                if (mouse.button === Qt.LeftButton) {
                                    selected = !selected
                                    if (selected) {
                                        mapModel.moveToFront(index)
                                    }
                                } else if (mouse.button === Qt.RightButton) {
                                    if (frequency > 0) {
                                        freqMenuItem.text = "Set frequency to " + frequencyString
                                        freqMenuItem.enabled = true
                                    } else {
                                        freqMenuItem.text = "No frequency available"
                                        freqMenuItem.enabled = false
                                    }
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
                                    onTriggered: mapModel.moveToFront(index)
                                }
                                MenuItem {
                                    text: "Move to back"
                                    onTriggered: mapModel.moveToBack(index)
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
        }
    }

    // Part of the line that crosses edge of map
    Component {
        id: predictedGroundTrack2Component
        MapPolyline {
            line.width: 2
            line.color: predictedGroundTrackColor
            path: predictedGroundTrack2
        }
    }

    Component {
        id: groundTrack1Component
        MapPolyline {
            line.width: 2
            line.color: groundTrackColor
            path: groundTrack1
        }
    }

    // Part of the line that crosses edge of map
    Component {
        id: groundTrack2Component
        MapPolyline {
            line.width: 2
            line.color: groundTrackColor
            path: groundTrack2
        }
    }

}
