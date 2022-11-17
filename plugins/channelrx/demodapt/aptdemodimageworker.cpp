///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <algorithm>

#include <QTime>
#include <QBuffer>
#include <QDebug>

#include "maincore.h"
#include "util/units.h"

#include "aptdemod.h"
#include "aptdemodimageworker.h"

#include "SWGMapItem.h"

MESSAGE_CLASS_DEFINITION(APTDemodImageWorker::MsgConfigureAPTDemodImageWorker, Message)
MESSAGE_CLASS_DEFINITION(APTDemodImageWorker::MsgSaveImageToDisk, Message)
MESSAGE_CLASS_DEFINITION(APTDemodImageWorker::MsgSetSatelliteName, Message)

APTDemodImageWorker::APTDemodImageWorker(APTDemod *aptDemod) :
    m_messageQueueToGUI(nullptr),
    m_aptDemod(aptDemod),
    m_sgp4(nullptr),
    m_running(false)
{
    for (int y = 0; y < APT_MAX_HEIGHT; y++)
    {
        m_image.prow[y] = new float[APT_PROW_WIDTH];
        m_tempImage.prow[y] = new float[APT_PROW_WIDTH];
    }

    resetDecoder();
}

APTDemodImageWorker::~APTDemodImageWorker()
{
    m_inputMessageQueue.clear();

    for (int y = 0; y < APT_MAX_HEIGHT; y++)
    {
        delete[] m_image.prow[y];
        delete[] m_tempImage.prow[y];
    }

    delete m_sgp4;
}

void APTDemodImageWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

void APTDemodImageWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
}

void APTDemodImageWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = false;
}

void APTDemodImageWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool APTDemodImageWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureAPTDemodImageWorker::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureAPTDemodImageWorker& cfg = (MsgConfigureAPTDemodImageWorker&) cmd;
        qDebug("APTDemodImageWorker::handleMessage: MsgConfigureAPTDemodImageWorker");
        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (MsgSaveImageToDisk::match(cmd))
    {
        saveImageToDisk();
        return true;
    }
    else if (MsgSetSatelliteName::match(cmd))
    {
        MsgSetSatelliteName& msg = (MsgSetSatelliteName&) cmd;
        m_satelliteName = msg.getSatelliteName();
        return true;
    }
    else if (APTDemod::MsgPixels::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        const APTDemod::MsgPixels& pixelsMsg = (APTDemod::MsgPixels&) cmd;
        const float *pixels = pixelsMsg.getPixels();
        processPixels(pixels);
        delete[] pixels;
        return true;
    }
    else if (APTDemod::MsgResetDecoder::match(cmd))
    {
        resetDecoder();
        return true;
    }
    else
    {
        return false;
    }
}

void APTDemodImageWorker::applySettings(const APTDemodSettings& settings, bool force)
{
    (void) force;

    bool callRecalcCoords = false;
    bool callProcessImage = false;

    if ((settings.m_cropNoise != m_settings.m_cropNoise) ||
        (settings.m_denoise != m_settings.m_denoise) ||
        (settings.m_linearEqualise != m_settings.m_linearEqualise) ||
        (settings.m_histogramEqualise != m_settings.m_histogramEqualise) ||
        (settings.m_precipitationOverlay != m_settings.m_precipitationOverlay) ||
        (settings.m_flip != m_settings.m_flip) ||
        (settings.m_channels != m_settings.m_channels) ||
        (settings.m_transparencyThreshold != m_settings.m_transparencyThreshold) ||
        (settings.m_opacityThreshold != m_settings.m_opacityThreshold) ||
        (settings.m_palettes != m_settings.m_palettes) ||
        (settings.m_palette != m_settings.m_palette) ||
        (settings.m_horizontalPixelsPerDegree != m_settings.m_horizontalPixelsPerDegree) ||
        (settings.m_verticalPixelsPerDegree != m_settings.m_verticalPixelsPerDegree))
    {
        // Call after settings have been applied
        callProcessImage = true;
    }

    if ((settings.m_satTimeOffset != m_settings.m_satTimeOffset) ||
         (settings.m_satYaw != m_settings.m_satYaw))
    {
        callRecalcCoords = true;
        callProcessImage = true;
    }

    if (!settings.m_decodeEnabled && m_settings.m_decodeEnabled)
    {
        // Decode complete - make sure we do a full image update
        // so we aren't left we unprocessed lines
        callProcessImage = true;
    }

    if (settings.m_palettes != m_settings.m_palettes)
    {
        // Load colour palettes
        m_palettes.clear();
        for (auto palette : settings.m_palettes)
        {
            QImage img;
            img.load(palette);
            if ((img.width() != 256) || (img.height() != 256)) {
                qWarning() << "APT colour palette " << palette << " is not 256x256 pixels - " << img.width() << "x" << img.height();
            }
            m_palettes.append(img);
        }
    }

    m_settings = settings;

    if (callRecalcCoords) {
        recalcCoords();
    }
    if (callProcessImage) {
        sendImageToGUI();
    }
}

