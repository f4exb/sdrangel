// Maidenhead grid for Cesium

const polylines = new Cesium.PolylineCollection();
const polylinesFine = new Cesium.PolylineCollection();

const labels = new Cesium.LabelCollection();
const labelsFine = new Cesium.LabelCollection();

const segLen = 2;

const gridMaterial = Cesium.Material.fromType('Color');
gridMaterial.uniforms.color = new Cesium.Color(0.6, 0.6, 0.6, 1.0);
const gridLabelColor = new Cesium.Color(1, 1, 1, 1.0);

function createLat() {
    for (var lon = -180; lon < 180; lon += 20) {
        const array = [];
        for (var lat = -90; lat <= 90; lat += segLen) {
            array.push(lon);
            array.push(lat);
        }
        polylines.add({
            positions: Cesium.Cartesian3.fromDegreesArray(array),
            width: 1,
            material: gridMaterial
        });

    }
}

function createLatFine() {
    for (var lon = -180; lon < 180; lon += 2) {
        if (lon % 20 != 0) {
            const array = [];
            for (var lat = -90; lat <= 90; lat += segLen) {
                array.push(lon);
                array.push(lat);
            }
            polylinesFine.add({
                positions: Cesium.Cartesian3.fromDegreesArray(array),
                width: 1,
                material: gridMaterial
            });
        }
    }
}

function createLon() {
    for (var lat = -80; lat <= 80; lat += 10) {
        const array = [];
        for (var lon = -180; lon <= 180; lon += segLen) {
            array.push(lon);
            array.push(lat);
        }
        polylines.add({
            positions: Cesium.Cartesian3.fromDegreesArray(array),
            width: 1,
            material: gridMaterial
        });
    }
}

function createLonFine() {
    for (var lat = -80; lat <= 80; lat += 1) {
        if (lat % 10 != 0) {
            const array = [];
            for (var lon = -180; lon <= 180; lon += segLen) {
                array.push(lon);
                array.push(lat);
            }
            polylinesFine.add({
                positions: Cesium.Cartesian3.fromDegreesArray(array),
                width: 1,
                material: gridMaterial
            });
        }
    }
}

function createLabels() {
    var latField = 'B'.charCodeAt(0);
    for (var lat = -80; lat <= 80; lat += 10) {
        var lonField = 'A'.charCodeAt(0);
        for (var lon = -180; lon < 180; lon += 20) {
            labels.add({
                position: Cesium.Cartesian3.fromDegrees(lon + 0.5, lat + 0.5, 0.0),
                text: String.fromCharCode(lonField) + String.fromCharCode(latField),
                fillColor: gridLabelColor,
                font: '20px sans-serif'
            });
            lonField++;
        }
        latField++;
    }
}

function createLabelsFine() {
    var latField = 'B'.charCodeAt(0);
    for (var lat = -80; lat <= 80; lat += 1) {
        var latSq = '0'.charCodeAt(0) + mod(lat, 10);
        var lonField = 'A'.charCodeAt(0);
        for (var lon = -180; lon < 180; lon += 2) {
            var lonSq = '0'.charCodeAt(0) + mod(lon, 20) / 2;
            labelsFine.add({
                position: Cesium.Cartesian3.fromDegrees(lon + 0.05, lat + 0.05, 0.0),
                text: String.fromCharCode(lonField) + String.fromCharCode(latField) + String.fromCharCode(lonSq) + String.fromCharCode(latSq),
                fillColor: gridLabelColor,
                font: '20px sans-serif'
            });
            if (mod(lon, 20) == 18) {
                lonField++;
            }
        }
        if (mod(lat, 10) == 9) {
            latField++;
        }
    }
}
function mod(n, m) {
    return ((n % m) + m) % m;
}

function showGridByDistance() {

    var cameraHeight = viewer.scene.ellipsoid.cartesianToCartographic(viewer.scene.camera.position).height;

    const showFine = cameraHeight < 1500000;

    polylinesFine.show = showFine;
    labels.show = !showFine;
    labelsFine.show = showFine;
}

function createGrid(viewer) {

    createLatFine();
    createLat();
    createLonFine();
    createLon();
    polylinesFine.show = false;
    viewer.scene.primitives.add(polylines);
    viewer.scene.primitives.add(polylinesFine);

    createLabels();
    createLabelsFine();
    labelsFine.show = false;
    viewer.scene.primitives.add(labels);
    viewer.scene.primitives.add(labelsFine);
}

function showGrid(show) {
    if (show) {
        if (polylines.length == 0) {
            createGrid(viewer);
        }
        polylines.show = true;
        viewer.clock.onTick.addEventListener(showGridByDistance);
    } else {
        viewer.clock.onTick.removeEventListener(showGridByDistance);
        polylines.show = false;
        polylinesFine.show = false;
        labels.show = false;
        labelsFine.show = false;
    }
}
