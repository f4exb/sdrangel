#include <QCoreApplication>
#include <QDebug>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QRegularExpression>

#include "../../../../../sdrbase/util/csv.h"
#include "../../../../../sdrbase/util/units.h"

void amUK(QTextStream& out)
{
    QFile inFile("../TxParamsAM.csv");
    if (inFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
        QTextStream in(&inFile);

        QString error;
        QHash<QString, int> colIndexes = CSV::readHeader(in, {"Station", "Area", "Site", "Frequency", "Site Ht", "In-Use EMRP (kW)", "Lat", "Long"}, error);
        if (error.isEmpty())
        {
            int stationCol = colIndexes.value("Station");
            int areaCol = colIndexes.value("Area");
            int siteCol = colIndexes.value("Site");
            int freqCol = colIndexes.value("Frequency");
            int siteHeightCol = colIndexes.value("Site Ht");
            int emrpCol = colIndexes.value("In-Use EMRP (kW)");
            int latCol = colIndexes.value("Lat");
            int longCol = colIndexes.value("Long");

            QStringList cols;
            while (CSV::readRow(in, &cols))
            {
                QString id;
                if (cols[areaCol] != cols[siteCol]) {
                    id = cols[stationCol] + " " + cols[areaCol] + " " + cols[siteCol];
                } else {
                    id = cols[stationCol] + " " + cols[areaCol];
                }

                out << "AM,";
                out << id;
                out << ",";
                out << cols[stationCol];
                out << ",";
                out << (int)std::round(cols[freqCol].toDouble() * 1e3);
                out << ",";
                out << cols[latCol];
                out << ",";
                out << cols[longCol];
                out << ",";
                out << cols[siteHeightCol];
                out << ",";
                out << cols[emrpCol].remove(",").toDouble();
                out << ",,\n";
            }
        }
        else
        {
            qCritical() << error;
        }
    }
    else
    {
        qDebug() << "Failed to open am file";
    }
}

void fmUK(QTextStream& out)
{
    QFile inFile("../TxParamsVHF.csv");
    if (inFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
        QTextStream in(&inFile);

        QString error;
        QHash<QString, int> colIndexes = CSV::readHeader(in, {"Station", "Area", "Site", "Frequency", "Site Ht", "In-Use ERP/VP", "Lat", "Long"}, error);
        if (error.isEmpty())
        {
            int stationCol = colIndexes.value("Station");
            int areaCol = colIndexes.value("Area");
            int siteCol = colIndexes.value("Site");
            int freqCol = colIndexes.value("Frequency");
            int siteHeightCol = colIndexes.value("Site Ht");
            int emrpCol = colIndexes.value("In-Use ERP/VP");
            int latCol = colIndexes.value("Lat");
            int longCol = colIndexes.value("Long");

            QStringList cols;
            while (CSV::readRow(in, &cols))
            {
                QString id;
                if (cols[areaCol] != cols[siteCol]) {
                    id = cols[stationCol] + " " + cols[areaCol] + " " + cols[siteCol];
                } else {
                    id = cols[stationCol] + " " + cols[areaCol];
                }

                out << "FM,";
                out << id;
                out << ",";
                out << cols[stationCol];
                out << ",";
                out << (int)std::round(cols[freqCol].toDouble() * 1e6);
                out << ",";
                out << cols[latCol];
                out << ",";
                out << cols[longCol];
                out << ",";
                out << cols[siteHeightCol];
                out << ",";
                out << cols[emrpCol].remove(",").toDouble();
                out << ",,\n";
            }
        }
        else
        {
            qCritical() << error;
        }
    }
    else
    {
        qDebug() << "Failed to open vhf file";
    }
}


void dabUK(QTextStream& out)
{
    QFile inFile("../TxParamsDAB.csv");
    if (inFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
        QTextStream in(&inFile);

        QString error;
        QHash<QString, int> colIndexes = CSV::readHeader(in, {"Ensemble", "Transmitter Area", "Site", "Freq.", "TII Main Id (Hex)", "TII Sub Id (Hex)", "Site Height", "In-Use ERP Total", "Lat", "Long"}, error);
        if (error.isEmpty())
        {
            int ensembleCol = colIndexes.value("Ensemble");
            int areaCol = colIndexes.value("Transmitter Area");
            int siteCol = colIndexes.value("Site");
            int freqCol = colIndexes.value("Freq.");
            int tiiMainCol = colIndexes.value("TII Main Id (Hex)");
            int tiiSubCol = colIndexes.value("TII Sub Id (Hex)");
            int siteHeightCol = colIndexes.value("Site Height");
            int erpCol = colIndexes.value("In-Use ERP Total");
            int latCol = colIndexes.value("Lat");
            int longCol = colIndexes.value("Long");

            QStringList cols;
            while (CSV::readRow(in, &cols))
            {
                QString id;
                /*if (cols[areaCol] != cols[siteCol]) {
                    id = cols[ensembleCol] + " " + cols[areaCol] + " " + cols[siteCol];
                } else {
                    id = cols[ensembleCol] + " " + cols[areaCol];
                }*/
                id = cols[ensembleCol] + " " + cols[tiiMainCol] + cols[tiiSubCol];

                out << "DAB,";
                out << id;
                out << ",";
                out << cols[ensembleCol];
                out << ",";
                out << (int)std::round(cols[freqCol].toDouble() * 1e6);
                out << ",";
                out << cols[latCol];
                out << ",";
                out << cols[longCol];
                out << ",";
                out << cols[siteHeightCol];
                out << ",";
                out << cols[erpCol].remove(",").toDouble();
                out << ",";
                out << cols[tiiMainCol];
                out << ",";
                out << cols[tiiSubCol];
                out << "\n";
            }
        }
        else
        {
            qCritical() << error;
        }
    }
    else
    {
        qDebug() << "Failed to open dab file";
    }
}