void APTDemodImageWorker::resetDecoder()
{
    m_image.nrow = 0;
    m_tempImage.nrow = 0;
    m_greyImage = QImage(APT_IMG_WIDTH, APT_MAX_HEIGHT, QImage::Format_Grayscale8);
    m_greyImage.fill(0);
    m_colourImage = QImage(APT_IMG_WIDTH, APT_MAX_HEIGHT, QImage::Format_RGB888);
    m_colourImage.fill(0);
    m_satelliteName = "";
    m_satCoords.clear();
    m_pixelCoords.clear();
    delete m_sgp4;
    m_sgp4 = nullptr;
}

// Convert Qt QDataTime to QGP4 DateTime
static DateTime qDateTimeToDateTime(QDateTime qdt)
{
    QDateTime utc = qdt.toUTC();
    QDate date = utc.date();
    QTime time = utc.time();
    DateTime dt;
    dt.Initialise(date.year(), date.month(), date.day(), time.hour(), time.minute(), time.second(), time.msec() * 1000);
    return dt;
}

// Get heading in range [0,360)
static double normaliseHeading(double heading)
{
    return fmod(heading + 360.0, 360.0);
}

// Get longitude in range -180,180
static double normaliseLongitude(double lon)
{
    return fmod(lon + 540.0, 360.0) - 180.0;
}

// Calculate heading (azimuth) in degrees
double APTDemodImageWorker::calcHeading(CoordGeodetic from, CoordGeodetic to) const
{
    // From https://en.wikipedia.org/wiki/Azimuth Section In Geodesy
    double flattening = 1.0 / 298.257223563; // For WGS84 ellipsoid
    double eSq = flattening * (2.0 - flattening);
    double oneMinusESq = (1.0 - flattening) * (1.0 - flattening);

    double tl1 = tan(from.latitude);
    double tl2 = tan(to.latitude);
    double n1 = 1.0 + oneMinusESq * tl2 * tl2;
    double d1 = 1.0 + oneMinusESq * tl1 * tl1;

    double l = to.longitude - from.longitude;

    double alpha;

    if (from.latitude == 0.0)
    {
        alpha = atan2(sin(l), (oneMinusESq * tan(to.latitude)));
    }
    else
    {
        double lambda = oneMinusESq * tan(to.latitude) / tan(from.latitude) + eSq * sqrt(n1/d1);

        alpha = atan2(sin(l), ((lambda - cos(l)) * sin(from.latitude)));
    }

    double deg = Units::radiansToDegrees(alpha);
    if (!m_settings.m_northToSouth) {
        deg += 180.0;
    }
    deg = normaliseHeading(deg);
    return deg;
}

// CoordGeodetic are in radians. Distance in metres. Bearing in radians.
// https://www.movable-type.co.uk/scripts/latlong.html
// This approximates Earth as spherical. If we need more accurate algorithm, see:
// https://www.movable-type.co.uk/scripts/latlong-vincenty.html
static void calcRadialEndPoint(CoordGeodetic start, double distance, double bearing, CoordGeodetic &end)
{
    double earthRadius = 6378137.0; // At equator
    double delta = distance/earthRadius;
    end.latitude = std::asin(sin(start.latitude)*cos(delta) + cos(start.latitude)*sin(delta)*cos(bearing));
    end.longitude = start.longitude + std::atan2(sin(bearing)*sin(delta)*cos(start.latitude), cos(delta) - sin(start.latitude)*sin(end.latitude));
    end.longitude = normaliseLongitude(end.longitude);
}

