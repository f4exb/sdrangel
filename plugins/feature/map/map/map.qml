import QtQuick 2.12
import QtQuick.Window 2.12
import QtLocation 5.12
import QtPositioning 5.12

Item {
    id: qmlMap
    property int mapZoomLevel: 11

    Plugin {
        id: mapPlugin
        name: "osm"
    }

    Map {
        id: map
        objectName: "map"
        anchors.fill: parent
        plugin: mapPlugin
        center: QtPositioning.coordinate(51.5, 0.125) // London
        zoomLevel: 10


        MapItemView {
            model: mapModel
            delegate: mapComponent
        }

        MapStation {
            id: station
            objectName: "station"
            stationName: "Home"
            coordinate:  QtPositioning.coordinate(51.5, 0.125)
        }

        onZoomLevelChanged: {
            if (zoomLevel > 11) {
                station.zoomLevel = zoomLevel
                mapZoomLevel = zoomLevel
            } else {
                station.zoomLevel = 11
                mapZoomLevel = 11
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
            zoomLevel: mapImageFixedSize ? zoomLevel : mapZoomLevel

            sourceItem: Grid {
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
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: (mouse) => {
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
                        visible: mapTextVisible
                        Text {
                            id: text
                            anchors.centerIn: parent
                            text: mapText
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

}