void dabFrance(QTextStream& out)
{
    QFile inFile("../sites-DAB-TII-v0.10.csv");
    if (inFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
        QTextStream in(&inFile);

        QString error;
        QHash<QString, int> colIndexes = CSV::readHeader(in, {"Identifiant Multiplex", "CTA du site", "Commune d'implantation de l'émetteur", "Fréquence (MHz)", "MainId (décimal)", "SubId (décimal)", "Hauteur de l'antenne", "PAR de l'émetteur (dBW)", "Latitude", "Longitude"}, error, ';');
        if (error.isEmpty())
        {
            int ensembleCol = colIndexes.value("Identifiant Multiplex"); // "Ensemble"
            int areaCol = colIndexes.value("CTA du site"); // "Transmitter Area"
            int siteCol = colIndexes.value("Commune d'implantation de l'émetteur"); // "Site"
            int freqCol = colIndexes.value("Fréquence (MHz)"); // "Freq."
            int tiiMainCol = colIndexes.value("MainId (décimal)"); // "TII Main Id (Hex)"
            int tiiSubCol = colIndexes.value("SubId (décimal)"); // "TII Sub Id (Hex)"
            int siteHeightCol = colIndexes.value("Hauteur de l'antenne"); // "Site Height"
            int erpCol = colIndexes.value("PAR de l'émetteur (dBW)"); // In-Use ERP Total"
            int latCol = colIndexes.value("Latitude");
            int longCol = colIndexes.value("Longitude");

            QStringList cols;
            while (CSV::readRow(in, &cols, ';'))
            {
                QString id;
                QString ensemble = cols[ensembleCol].replace("_", " ");

                /*if (cols[areaCol] != cols[siteCol]) {
                    id = ensemble + " " + cols[areaCol] + " " + cols[siteCol];
                } else {
                    id = ensemble + " " + cols[areaCol];
                }*/
                id = ensemble + " " + cols[tiiMainCol] + cols[tiiSubCol];

                QString lat = cols[latCol];
                QString lon = cols[longCol];
                QString pos = lat + "," + lon;
                QString freq = cols[freqCol].replace(",", ".");
                double power = pow(10.0, cols[erpCol].replace(",",".").toDouble() / 10.0) / 1000.0; // dBw to kW
                power = std::round(power * 10.0) / 10.0;

                float latitude, longitude;
                if (!Units::stringToLatitudeAndLongitude(pos, latitude, longitude)) {
                    qDebug() << "Failed to parse " << pos;
                } else {
                    out << "DAB,";
                    out << id;
                    out << ",";
                    out << ensemble;
                    out << ",";
                    out << (int)std::round(freq.toDouble() * 1e6);
                    out << ",";
                    out << latitude;
                    out << ",";
                    out << longitude;
                    out << ",";
                    out << cols[siteHeightCol];
                    out << ",";
                    out << power;
                    out << ",";
                    out << QString("%1").arg(cols[tiiMainCol].toInt(), 2, 16, QChar('0'));
                    out << ",";
                    out << QString("%1").arg(cols[tiiSubCol].toInt(), 2, 16, QChar('0'));
                    out << "\n";
                }
            }
        }
        else
        {
            qCritical() << error;
        }
    }
    else
    {
        qDebug() << "Failed to open dab file";
    }
}



int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    qDebug() << "Running in " << QDir::currentPath();

    QFile outFile("../transmitters.csv");
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qCritical() << "Failed to open";
        exit(1);
    }

    QTextStream out(&outFile);

    // Power is ERP for FM/DAB and EMRP for AM
    out << "Type,Id,Name,Frequency (Hz),Latitude,Longitude,Altitude (m),Power,TII Main,TII Sub\n";

    amUK(out);
    fmUK(out);
    dabUK(out);
    dabFrance(out);

    qDebug() << "Done";

    //return a.exec();
    return 0;
}