void APTDemodImageWorker::calcPixelCoords(CoordGeodetic centreCoord, double heading)
{
    // Calculate coordinates of each pixel in a row (swath)
    // Assume satellite is at centre pixel, and project +-90 degrees from satellite heading
    // https://www.ncei.noaa.gov/pub/data/satellite/publications/podguides/N-15%20thru%20N-19/pdf/APPENDIX%20J%20Instrument%20Scan%20Properties.pdf
    // Swath for AVHRR/3 of 2926.6km at 833km altitude over spherical Earth
    // Some docs say resolution is 4.0km, but it varies as per fig 4.2.3-1 in:
    // https://www.ncei.noaa.gov/pub/data/satellite/publications/podguides/N-15%20thru%20N-19/pdf/2.1%20Section%204.0%20Real%20Time%20Data%20Systems%20for%20Local%20Users%20.pdf
    // TODO: Could try to adjust for altitude

    QVector<CoordGeodetic> pixelCoords(APT_CH_WIDTH);
    pixelCoords[APT_CH_WIDTH/2] = centreCoord;
    double heading1 = Units::degreesToRadians(heading + m_settings.m_satYaw + 90.0);
    double heading2 = Units::degreesToRadians(heading + m_settings.m_satYaw - 90.0);
    for (int i = 1; i <= APT_CH_WIDTH/2; i++)
    {
        double distance = i * 2926600.0/APT_CH_WIDTH;
        calcRadialEndPoint(centreCoord, distance, heading1, pixelCoords[APT_CH_WIDTH/2-i]);
        calcRadialEndPoint(centreCoord, distance, heading2, pixelCoords[APT_CH_WIDTH/2+i]);
    }

    if (m_settings.m_northToSouth) {
        m_pixelCoords.append(pixelCoords);
    } else {
        m_pixelCoords.prepend(pixelCoords);
    }
}

// Recalculate all pixel coordiantes as satTimeOffset or satYaw has changed
void APTDemodImageWorker::recalcCoords()
{
    if (m_sgp4)
    {
        m_satCoords.clear();
        m_pixelCoords.clear();
        for (int row = 0; row < m_image.nrow; row++)
        {
            QDateTime qdt = m_settings.m_aosDateTime.addMSecs(m_settings.m_satTimeOffset * 1000.0f + row * 500);
            calcCoords(qdt, row);
        }
    }
}

// Calculate pixel coordinates for a single row at the given date and time
void APTDemodImageWorker::calcCoords(QDateTime qdt, int row)
{
    try
    {
        DateTime dt = qDateTimeToDateTime(qdt);

        // Calculate satellite position
        Eci eci = m_sgp4->FindPosition(dt);

        // Convert satellite position to geodetic coordinates (lat and long)
        CoordGeodetic geo = eci.ToGeodetic();

        m_satCoords.append(geo);

        // Calculate satellite heading (Could convert eci.Velocity() instead)
        double heading;
        if (m_satCoords.size() == 2)
        {
            heading = calcHeading(m_satCoords[0], m_satCoords[1]);
            calcPixelCoords(m_satCoords[0], heading);
            calcPixelCoords(m_satCoords[1], heading);
        }
        else if (m_satCoords.size() > 2)
        {
            heading = calcHeading(m_satCoords[row-1], m_satCoords[row]);
            calcPixelCoords(geo, heading);
        }
    }
    catch (SatelliteException& se)
    {
        qDebug() << "APTDemodImageWorker::calcCoord: " << se.what();
    }
    catch (DecayedException& de)
    {
        qDebug() << "APTDemodImageWorker::calcCoord: " << de.what();
    }
    catch (TleException& tlee)
    {
        qDebug() << "APTDemodImageWorker::calcCoord: " << tlee.what();
    }
}


// Calculate satellite's geodetic coordinates and heading
void APTDemodImageWorker::calcCoord(int row)
{
    if (row == 0)
    {
        QStringList elements = m_settings.m_tle.trimmed().split("\n");
        if (elements.size() == 3)
        {
            // Initalise SGP4
            Tle tle(elements[0].toStdString(), elements[1].toStdString(), elements[2].toStdString());
            m_sgp4 = new SGP4(tle);

            // Output time so we can check time offset from when AOS is signalled
            qDebug() << "APTDemod: Processing row 0 at " << QDateTime::currentDateTime();

            calcCoords(m_settings.m_aosDateTime, row);
        }
        else
        {
            qDebug() << "APTDemodImageWorker::calcCoord: No TLE for satellite. Is Satellite Tracker running?";
            return;
        }
    }
    else if (m_sgp4 == nullptr)
    {
        return;
    }
    else
    {
        // Calculate time at which
        // Don't try to use QDateTime::currentDateTime() as processing & scheduling delays mean
        // it's not constant and can sometimes even be 0
        // Lines should be transmitted at 2 per second, so just use number of rows since AOS
        // We add a user-defined delay to account for delays in transferring SDR data and demodulation
        QDateTime qdt = m_settings.m_aosDateTime.addMSecs(m_settings.m_satTimeOffset * 1000.0f + row * 500);
        calcCoords(qdt, row);
    }
}

