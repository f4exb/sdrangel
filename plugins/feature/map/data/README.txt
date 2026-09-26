UK transmitter data:

TxParamsAM.csv. TxParamsDAB.csv, TxParamsVHF.csv - https://www.ofcom.org.uk/spectrum/information/radio-tech-parameters

700-plan-clearance.xlsx - https://www.ofcom.org.uk/spectrum/information/transmitter-frequency

French transmitter data:

sites-DAB-TII-v0.10.csv - https://extranet.arcom.fr/radio/index.php


WMM (World Magnetic Model):

Download ERSI shapefiles from: https://data.ngdc.noaa.gov/geomag/wmm/wmm2025/shapefiles/
Open D_2025.shp in QGIS: https://qgis.org/
Layer > Open Attribute Table, Toggle Edit Mode, Delete unneeded contours (E.g. odd values).
Edit > Edit Geometry > Simplify feature, Draw box around all geometry, set tolerance to 0.1 and check reduction in number of verticies.
Layer > Save As...

QGIS geojson export doesn't support styles (See: https://github.com/qgis/QGIS/issues/21195) so manually edit the geojson and insert:

    "stroke": "#ff0000" properties to set color. Negative blue, positive red, 0 green.
    "stroke-width": 0.5,  for contours that aren't multiples of 10 and 1.0 for those that are.
