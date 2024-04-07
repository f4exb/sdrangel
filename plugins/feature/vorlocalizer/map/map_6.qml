import QtQuick 2.14
import QtQuick.Window 2.14
import QtLocation 6.5
import QtPositioning 6.5

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
        var pluginString = 'import QtLocation 6.5; Plugin{ name:"' + mapProvider + '"; '  + paramString + '}'
        var plugin = Qt.createQmlObject (pluginString, qmlMap)

        if (mapPtr) {
            // Objects aren't destroyed immediately, so don't call findChild("map")
            mapPtr.destroy()
            mapPtr = null
        }
        mapPtr = actualMapComponent.createObject(page)
        mapPtr.map.plugin = plugin
        mapPtr.map.forceActiveFocus()
        return mapPtr
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
                model: vorModel
                delegate: vorRadialComponent
                parent: mapView.map
            }

            MapStation {
                id: station
                objectName: "station"
                stationName: "Home"
            }

            MapItemView {
                model: vorModel
                delegate: vorComponent
                parent: mapView.map
            }

            map.onZoomLevelChanged: {
                if (map.zoomLevel > 11) {
                    station.zoomLevel = map.zoomLevel
                    vorZoomLevel = map.zoomLevel
                } else {
                    station.zoomLevel = 11
                    vorZoomLevel = 11
                }
            }

            map.onSupportedMapTypesChanged : {
                for (var i = 0; i < map.supportedMapTypes.length; i++) {
                    if (requestedMapType == map.supportedMapTypes[i].name) {
                        map.activeMapType = map.supportedMapTypes[i]
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