void APTDemodImageWorker::processPixels(const float *pixels)
{
    if (m_image.nrow < APT_MAX_HEIGHT)
    {
        // Calculate lat and lon of centre of row
        calcCoord(m_image.nrow);

        std::copy(pixels, pixels + APT_PROW_WIDTH, m_image.prow[m_image.nrow]);
        m_image.nrow++;

        if (m_image.nrow % m_settings.m_scanlinesPerImageUpdate == 0) { // send full image only every N lines
            sendImageToGUI();
        } else { // else send unprocessed line just to show stg is moving
            sendLineToGUI();
        }
    }
}

void APTDemodImageWorker::sendImageToGUI()
{
    // Send image to GUI
    if (m_messageQueueToGUI)
    {
        QStringList imageTypes;
        QImage image = processImage(imageTypes, m_settings.m_channels);
        m_messageQueueToGUI->push(APTDemod::MsgImage::create(image, imageTypes, m_satelliteName));
        if (m_sgp4) {
            sendImageToMap(image);
        }
    }
}

// Find the value of the pixel closest to the given coordinates
// If we have previously found a pixel, we constrain the search to be nearby, in order to speed up the search
QRgb APTDemodImageWorker::findNearest(const QImage &image, double latitude, double longitude, int xPrevious, int yPrevious, int &xNearest, int &yNearest) const
{
    double dmin = 360.0 * 360.0 + 90.0 * 90.0;
    xNearest = -1;
    yNearest = -1;
    QRgb p = qRgba(0, 0, 0, 0); // Transparent

    int xMin, xMax;
    int yMin, yMax;

    int yStartPostCrop;
    int yEndPostCrop;
    if (m_settings.m_northToSouth)
    {
        yStartPostCrop = 0;
        yEndPostCrop = yStartPostCrop + image.height();
    }
    else
    {
        yStartPostCrop = m_image.nrow - m_tempImage.nrow;
        yEndPostCrop = yStartPostCrop + image.height();
    }

    if (xPrevious == -1)
    {
        yMin = yStartPostCrop;
        yMax = yEndPostCrop;
        xMin = 0;
        xMax = m_pixelCoords[0].size();
    }
    else
    {
        int searchRadius = 4;
        yMin = yPrevious - searchRadius;
        yMax = yPrevious + searchRadius + 1;
        xMin = xPrevious - searchRadius;
        xMax = xPrevious + searchRadius + 1;
        yMin = std::max(yMin, yStartPostCrop);
        yMax = std::min(yMax, yEndPostCrop);
        xMin = std::max(xMin, 0);
        xMax = std::min(xMax, (int)m_pixelCoords[0].size());
    }

    const int ySize = yEndPostCrop-1;
    const int xSize = m_pixelCoords[0].size()-1;
    for (int y = yMin; y < yMax; y++)
    {
        for (int x = xMin; x < xMax; x++)
        {
            CoordGeodetic coord = m_pixelCoords[y][x];
            double dlat = coord.latitude - latitude;
            double dlon = coord.longitude - longitude;
            double d = dlat * dlat + dlon * dlon;
            if (d < dmin)
            {
                dmin = d;
                xNearest = x;
                yNearest = y;
                // Only use color of pixel if we're inside the source image
                if (   ((y != yStartPostCrop) || ((y == yStartPostCrop) && (latitude <= coord.latitude)))
                    && ((y != ySize)  || ((y == ySize)  && (latitude >= coord.latitude)))
                    && ((x != 0) || ((x == 0) && (longitude >= coord.longitude)))
                    && ((x != xSize) || ((x == xSize) && (longitude <= coord.longitude)))
                   )
                {
                    p = image.pixel(x, y - yStartPostCrop);
                }
                else
                {
                    p = qRgba(0, 0, 0, 0); // Transparent
                }
            }
        }
    }

    return p;
}

// Calculate bounding box for projected image in terms of latitude and longitude
// TODO: Handle crossing of anti-meridian
void APTDemodImageWorker::calcBoundingBox(double &east, double &south, double &west, double &north, const QImage &image)
{
    int start;
    if (m_settings.m_northToSouth) {
        start = 0;
    } else {
        start = m_image.nrow - m_tempImage.nrow;
    }
    int stop = start + image.height();

    east = -M_PI;
    west = M_PI;
    north = -M_PI/2.0;
    south = M_PI/2.0;

    //FILE *f = fopen("coords.txt", "w");
    for (int y = start; y < stop; y++)
    {
        for (int x = 0; x < m_pixelCoords[y].size(); x++)
        {
            double latitude = m_pixelCoords[y][x].latitude;
            double longitude = m_pixelCoords[y][x].longitude;
            //fprintf(f, "%f,%f ", Units::radiansToDegrees(m_pixelCoords[y][x].latitude), Units::radiansToDegrees(m_pixelCoords[y][x].longitude));
            south = std::min(latitude, south);
            north = std::max(latitude, north);
            east = std::max(longitude, east);
            west = std::min(longitude, west);
        }
        //fprintf(f, "\n");
    }
    //fclose(f);
}

