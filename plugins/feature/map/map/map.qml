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
        id: mapComponent
        MapQuickItem {
            id: mapElement
            anchorPoint.x: image.width/2
            anchorPoint.y: image.height/2
            coordinate: position
            zoomLevel: mapZoomLevel > mapImageMinZoom ? mapZoomLevel : mapImageMinZoom
            autoFadeIn: false               // not in 5.12

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
                                MenuItem {
                                    text: "Track on 3D map"
                                    onTriggered: mapModel.track3D(index)
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
