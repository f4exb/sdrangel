import QtQuick 2.4
import QtQuick.Window 2.4
import QtLocation 5.6
import QtPositioning 5.6

Item {
    id: qmlMap
    property int aircraftZoomLevel: 11

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

        MapStation {
            id: station
            objectName: "station"
            stationName: "Home"
            coordinate:  QtPositioning.coordinate(51.5, 0.125)
        }

        MapItemView{
            model: aircraftModel
            delegate: aircraftComponent
        }

        onZoomLevelChanged: {
            if (zoomLevel > 11) {
                station.zoomLevel = zoomLevel
                aircraftZoomLevel = zoomLevel
            } else {
                station.zoomLevel = 11
                aircraftZoomLevel = 11
            }
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
                    horizontalItemAlignment: Grid.AlignHCenter
                    Image {
                        id: image
                        rotation: heading
                        source: aircraftImage
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
                        }
                    }
                }
            }


        }
    }

}