// Project satellite image to equidistant cyclindrical projection (Plate Carree) for use on 3D Map
// We've previously computed lat and lon for each pixel in satellite image
// so we just work through coords in projected image, trying to find closest pixel in satellite image
// FIXME: How do we handle sat going over the poles?
QImage APTDemodImageWorker::projectImage(const QImage &image)
{
    double east, south, west, north;

    // Calculate bounding box for image tile
    calcBoundingBox(east, south, west, north, image);
    m_tileEast = ceil(Units::radiansToDegrees(east));
    m_tileWest = floor(Units::radiansToDegrees(west));
    m_tileNorth = ceil(Units::radiansToDegrees(north));
    m_tileSouth = floor(Units::radiansToDegrees(south));

    double widthDeg = m_tileEast - m_tileWest;
    double heightDeg = m_tileNorth - m_tileSouth;

    int width = widthDeg * m_settings.m_horizontalPixelsPerDegree;
    int height = heightDeg * m_settings.m_verticalPixelsPerDegree;

    //image.save("source.png");
    //FILE *f = fopen("mapping.txt", "w");
    QImage projection(width, height, QImage::Format_ARGB32);
    int xNearest, yNearest, xPrevious, yPrevious;
    xPrevious = -1;
    yPrevious = -1;
    for (int y = 0; y < height; y++)
    {
        // Calculate geodetic coords of pixel in projected image
        double lat = m_tileNorth - (y / (double)m_settings.m_verticalPixelsPerDegree);
        // Reverse search direction in alternate rows, so we are always seaching
        // close to previously found pixel
        if ((y & 1) == 0)
        {
            for (int x = 0; x < width; x++)
            {
                double lon = m_tileWest + (x / (double)m_settings.m_horizontalPixelsPerDegree);
                // Find closest pixel in source image
                QRgb pixel = findNearest(image, Units::degreesToRadians(lat), Units::degreesToRadians(lon), xPrevious, yPrevious, xNearest, yNearest);
                xPrevious = xNearest;
                yPrevious = yNearest;
                projection.setPixel(x, y, pixel);
                //fprintf(f, "%f,%f,%d,%d,%d ", lat, lon, xNearest, yNearest, pixel==0);
            }
        }
        else
        {
            for (int x = width - 1; x >= 0; x--)
            {
                double lon = m_tileWest + (x / (double)m_settings.m_horizontalPixelsPerDegree);
                // Find closest pixel in source image
                QRgb pixel = findNearest(image, Units::degreesToRadians(lat), Units::degreesToRadians(lon), xPrevious, yPrevious, xNearest, yNearest);
                xPrevious = xNearest;
                yPrevious = yNearest;
                projection.setPixel(x, y, pixel);
                //fprintf(f, "%f,%f,%d,%d,%d ", lat, lon, xNearest, yNearest, pixel==0);
            }
        }
        //fprintf(f, "\n");
    }
    //fclose(f);

    return projection;
}

// Make an image transparent, so when overlaid on 3D map, we can see the underlying terrain
// Image is full transparent below m_transparencyThreshold and fully opaque above m_opacityThreshold
void APTDemodImageWorker::makeTransparent(QImage &image)
{
    for (int y = 0; y < image.height(); y++)
    {
        for (int x = 0; x < image.width(); x++)
        {
            QRgb pixel = image.pixel(x, y);
            int grey = qGray(pixel);
            if (grey < m_settings.m_transparencyThreshold)
            {
                // Make fully transparent
                pixel = qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), 0);
                image.setPixel(x, y, pixel);
            }
            else if (grey < m_settings.m_opacityThreshold)
            {
                // Make slightly transparent
                float opacity = 1.0f - ((m_settings.m_opacityThreshold - grey) / (float)(m_settings.m_opacityThreshold - m_settings.m_transparencyThreshold));
                opacity = opacity * 255.0f;
                opacity = std::min(255.0f, opacity);
                opacity = std::max(0.0f, opacity);
                pixel = qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), (int)std::round(opacity));
                image.setPixel(x, y, pixel);
            }
        }
    }
}

