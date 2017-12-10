#!/bin/sh

CODEGEN=/opt/install/swagger/swagger-codegen
SDRANGEL_SRC=/opt/build/sdrangel

${CODEGEN} generate -i api/swagger/swagger.yaml -l qt5cpp -c qt5cpp-config.json -o code/qt5
${CODEGEN} generate -i api/swagger/swagger.yaml -l html2 -c html2-config.json -o code/html2
cp -v code/html2/index.html ${SDRANGEL_SRC}/sdrbase/resources/
