import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtLocation 5.12
import QtPositioning 5.12
import QtGraphicalEffects 1.12

Item {
    id: qmlMap
    property int aircraftZoomLevel: 11
    property int aircraftMinZoomLevel: 11
    property int airportZoomLevel: 11
    property string mapProvider: "osm"
    property variant mapPtr
    property string requestedMapType
    property bool lightIcons
    property variant guiPtr
    property bool smoothing

    function createMap(pluginParameters, requestedMap, gui) {
        requestedMapType = requestedMap
        guiPtr = gui

        var paramString = ""
        for (var prop in pluginParameters) {
            var parameter = 'PluginParameter { name: "' + prop + '"; value: "' + pluginParameters[prop] + '"}'
            paramString = paramString + parameter
        }
        var pluginString = 'import QtLocation 5.12; Plugin{ name:"' + mapProvider + '"; '  + paramString + '}'
        var plugin = Qt.createQmlObject (pluginString, qmlMap)

        if (mapPtr) {
            // Objects aren't destroyed immediately, so don't call findChild("map")
            mapPtr.destroy()
            mapPtr = null
        }
        mapPtr = actualMapComponent.createObject(page)
        mapPtr.plugin = plugin
        mapPtr.forceActiveFocus()
        return mapPtr
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

            // Needs to come first, otherwise MouseAreas in the MapItemViews don't get clicked event first
            // Setting z doesn't seem to work
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    // Unhighlight current aircraft
                    guiPtr.clearHighlighted()
                }
            }

            MapStation {
                id: station
                objectName: "station"
                stationName: "Home"
            }

            MapItemView {
                model: airspaceModel
                delegate: airspaceComponent
            }

            MapItemView {
                model: navAidModel
                delegate: navAidComponent
            }

            MapItemView {
                model: airspaceModel
                delegate: airspaceNameComponent
            }

            MapItemView {
                model: airportModel
                delegate: airportComponent
            }

            // This needs to be before aircraftComponent MapItemView, so it's drawn underneath
            MapItemView {
                model: aircraftModel
                delegate: aircraftPathComponent
            }

            MapItemView {
                model: aircraftModel
                delegate: aircraftComponent
            }

            onZoomLevelChanged: {
                if (zoomLevel > aircraftMinZoomLevel) {
                    aircraftZoomLevel = zoomLevel
                } else {
                    aircraftZoomLevel = aircraftMinZoomLevel
                }
                if (zoomLevel > 11) {
                    station.zoomLevel = zoomLevel
                    airportZoomLevel = zoomLevel
                } else {
                    station.zoomLevel = 11
                    airportZoomLevel = 11
                }
            }

            onSupportedMapTypesChanged : {
                for (var i = 0; i < supportedMapTypes.length; i++) {
                    if (requestedMapType == supportedMapTypes[i].name) {
                        activeMapType = supportedMapTypes[i]
                    }
                }
                lightIcons = (requestedMapType == "Night Transit Map") || (requestedMapType == "mapbox://styles/mapbox/dark-v9")
            }

        }
    }

    Component {
        id: navAidComponent
        MapQuickItem {
            id: navAid
            anchorPoint.x: image.width/2
            anchorPoint.y: image.height/2
            coordinate: position
            zoomLevel: airportZoomLevel

            sourceItem: Grid {
                columns: 1
                Grid {
                    horizontalItemAlignment: Grid.AlignHCenter
                    columnSpacing: 5
                    layer.enabled: smoothing
                    layer.smooth: smoothing
                    Image {
                        id: image
                        source: navAidImage
                        visible: !lightIcons
                        MouseArea {
                            anchors.fill: parent
                            onClicked: (mouse) => {
                                selected = !selected
                            }
                        }
                    }
                    ColorOverlay {
                        cached: true
                        width: image.width
                        height: image.height
                        source: image
                        color: "#c0ffffff"
                        visible: lightIcons
                    }
                    Rectangle {
                        id: bubble
                        color: bubbleColour
                        border.width: 1
                        width: text.width + 5
                        height: text.height + 5
                        radius: 5
                        Text {
                            id: text
                            anchors.centerIn: parent
                            text: navAidData
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: (mouse) => {
                                selected = !selected
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: airspaceComponent
        MapPolygon {
            border.width: 1
            border.color: airspaceBorderColor
            color: airspaceFillColor
            path: airspacePolygon
        }
    }

    Component {
        id: airspaceNameComponent
        MapQuickItem {
            coordinate: position
            anchorPoint.x: airspaceText.width/2
            anchorPoint.y: airspaceText.height/2
            zoomLevel: airportZoomLevel
            sourceItem: Grid {
                columns: 1
                Grid {
                    layer.enabled: smoothing
                    layer.smooth: smoothing
                    horizontalItemAlignment: Grid.AlignHCenter
                    Text {
                        id: airspaceText
                        text: details
                    }
                }
            }
        }
    }

    Component {
        id: aircraftPathComponent
        MapPolyline {
            line.width: 2
            line.color: 'gray'
            path: aircraftPath
        }
    }

    Component {
        id: aircraftComponent
        MapQuickItem {
            id: aircraft
            anchorPoint.x: image.width/2
            anchorPoint.y: image.height/2
            coordinate: position
            zoomLevel: aircraftZoomLevel

            sourceItem: Grid {
                columns: 1
                Grid {
                    layer.enabled: smoothing
                    layer.smooth: smoothing
                    horizontalItemAlignment: Grid.AlignHCenter
                    Image {
                        id: image
                        rotation: heading
                        source: aircraftImage
                        visible: !lightIcons
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: {
                                if (mouse.button === Qt.LeftButton) {
                                    highlighted = true
                                } else if (mouse.button === Qt.RightButton) {
                                    contextMenu.popup()
                                }
                            }
                            onDoubleClicked: {
                                target = true
                            }
                        }
                    }
                    ColorOverlay {
                        cached: true
                        width: image.width
                        height: image.height
                        rotation: heading
                        source: image
                        color: "#c0ffffff"
                        visible: lightIcons
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                highlighted = true
                            }
                            onDoubleClicked: {
                                target = true
                            }
                        }
                    }
                    Rectangle {
                        id: bubble
                        color: bubbleColour
                        border.width: 1
                        width: text.width * 1.1
                        height: text.height * 1.1
                        radius: 5
                        Text {
                            id: text
                            anchors.centerIn: parent
                            text: adsbData
                            textFormat: TextEdit.RichText
                        }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: {
                                if (mouse.button === Qt.LeftButton) {
                                    showAll = !showAll
                                } else if (mouse.button === Qt.RightButton) {
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
                                    text: "Find on feature map"
                                    onTriggered: aircraftModel.findOnMap(index)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: airportComponent
        MapQuickItem {
            id: aircraft
            anchorPoint.x: image.width/2
            anchorPoint.y: image.height/2
            coordinate: position
            zoomLevel: airportZoomLevel

            sourceItem: Grid {
                columns: 1
                Grid {
                    horizontalItemAlignment: Grid.AlignHCenter
                    layer.enabled: smoothing
                    layer.smooth: smoothing
                    Image {
                        id: image
                        source: airportImage
                        visible: !lightIcons
                    }
                    ColorOverlay {
                        cached: true
                        width: image.width
                        height: image.height
                        source: image
                        color: "#c0ffffff"
                        visible: lightIcons
                    }
                    Rectangle {
                        id: bubble
                        color: bubbleColour
                        border.width: 1
                        width: text.width + 5
                        height: text.height + 5
                        radius: 5
                        Text {
                            id: text
                            anchors.centerIn: parent
                            text: airportData
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: (mouse) => {
                                if (showFreq) {
                                    var freqIdx = Math.floor((mouse.y-5)/((height-10)/airportDataRows))
                                    if (freqIdx == 0) {
                                        showFreq = false
                                    }
                                } else {
                                   showFreq = true
                                }
                            }
                            onDoubleClicked: (mouse) => {
                                if (showFreq) {
                                    var freqIdx = Math.floor((mouse.y-5)/((height-10)/airportDataRows))
                                    if (freqIdx != 0) {
                                        selectedFreq = freqIdx - 1
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

}