void APTDemodImageWorker::sendImageToMap(QImage image)
{
    // Send to Map feature
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_aptDemod, "mapitems", mapPipes);

    if (mapPipes.size() > 0)
    {
        // Only display one channel on map
        QImage selectedChannel;
        if (m_settings.m_channels == APTDemodSettings::BOTH_CHANNELS) {
            selectedChannel = extractImage(image, APTDemodSettings::CHANNEL_B);
        } else {
            selectedChannel = image;
        }

        // Project image to geodetic coords (lat & lon)
        selectedChannel = projectImage(selectedChannel);
        //selectedChannel.save("projected.png");

        // Use alpha channel to remove land & sea
        makeTransparent(selectedChannel);

        // Encode image as base64 PNG
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        selectedChannel.save(&buffer, "PNG");
        QByteArray data = ba.toBase64();

        // Create name for the image
        QString satName = m_satelliteName;
        satName.replace(" ", "_");
        QString name = QString("apt_%1_%2").arg(satName).arg(m_settings.m_aosDateTime.toString("yyyyMMdd_hhmmss"));

        // Send name to GUI
        m_messageQueueToGUI->push(APTDemod::MsgMapImageName::create(name));

        for (const auto& pipe : mapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
            swgMapItem->setName(new QString(name));
            swgMapItem->setImage(new QString(data));
            swgMapItem->setAltitude(3000.0); // Typical cloud height - So it appears above objects on the ground
            swgMapItem->setType(1);
            swgMapItem->setImageTileEast(m_tileEast);
            swgMapItem->setImageTileWest(m_tileWest);
            swgMapItem->setImageTileNorth(m_tileNorth);
            swgMapItem->setImageTileSouth(m_tileSouth);

            MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_aptDemod, swgMapItem);
            messageQueue->push(msg);
        }
    }
}

void APTDemodImageWorker::sendLineToGUI()
{
    if (m_messageQueueToGUI)
    {
        float *pixels = m_image.prow[m_image.nrow-1];
        uchar *line;
        APTDemod::MsgLine *msg = APTDemod::MsgLine::create(&line);

        if (m_settings.m_channels == APTDemodSettings::BOTH_CHANNELS)
        {
            for (int i = 0; i < APT_IMG_WIDTH; i++) {
                line[i] = roundAndClip(pixels[i]);
            }
            msg->setSize(APT_IMG_WIDTH);
        }
        else if (m_settings.m_channels == APTDemodSettings::CHANNEL_A)
        {
            for (int i = 0; i < APT_CH_WIDTH; i++) {
                line[i] = roundAndClip(pixels[i + APT_CHA_OFFSET]);
            }
            msg->setSize(APT_CH_WIDTH);
        }
        else
        {
            for (int i = 0; i < APT_CH_WIDTH; i++) {
                line[i] = roundAndClip(pixels[i + APT_CHB_OFFSET]);
            }
            msg->setSize(APT_CH_WIDTH);
        }

        m_messageQueueToGUI->push(msg);
    }
}

