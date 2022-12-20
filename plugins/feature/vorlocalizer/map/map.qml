import QtQuick 2.12
import QtQuick.Window 2.12
import QtLocation 5.12
import QtPositioning 5.12

Item {
    id: qmlMap
    property int vorZoomLevel: 11
    property string mapProvider: "osm"
    property variant mapPtr
    property string requestedMapType
    property variant guiPtr

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
            plugin: mapPlugin
            center: QtPositioning.coordinate(51.5, 0.125) // London
            zoomLevel: 10
            gesture.enabled: true
            gesture.acceptedGestures: MapGestureArea.PinchGesture | MapGestureArea.PanGesture

            MapItemView {
                model: vorModel
                delegate: vorRadialComponent
            }

            MapStation {
                id: station
                objectName: "station"
                stationName: "Home"
            }

            MapItemView {
                model: vorModel
                delegate: vorComponent
            }

            onZoomLevelChanged: {
                if (zoomLevel > 11) {
                    station.zoomLevel = zoomLevel
                    vorZoomLevel = zoomLevel
                } else {
                    station.zoomLevel = 11
                    vorZoomLevel = 11
                }
            }

            onSupportedMapTypesChanged : {
                for (var i = 0; i < supportedMapTypes.length; i++) {
                    if (requestedMapType == supportedMapTypes[i].name) {
                        activeMapType = supportedMapTypes[i]
                    }
                }
            }
        }
    }

    Component {
        id: vorRadialComponent
        MapPolyline {
            line.width: 2
            line.color: 'gray'
            path: vorRadial
        }
    }

    Component {
        id: vorComponent
        MapQuickItem {
            id: vor
            anchorPoint.x: image.width/2
            anchorPoint.y: bubble.height/2
            coordinate: position
            zoomLevel: vorZoomLevel

            sourceItem: Grid {
                columns: 1
                Grid {
                    horizontalItemAlignment: Grid.AlignHCenter
                    verticalItemAlignment: Grid.AlignVCenter
                    columnSpacing: 5
                    layer.enabled: true
                    layer.smooth: true
                    Image {
                        id: image
                        source: vorImage
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onDoubleClicked: (mouse) => {
                                selected = !selected
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
                        Text {
                            id: text
                            anchors.centerIn: parent
                            text: vorData
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onDoubleClicked: (mouse) => {
                                selected = !selected
                            }
                        }
                    }
                }
            }
        }
    }

}