QImage APTDemodImageWorker::processImage(QStringList& imageTypes, APTDemodSettings::ChannelSelection channels)
{
    copyImage(&m_tempImage, &m_image);

    // Calibrate channels according to wavelength (1.7x to stop flickering)
    if (m_tempImage.nrow >= 1.7 * APT_CALIBRATION_ROWS)
    {
        m_tempImage.chA = apt_calibrate(m_tempImage.prow, m_tempImage.nrow, APT_CHA_OFFSET, APT_CH_WIDTH);
        m_tempImage.chB = apt_calibrate(m_tempImage.prow, m_tempImage.nrow, APT_CHB_OFFSET, APT_CH_WIDTH);
        QStringList channelTypes({
            "",  // Unknown
            "Visible (0.58-0.68 um)",
            "Near-IR (0.725-1.0 um)",
            "Near-IR (1.58-1.64 um)",
            "Thermal-infrared (10.3-11.3 um)",
            "Thermal-infrared (11.5-12.5 um)",
            "Mid-infrared (3.55-3.93 um)"
            });

        imageTypes.append(channelTypes[m_tempImage.chA]);
        imageTypes.append(channelTypes[m_tempImage.chB]);
    }

    // Crop noise due to low elevation at top and bottom of image
    if (m_settings.m_cropNoise) {
        apt_cropNoise(&m_tempImage);
    }

    // Denoise filter
    if (m_settings.m_denoise)
    {
        apt_denoise(m_tempImage.prow, m_tempImage.nrow, APT_CHA_OFFSET, APT_CH_WIDTH);
        apt_denoise(m_tempImage.prow, m_tempImage.nrow, APT_CHB_OFFSET, APT_CH_WIDTH);
    }

    // Flip image if satellite pass is North to South
    if (m_settings.m_flip)
    {
        apt_flipImage(&m_tempImage, APT_CH_WIDTH, APT_CHA_OFFSET);
        apt_flipImage(&m_tempImage, APT_CH_WIDTH, APT_CHB_OFFSET);
    }

    // Linear equalise to improve contrast
    if (m_settings.m_linearEqualise)
    {
        apt_linearEnhance(m_tempImage.prow, m_tempImage.nrow, APT_CHA_OFFSET, APT_CH_WIDTH);
        apt_linearEnhance(m_tempImage.prow, m_tempImage.nrow, APT_CHB_OFFSET, APT_CH_WIDTH);
    }

    // Histogram equalise to improve contrast
    if (m_settings.m_histogramEqualise)
    {
        apt_histogramEqualise(m_tempImage.prow, m_tempImage.nrow, APT_CHA_OFFSET, APT_CH_WIDTH);
        apt_histogramEqualise(m_tempImage.prow, m_tempImage.nrow, APT_CHB_OFFSET, APT_CH_WIDTH);
    }

    if (m_settings.m_precipitationOverlay)
    {
        // Overlay precipitation
        for (int r = 0; r < m_tempImage.nrow; r++)
        {
            uchar *l = m_colourImage.scanLine(r);
            for (int i = 0; i < APT_IMG_WIDTH; i++)
            {
                float p = m_tempImage.prow[r][i];

                if ((i >= APT_CHB_OFFSET) && (i < APT_CHB_OFFSET + APT_CH_WIDTH) && (p >= 198))
                {
                    apt_rgb_t rgb = apt_applyPalette(apt_PrecipPalette, p - 198);
                    // Negative float values get converted to positive uchars here
                    l[i*3] = (uchar)rgb.r;
                    l[i*3+1] = (uchar)rgb.g;
                    l[i*3+2] = (uchar)rgb.b;
                    int a = i - APT_CHB_OFFSET + APT_CHA_OFFSET;
                    l[a*3] = (uchar)rgb.r;
                    l[a*3+1] = (uchar)rgb.g;
                    l[a*3+2] = (uchar)rgb.b;
                }
                else
                {
                    uchar q = roundAndClip(p);
                    l[i*3] = q;
                    l[i*3+1] = q;
                    l[i*3+2] = q;
                }
            }
        }
        return extractImage(m_colourImage, channels);
    }
    if (channels == APTDemodSettings::VISIBLE)
    {
        // Visible calibration
        int satnum = 15;
        if (m_satelliteName == "NOAA 18") {
            satnum = 18;
        } else if (m_satelliteName == "NOAA 19") {
            satnum = 19;
        }
        apt_calibrate_visible(satnum, &m_tempImage, APT_CHA_OFFSET, APT_CH_WIDTH);
    }

    if (channels == APTDemodSettings::TEMPERATURE)
    {
        // Temperature calibration
        int satnum = 15;
        if (m_satelliteName == "NOAA 18") {
            satnum = 18;
        } else if (m_satelliteName == "NOAA 19") {
            satnum = 19;
        }
        apt_calibrate_thermal(satnum, &m_tempImage, APT_CHB_OFFSET, APT_CH_WIDTH);

        // Apply colour palette
        for (int r = 0; r < m_tempImage.nrow; r++)
        {
            uchar *l = m_colourImage.scanLine(r);
            for (int i = 0; i < APT_CH_WIDTH; i++)
            {
                float p = m_tempImage.prow[r][i+APT_CHB_OFFSET];
                uchar q = roundAndClip(p);
                l[i*3] = apt_TempPalette[q*3];
                l[i*3+1] = apt_TempPalette[q*3+1];
                l[i*3+2] = apt_TempPalette[q*3+2];
            }
        }
        return m_colourImage.copy(0, 0, APT_CH_WIDTH, m_tempImage.nrow);
    }
    else if (channels == APTDemodSettings::PALETTE)
    {
        if ((m_settings.m_palette >= 0) && (m_settings.m_palette < m_palettes.size()))
        {
            // Apply colour palette
            for (int r = 0; r < m_tempImage.nrow; r++)
            {
                uchar *l = m_colourImage.scanLine(r);
                for (int i = 0; i < APT_CH_WIDTH; i++)
                {
                    float pA = m_tempImage.prow[r][i+APT_CHA_OFFSET];
                    float pB = m_tempImage.prow[r][i+APT_CHB_OFFSET];
                    uchar qA = roundAndClip(pA);
                    uchar qB = roundAndClip(pB);
                    QRgb rgb = m_palettes[m_settings.m_palette].pixel(qA, qB);
                    l[i*3] = qRed(rgb);
                    l[i*3+1] = qGreen(rgb);
                    l[i*3+2] = qBlue(rgb);
                }
            }
            return m_colourImage.copy(0, 0, APT_CH_WIDTH, m_tempImage.nrow);
        }
        else
        {
            qDebug() << "APTDemodImageWorker::processImage - Illegal palette number: " << m_settings.m_palette;
            return QImage();
        }
    }
    else
    {
        // Extract grey-scale image
        for (int r = 0; r < m_tempImage.nrow; r++)
        {
            uchar *l = m_greyImage.scanLine(r);

            for (int i = 0; i < APT_IMG_WIDTH; i++)
            {
                float p = m_tempImage.prow[r][i];
                l[i] = roundAndClip(p);
            }
        }
        return extractImage(m_greyImage, channels);
    }
}

QImage APTDemodImageWorker::extractImage(QImage image, APTDemodSettings::ChannelSelection channels)
{
    if (channels == APTDemodSettings::BOTH_CHANNELS) {
        return image.copy(0, 0, APT_IMG_WIDTH, m_tempImage.nrow);
    } else if ((channels == APTDemodSettings::CHANNEL_A) || (channels == APTDemodSettings::VISIBLE)) {
        return image.copy(APT_CHA_OFFSET, 0, APT_CH_WIDTH, m_tempImage.nrow);
    } else {
        return image.copy(APT_CHB_OFFSET, 0, APT_CH_WIDTH, m_tempImage.nrow);
    }
}

void APTDemodImageWorker::prependPath(QString &filename)
{
    if (!m_settings.m_autoSavePath.isEmpty())
    {
        if (m_settings.m_autoSavePath.endsWith('/')) {
            filename = m_settings.m_autoSavePath + filename;
        } else {
            filename = m_settings.m_autoSavePath + '/' + filename;
        }
    }
}

void APTDemodImageWorker::saveImageToDisk()
{
    QStringList imageTypes;
    QImage image = processImage(imageTypes, APTDemodSettings::BOTH_CHANNELS);

    if (image.height() >= m_settings.m_autoSaveMinScanLines)
    {
        QString filename;
        QDateTime dateTime;
        QString dt;
        if (m_settings.m_aosDateTime.isValid()) {
            dateTime = m_settings.m_aosDateTime;
        } else {
            dateTime = QDateTime::currentDateTime();
        }
        dt = dateTime.toString("yyyyMMdd_hhmm");
        QString sat = m_satelliteName;
        sat.replace(" ", "_");

        if (m_settings.m_saveCombined)
        {
            filename = QString("apt_%1_%2.png").arg(sat).arg(dt);
            prependPath(filename);
            if (!image.save(filename)) {
                qCritical() << "Failed to save APT image to: " << filename;
            }
        }

        QImage chA = extractImage(image, APTDemodSettings::CHANNEL_A);
        QImage chB = extractImage(image, APTDemodSettings::CHANNEL_B);

        if (m_settings.m_saveSeparate)
        {
            filename = QString("apt_%1_%2_cha.png").arg(sat).arg(dt);
            prependPath(filename);
            if (!chA.save(filename)) {
                qCritical() << "Failed to save APT image to: " << filename;
            }
            filename = QString("apt_%1_%2_chb.png").arg(sat).arg(dt);
            prependPath(filename);
            if (!chB.save(filename)) {
                qCritical() << "Failed to save APT image to: " << filename;
            }
        }

        if (m_settings.m_saveProjection)
        {
            filename = QString("apt_%1_%2_cha_eqi_cylindrical.png").arg(sat).arg(dt);
            prependPath(filename);
            QImage chAProj = projectImage(chA);
            if (!chAProj.save(filename)) {
                qCritical() << "Failed to save APT image to: " << filename;
            }
            filename = QString("apt_%1_%2_chb_eqi_cylindrical.png").arg(sat).arg(dt);
            prependPath(filename);
            QImage chBProj = projectImage(chB);
            if (!chBProj.save(filename)) {
                qCritical() << "Failed to save APT image to: " << filename;
            }
        }
    }
}

void APTDemodImageWorker::copyImage(apt_image_t *dst, apt_image_t *src)
{
    dst->nrow = src->nrow;
    dst->chA = src->chA;
    dst->chB = src->chB;

    for (int i = 0; i < src->nrow; i++) {
        std::copy(src->prow[i], src->prow[i] + APT_PROW_WIDTH, dst->prow[i]);
    }
}

uchar APTDemodImageWorker::roundAndClip(float p)
{
    int q = (int) round(p);
    q = q > 255 ? 255 : q < 0 ? 0 : q;
    return q;
}
