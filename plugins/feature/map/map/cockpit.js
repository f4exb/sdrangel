// Airbus like PFD (Primary Flight Display)

var icao;
var callsign;
var aircraftType;
var onSurface;
var wasOnSurface60SecsAgo;
var speed;
var modelSpeed; // Speed of 3D model in Cesium
var trueAirspeed;
var groundspeed;
var targetSpeed;
var altitude;
var targetAltitude;
var radioAltitude;
var verticalSpeed; // ft/m
var heading; // degrees mag
var targetHeading;
var track; // degrees true
var pressure; // mb
var mach;
var roll;
var autopilot;
var verticalMode;
var lateralMode;
var tcasMode;
var windSpeed;
var windDirection;
var staticAirTemperature;

var mdoelSpeed;
var toga = false;
var rollOut = false;
var runwayAltitude;

const grayColor = "#6A75AE";
const greenColor = "#20E966";
const cyanColor = "#2FF7FE";
const yellowColor = "#F4F82F";
const orangeColor = "#FEDA30";
const skyColor = "#26C9FF";
const groundColor = "#D35E34";

function setPFDData(forward, newICAO, newCallsign, newAircraftType, newOnSurface, newWasOnSurface60SecsAgo, newRunwayAltitudeEstimate,
    newModelSpeed, newIndicatedAirspeed, newTrueAirspeed, newGroundspeed, newMach, newAltitude, newRadioAltitude, newQnh, newVerticalSpeed,
    newHeading, newTrack, newRoll, newSelectedAltitude, newSelectedHeading,
    newAutopilot, newVerticalMode, newLateralMode, newTCASMode,
    newWindSpeed, newWindDirection, newStaticAirTemperature) {

    //console.log('PFD', newIndicatedAirspeed, newMach, newAltitude, newQnh, newVerticalSpeed, newHeading, newTrack, newRoll, newSelectedAltitude, newSelectedHeading);

    const newAircraft = icao !== newICAO;

    if (newAircraft) {
        toga = false;
        rollOut = false;
        runwayAltitude = undefined;
    }

    icao = newICAO;
    callsign = newCallsign;
    aircraftType = newAircraftType;
    onSurface = newOnSurface;
    wasOnSurface60SecsAgo = newWasOnSurface60SecsAgo;
    modelSpeed = newModelSpeed;
    if ((newIndicatedAirspeed === undefined) || (newOnSurface > 0)) {
        // IAS not transmitted frequently. After landing will no longer be valid
        if (newGroundspeed !== undefined) {
            speed = newGroundspeed;
        } else {
            speed = newModelSpeed;
        }
    } else {
        speed = newIndicatedAirspeed;
    }
    trueAirspeed = newTrueAirspeed;
    groundspeed = newGroundspeed;
    mach = newMach;
    altitude = newAltitude;
    radioAltitude = newRadioAltitude;
    pressure = newQnh;
    verticalSpeed = newVerticalSpeed;
    if (newHeading === undefined) {
        heading = newTrack; // Track more frequent than heading
    } else {
        heading = newHeading;
    }
    track = newTrack;
    roll = newRoll;
    targetAltitude = newSelectedAltitude;
    targetHeading = newSelectedHeading;
    if (newAutopilot === -1) {
        autopilot = undefined;
    } else {
        autopilot = newAutopilot;
    }
    verticalMode = newVerticalMode;
    lateralMode = newLateralMode;
    tcasMode = newTCASMode;
    windSpeed = newWindSpeed;
    windDirection = newWindDirection;
    staticAirTemperature = newStaticAirTemperature;

    // Correct altitude for QNH setting
    if (pressure !== undefined) {
        altitude += (pressure - 1013.25) * 30;
    }

    if (!rollOut && forward && (wasOnSurface60SecsAgo === 0) && (onSurface > 0) && (modelSpeed >= 70)) {
        rollOut = true;
    } else if (rollOut && ((modelSpeed < 40) || (onSurface === 0))) {
        rollOut = false;
    }

    const accelerationHeight = 1500;

    if (!toga && !rollOut && forward && (onSurface > 0) && (modelSpeed >= 35)) {
        toga = true; // Start take-off roll on surface
        runwayAltitude = altitude;
    } else if (!toga && !rollOut && (modelSpeed >= 35) && (runwayAltitude === undefined) && (wasOnSurface60SecsAgo > 0) && (newRunwayAltitudeEstimate !== undefined)) {
        toga = true; // For when selecting an aircraft having just taken off
        runwayAltitude = newRunwayAltitudeEstimate;
    } else if (!toga && !rollOut && (onSurface === 0) && (wasOnSurface60SecsAgo > 0) && (altitude !== undefined) && (runwayAltitude !== undefined) && (altitude < runwayAltitude + accelerationHeight)) {
        toga = true; // For when played in reverse
    } else if (toga && (modelSpeed <= 20)) {
        toga = false;
    } else if (toga && (altitude !== undefined) && (runwayAltitude !== undefined) && (altitude >= runwayAltitude + accelerationHeight)) {
        toga = false;
    }
    if (toga && (runwayAltitude === undefined) && (newRunwayAltitudeEstimate !== undefined)) {
        runwayAltitude = newRunwayAltitudeEstimate;
    }
}

function isStd() {
    return (pressure >= 1012) && (pressure < 1014);
}

function drawAirspeedIndicator(ctx, fm20, fm30) {
    // Airspeed indicator

    const speedIndWidth = 100;
    const speedIndHeight = 500;
    const speedIndLeft = 30;
    const speedIndRight = speedIndLeft + speedIndWidth;
    const speedIndTop = 250;
    const speedIndBottom = speedIndTop + speedIndHeight;
    const speedIndMid = speedIndTop + speedIndHeight / 2;

    // Background
    ctx.fillStyle = grayColor;
    ctx.fillRect(speedIndLeft, speedIndTop, speedIndWidth, speedIndHeight);

    // Clip speed tape
    ctx.save();
    ctx.rect(speedIndLeft, speedIndTop, speedIndWidth, speedIndHeight);
    ctx.clip();

    // Speed tape markings every 10knts
    const speedTapMinSpeed = 0; // A320 only starts drawning from 30knts, but we draw from 0
    const speedTapeLeft = speedIndRight - 20;
    const speedTapeRight = speedIndRight;
    const speedTapeSpacing = 60;
    const speedTapeStep = 10;
    const speedTextStep = 20;
    const speedPerPixel = speedTapeSpacing / speedTapeStep;
    const speedOffset = (speed % speedTextStep) * speedPerPixel;
    ctx.beginPath();
    var speedTape = Math.round(speed - speed % speedTextStep + 6 * speedTapeStep);
    for (var i = -6; i < 5; i++) {
        if (speedTape >= speedTapMinSpeed) {
            ctx.moveTo(speedTapeLeft, speedOffset + i * speedTapeSpacing + speedIndMid);
            ctx.lineTo(speedTapeRight, speedOffset + i * speedTapeSpacing + speedIndMid);
        }
        speedTape -= speedTapeStep;
    }
    ctx.lineWidth = 2;
    ctx.strokeStyle = "white";
    ctx.stroke();

    // Speeds every 20knts
    var speedText = Math.round(speed - speed % speedTextStep + 3 * speedTextStep);
    for (var i = -3; i < 3; i++) {
        ctx.font = "30px Arial";
        ctx.fillStyle = "white";
        if (speedText >= speedTapMinSpeed) {
            const speedTextString = speedText.toString().padStart(3, '0');
            ctx.fillText(speedTextString, speedIndLeft + 20, speedOffset + i * speedTapeSpacing * 2 + speedIndMid + fm30.actualBoundingBoxAscent / 2);
        }
        speedText -= speedTextStep;
    }

    // Disable clipping
    ctx.restore();

    ctx.beginPath();
    ctx.moveTo(speedIndLeft, speedIndTop);
    ctx.lineTo(speedIndRight + 30, speedIndTop);
    var speedIndRightLineHeight = speed / (speedIndHeight / 2 / speedPerPixel); // White line on right only drawn down to speedTapMinSpeed
    if (speedIndRightLineHeight > 1) {
        speedIndRightLineHeight = 1;
        ctx.moveTo(speedIndLeft, speedIndTop + speedIndHeight);
        ctx.lineTo(speedIndRight + 30, speedIndTop + speedIndHeight);
    }
    ctx.moveTo(speedIndRight, speedIndTop);
    ctx.lineTo(speedIndRight, speedIndTop + speedIndRightLineHeight * (speedIndHeight / 2) + (speedIndHeight / 2));
    ctx.lineWidth = 2;
    ctx.strokeStyle = "white";
    ctx.stroke();

    // Speed indicator
    ctx.beginPath();
    ctx.moveTo(speedIndLeft - 10, speedIndMid);
    ctx.lineTo(speedIndLeft, speedIndMid);
    ctx.moveTo(speedTapeLeft - 5, speedIndMid);
    ctx.lineTo(speedTapeRight + 20, speedIndMid);
    ctx.lineWidth = 2;
    ctx.strokeStyle = yellowColor;
    ctx.stroke();

    ctx.beginPath();
    ctx.moveTo(speedTapeRight + 10, speedIndMid);
    ctx.lineTo(speedTapeRight + 35, speedIndMid - 10);
    ctx.lineTo(speedTapeRight + 35, speedIndMid + 10);
    ctx.lineTo(speedTapeRight + 10, speedIndMid);
    ctx.fillStyle = yellowColor;
    ctx.fill();

    // Target speed

    if (targetSpeed !== undefined) {
        var targetSpeedY = (speed - targetSpeed) * speedPerPixel;
        targetSpeedY = targetSpeedY + speedIndMid;
        if (targetSpeedY < speedIndTop) {
            // Target speed as text on top
            ctx.font = "20px Arial";
            ctx.fillStyle = cyanColor;
            const targetSpeedMetrics = ctx.measureText(targetSpeed);
            ctx.fillText(targetSpeed, speedIndRight - targetSpeedMetrics.width / 2, speedIndTop - 4);

        } else if (targetSpeedY > speedIndBottom) {
            // Target speed as text underneath
            ctx.font = "20px Arial";
            ctx.fillStyle = cyanColor;
            const targetSpeedMetrics = ctx.measureText(targetSpeed);
            ctx.fillText(targetSpeed, speedIndRight - targetSpeedMetrics.width / 2, speedIndBottom + fm20.actualBoundingBoxAscent + 4);
        } else {
            // Cyan triangle on right side
            const targetSpeedWidth = 40;
            const targetSpeedHeight = 30;
            const targetSpeedLeft = speedIndRight;
            const targetSpeedRight = targetSpeedLeft + targetSpeedWidth;

            ctx.beginPath();
            ctx.moveTo(targetSpeedLeft, targetSpeedY);
            ctx.lineTo(targetSpeedRight, targetSpeedY - targetSpeedHeight / 2);
            ctx.lineTo(targetSpeedRight, targetSpeedY + targetSpeedHeight / 2);
            ctx.lineTo(targetSpeedLeft, targetSpeedY);
            ctx.lineWidth = 2;
            ctx.strokeStyle = cyanColor;
            ctx.stroke();
        }
    }
}

function drawAltitudeIndicator(ctx, fm20, fm30) {
    // Altitude indicator

    const altitudeIndWidth = 70;
    const altitudeIndHeight = 500;
    const altitudeIndLeft = 750;
    const altitudeIndRight = altitudeIndLeft + altitudeIndWidth;
    const altitudeIndTop = 250;
    const altitudeIndBottom = altitudeIndTop + altitudeIndHeight;
    const altitudeIndMid = altitudeIndTop + altitudeIndHeight / 2;

    // Background
    ctx.fillStyle = grayColor;
    ctx.fillRect(altitudeIndLeft, altitudeIndTop, altitudeIndWidth, altitudeIndHeight);

    ctx.font = "30px Arial";
    const greaterThanMetrics = ctx.measureText(">");

    // Mid point marker (for glideslope, but always visible)
    ctx.fillStyle = yellowColor;
    ctx.fillRect(altitudeIndLeft - 70, altitudeIndMid - 3, 50, 6);

    // Clip altitude tape
    ctx.save();
    ctx.rect(altitudeIndLeft - greaterThanMetrics.width, altitudeIndTop, altitudeIndWidth + greaterThanMetrics.width, altitudeIndHeight);
    ctx.clip();

    // Altitude tape markings every 100 ft
    const altitudeTapeLeft = altitudeIndRight - 10;
    const altitudeTapeRight = altitudeIndRight;
    const altitudeTapeSpacing = 40;
    const altitudeTapeStep = 100;
    const altitudeTextStep = 500;
    const altitudePerPixel = altitudeTapeSpacing / altitudeTapeStep; // reciprocal
    const altitudeOffset = (altitude % altitudeTextStep) * altitudePerPixel;
    ctx.beginPath();
    var altitudeTape = Math.round(altitude - altitude % altitudeTextStep + 10 * altitudeTapeStep);
    for (var i = -10; i < 7; i++) {
        ctx.moveTo(altitudeTapeLeft, altitudeOffset + i * altitudeTapeSpacing + altitudeIndMid);
        ctx.lineTo(altitudeTapeRight, altitudeOffset + i * altitudeTapeSpacing + altitudeIndMid);
        altitudeTape -= altitudeTapeStep;
    }
    ctx.lineWidth = 2;
    ctx.strokeStyle = "white";
    ctx.stroke();

    // Flight level texts every 500ft
    ctx.font = "30px Arial";
    ctx.fillStyle = "white";
    var altitudeText = Math.round(altitude - altitude % altitudeTextStep + 2 * altitudeTextStep);
    for (var i = -2; i < 2; i++) {
        const flightLevelText = Math.abs(altitudeText / 100); // Don't display - if negative
        const flightLevelTextString = ">" + flightLevelText.toString().padStart(3, '0');
        ctx.fillText(flightLevelTextString, altitudeIndLeft - greaterThanMetrics.width, altitudeOffset + i * altitudeTapeSpacing * 5 + altitudeIndMid + fm30.actualBoundingBoxAscent / 2);
        altitudeText -= altitudeTextStep;
    }

    // Disable clipping
    ctx.restore();

    // White lines at top, bottom and on right
    ctx.beginPath();
    ctx.moveTo(altitudeIndLeft, altitudeIndTop);
    ctx.lineTo(altitudeIndRight + 30, altitudeIndTop);
    ctx.moveTo(altitudeIndLeft, altitudeIndTop + altitudeIndHeight);
    ctx.lineTo(altitudeIndRight + 30, altitudeIndTop + altitudeIndHeight);
    ctx.moveTo(altitudeIndRight, altitudeIndTop);
    ctx.lineTo(altitudeIndRight, altitudeIndTop + altitudeIndHeight);
    ctx.lineWidth = 2;
    ctx.strokeStyle = "white";
    ctx.stroke();

    // Target altitude

    if (targetAltitude !== undefined) {
        var targetAltitudeY = (altitude - targetAltitude) * altitudePerPixel;
        targetAltitudeY = targetAltitudeY + altitudeIndMid;

        if (isStd()) {
            targetAltitudeText = Math.round(targetAltitude / 100);
        } else {
            targetAltitudeText = Math.round(targetAltitude);
        }

        if (targetAltitudeY < altitudeIndTop) {
            // Target altitude as text on top
            ctx.font = "30px Arial";
            ctx.fillStyle = cyanColor;
            const targetAltitudeMetrics = ctx.measureText(targetAltitudeText);
            if (isStd()) {
                ctx.fillText("FL", altitudeIndLeft, altitudeIndTop - 4);
            }
            ctx.fillText(targetAltitudeText, altitudeIndRight - targetAltitudeMetrics.width / 2, altitudeIndTop - 4);
        } else if (targetAltitudeY > altitudeIndBottom) {
            // Target altitude as text underneath
            ctx.font = "30px Arial";
            ctx.fillStyle = cyanColor;
            const targetAltitudeMetrics = ctx.measureText(targetAltitudeText);
            if (isStd()) {
                ctx.fillText("FL", altitudeIndLeft, altitudeIndBottom + fm30.actualBoundingBoxAscent + 4);
            }
            ctx.fillText(targetAltitudeText, altitudeIndRight - targetAltitudeMetrics.width / 2, altitudeIndBottom + fm30.actualBoundingBoxAscent + 4);
        } else {

            // Clip
            ctx.save();
            ctx.rect(altitudeIndLeft - greaterThanMetrics.width, altitudeIndTop, altitudeIndWidth + greaterThanMetrics.width, altitudeIndHeight);
            ctx.clip();

            targetAltitudeText = targetAltitudeText.toString().padStart(3, '0');

            ctx.font = "30px Arial";
            ctx.fillStyle = cyanColor;
            const targetAltitudeMetrics = ctx.measureText(targetAltitudeText);

            // Cyan box
            const targetAltitudeWidth = 30;
            const targetAltitudeHeight = 3 * (fm30.actualBoundingBoxAscent + fm30.actualBoundingBoxDescent);
            const targetAltitudeBoxHeight = 1.1 * (fm30.actualBoundingBoxAscent + fm30.actualBoundingBoxDescent);
            const targetAltitudeLeft = altitudeIndLeft - 5;
            const targetAltitudeRight = targetAltitudeLeft + targetAltitudeWidth;

            ctx.beginPath();
            ctx.moveTo(targetAltitudeLeft, targetAltitudeY - targetAltitudeHeight / 2);
            ctx.lineTo(targetAltitudeLeft, targetAltitudeY - 5);
            ctx.lineTo(targetAltitudeLeft + 5, targetAltitudeY);
            ctx.lineTo(targetAltitudeLeft, targetAltitudeY + 5);
            ctx.lineTo(targetAltitudeLeft, targetAltitudeY + targetAltitudeHeight / 2);
            ctx.lineTo(targetAltitudeRight, targetAltitudeY + targetAltitudeHeight / 2);
            ctx.lineTo(targetAltitudeRight, targetAltitudeY - targetAltitudeHeight / 2);
            ctx.lineTo(targetAltitudeLeft, targetAltitudeY - targetAltitudeHeight / 2);
            ctx.lineWidth = 2;
            ctx.lineWidth = 2;
            ctx.strokeStyle = cyanColor;
            ctx.stroke();

            ctx.fillStyle = 'black';
            ctx.fillRect(altitudeIndLeft, targetAltitudeY - targetAltitudeBoxHeight / 2, altitudeIndWidth - 2, targetAltitudeBoxHeight);
            ctx.fillStyle = cyanColor;
            ctx.fillText(targetAltitudeText, altitudeIndLeft, targetAltitudeY + fm30.actualBoundingBoxAscent / 2);

            // Disable clipping
            ctx.restore();
        }
    }

    // Altitude

    ctx.font = "34px Arial";
    const levelText = (altitude === undefined) || isNaN(altitude) ? "" : Math.floor(altitude / 100).toString();
    const levelTextMetrics = ctx.measureText(levelText);
    const levelTextHeightMetrics = ctx.measureText("0");
    const border = 2;
    const levelHeight = levelTextHeightMetrics.actualBoundingBoxAscent + levelTextHeightMetrics.actualBoundingBoxDescent + 8 * border;

    const subscaleHeight = 60;
    const subscaleWidth = 30;

    // Black background box
    ctx.fillStyle = 'black';
    ctx.fillRect(altitudeIndLeft, altitudeIndMid - levelHeight / 2, altitudeIndWidth + 1, levelHeight);

    // Current flight level
    if (levelText !== "") {
        ctx.fillStyle = greenColor;
        ctx.fillText(levelText, altitudeIndRight - levelTextMetrics.width, altitudeIndMid + levelTextMetrics.actualBoundingBoxAscent / 2);
    }

    // Yellow box
    ctx.beginPath();
    ctx.moveTo(altitudeIndLeft, altitudeIndMid - levelHeight / 2);
    ctx.lineTo(altitudeIndRight, altitudeIndMid - levelHeight / 2);
    ctx.lineTo(altitudeIndRight, altitudeIndMid - subscaleHeight / 2);
    ctx.lineTo(altitudeIndRight + subscaleWidth, altitudeIndMid - subscaleHeight / 2);
    ctx.lineTo(altitudeIndRight + subscaleWidth, altitudeIndMid + subscaleHeight / 2);
    ctx.lineTo(altitudeIndRight, altitudeIndMid + subscaleHeight / 2);
    ctx.lineTo(altitudeIndRight, altitudeIndMid + levelHeight / 2);
    ctx.lineTo(altitudeIndLeft, altitudeIndMid + levelHeight / 2);
    ctx.lineWidth = 2;
    if (Math.abs(verticalSpeed) >= 6000) {
        ctx.strokeStyle = orangeColor;
    } else {
        ctx.strokeStyle = yellowColor;
    }
    ctx.stroke();

    // Clip around subscale
    ctx.save();
    ctx.rect(altitudeIndRight, altitudeIndMid - subscaleHeight / 2, subscaleWidth, subscaleHeight);
    ctx.clip();

    const subscaleStep = 20;
    var subscaleText = (altitude % 100) + subscaleStep * 2;
    var subscaleSpacing = fm20.actualBoundingBoxAscent + fm20.actualBoundingBoxDescent + 2;
    const subscalePerPixel = subscaleSpacing / subscaleStep; // reciprocal
    const subscaleOffset = (altitude % subscaleStep) * subscalePerPixel;
    ctx.font = "20px Arial";
    for (var i = -2; i < 2; i++) {
        const subscaleTextString = (Math.floor((subscaleText % 100) / subscaleStep) * subscaleStep).toString().padStart(2, '0');
        ctx.fillText(subscaleTextString, altitudeIndRight + 1, subscaleOffset + i * subscaleSpacing + altitudeIndMid + fm20.actualBoundingBoxAscent / 2);
        subscaleText -= subscaleStep;
        if (subscaleText < 0) {
            subscaleText += 100;
        }
    }

    // Disable clipping
    ctx.restore();
}
function drawHeadingIndicator(ctx, fm20, fm30) {

    const headingIndWidth = 450;
    const headingIndHeight = 60;
    const headingIndLeft = 225;
    const headingIndRight = headingIndLeft + headingIndWidth;
    const headingIndTop = 890;
    const headingIndBottom = headingIndTop + headingIndHeight;
    const headingIndMid = headingIndLeft + headingIndWidth / 2;

    ctx.fillStyle = grayColor;
    ctx.fillRect(headingIndLeft, headingIndTop, headingIndWidth, headingIndHeight);

    // White lines left, right and top
    ctx.beginPath();
    ctx.moveTo(headingIndLeft, headingIndBottom);
    ctx.lineTo(headingIndLeft, headingIndTop);
    ctx.lineTo(headingIndRight, headingIndTop);
    ctx.lineTo(headingIndRight, headingIndBottom);
    ctx.lineWidth = 2;
    ctx.strokeStyle = "white";
    ctx.stroke();

    // Heading indicator line
    ctx.beginPath();
    ctx.moveTo(headingIndMid, headingIndTop - 30);
    ctx.lineTo(headingIndMid, headingIndTop);
    ctx.lineWidth = 4;
    ctx.strokeStyle = yellowColor;
    ctx.stroke();

    // Clip tape
    ctx.save();
    ctx.rect(headingIndLeft, headingIndTop, headingIndWidth, headingIndHeight);
    ctx.clip();

    // Tape markings every 5 degrees
    const headingTapeLeft = headingIndRight - 20;
    const headingTapeRight = headingIndRight;
    const headingTapeSpacing = 50;
    const headingTapeStep = 5;
    const headingTextStep = 10;
    const headingPerPixel = headingTapeSpacing / headingTapeStep;
    const headingOffset = - (heading % headingTextStep) * headingPerPixel;
    ctx.beginPath();
    var headingTape = Math.round(heading - heading % headingTextStep - 6 * headingTapeStep);
    for (var i = -6; i < 7; i++) {
        ctx.moveTo(headingOffset + i * headingTapeSpacing + headingIndMid, headingIndTop);
        if (headingTape % 10 != 0) {
            ctx.lineTo(headingOffset + i * headingTapeSpacing + headingIndMid, headingIndTop + 10);
        } else {
            ctx.lineTo(headingOffset + i * headingTapeSpacing + headingIndMid, headingIndTop + 20);
        }
        headingTape += headingTapeStep;
    }
    ctx.lineWidth = 2;
    ctx.strokeStyle = "white";
    ctx.stroke();

    ctx.font = "30px Arial";
    ctx.fillStyle = "white";
    const headingMetricsBig = ctx.measureText("36");
    ctx.font = "24px Arial";
    ctx.fillStyle = "white";
    const headingMetricsSmall = ctx.measureText("36");

    // Headings every 10 degrees
    var headingText = Math.round(heading - heading % headingTextStep - 3 * headingTextStep);
    for (var i = -3; i < 4; i++) {
        const big = headingText % 30 == 0;
        if (big) {
            ctx.font = "30px Arial";
        } else {
            ctx.font = "24px Arial";
        }
        ctx.fillStyle = "white";
        var headingTextMod = Math.round(headingText / 10) % 36;
        if (headingTextMod < 0) {
            headingTextMod += 36;
        }
        const headingTextString = headingTextMod.toString();
        const headingMetrics = ctx.measureText(headingTextString);

        ctx.fillText(headingTextString, headingOffset + i * headingTapeSpacing * 2 + headingIndMid - headingMetrics.width / 2, headingIndTop + 30 + headingMetrics.actualBoundingBoxAscent);
        headingText += headingTextStep;
    }

    // Track diamond

    if (track !== undefined) {
        const trackMid = headingIndMid + (track - heading) * headingPerPixel;
        const trackSize = 20;

        ctx.beginPath();
        ctx.moveTo(trackMid, headingIndTop);
        ctx.lineTo(trackMid - trackSize / 2, headingIndTop + trackSize / 2);
        ctx.lineTo(trackMid, headingIndTop + trackSize);
        ctx.lineTo(trackMid + trackSize / 2, headingIndTop + trackSize / 2);
        ctx.lineTo(trackMid, headingIndTop);
        ctx.lineWidth = 5;
        ctx.strokeStyle = greenColor;
        ctx.stroke();
    }

    // Disable clipping
    ctx.restore();

    // Target heading

    if (targetHeading !== undefined) {
        var targetDiff = targetHeading - heading;
        if (targetDiff > 180) {
            targetDiff -= 360;
        }
        var targetMid = headingIndMid + targetDiff * headingPerPixel;
        if (targetMid < headingIndLeft) {

            // Display target heading as text on left
            ctx.font = "20px Arial";
            ctx.fillStyle = cyanColor;
            const targetHeadingText = Math.round(targetHeading).toString().padStart(3, '0');
            const targetHeadingMetrics = ctx.measureText(targetHeadingText);
            ctx.fillText(targetHeadingText, headingIndLeft, headingIndTop - 4);

        } else if (targetMid > headingIndRight) {

            // Display target heading as text on right
            ctx.font = "20px Arial";
            ctx.fillStyle = cyanColor;
            const targetHeadingText = Math.round(targetHeading).toString().padStart(3, '0');
            const targetHeadingMetrics = ctx.measureText(targetHeadingText);
            ctx.fillText(targetHeadingText, headingIndRight - targetHeadingMetrics.width, headingIndTop - 4);

        } else {
            // Target indicator triangle

            const targetWidth = 30;
            const targetHeight = 40;

            ctx.beginPath();
            ctx.moveTo(targetMid, headingIndTop);
            ctx.lineTo(targetMid - targetWidth / 2, headingIndTop - targetHeight);
            ctx.lineTo(targetMid + targetWidth / 2, headingIndTop - targetHeight);
            ctx.lineTo(targetMid, headingIndTop);
            ctx.lineWidth = 2;
            ctx.strokeStyle = cyanColor;
            ctx.stroke();
        }
    }
}

function drawBankAngleBoxMarker(ctx, radius, height) {
    ctx.beginPath();
    ctx.moveTo(-5, -radius);
    ctx.lineTo(-5, -radius - height);
    ctx.lineTo(5, -radius - height);
    ctx.lineTo(5, -radius);
    ctx.lineWidth = 2;
    ctx.strokeStyle = 'white';
    ctx.stroke();
}

function drawBankAngleLineMarker(ctx, radius, height) {
    ctx.beginPath();
    ctx.moveTo(0, -radius);
    ctx.lineTo(0, -radius - height);
    ctx.lineWidth = 2;
    ctx.strokeStyle = 'white';
    ctx.stroke();
}
function drawAttitudeIndicator(ctx, fm20, fm30) {

    if (roll === undefined) {
        return;
    }

    // Height the same as airspeed indicator - cropped width same as heading indicator

    const speedIndRight = 30 + 100;
    const altitudeIndLeft = 750;

    const attitudeIndMidX = speedIndRight + ((altitudeIndLeft - speedIndRight) / 2);
    //const attitudeIndLeft = 30 + 100; // speedIndRight
    //const attitudeIndRight = 750; // altitudeIndLeft
    const attitudeIndWidth = 500; // speedIndHeight = 500; //  attitudeIndRight - attitudeIndLeft;
    const attitudeIndTop = 250;
    const attitudeIndLeft = attitudeIndMidX - attitudeIndWidth / 2;
    const attitudeIndHeight = attitudeIndWidth; // It's a circle
    const attitudeIndMidY = attitudeIndTop + attitudeIndHeight / 2;
    const attitudeIndCrop = (attitudeIndWidth - 450) / 2; // headingIndWidth = 450

    // Clip
    ctx.save();
    //ctx.arc(attitudeIndMidX, attitudeIndMidY, attitudeIndWidth / 2, 0, 2 * Math.PI);
    ctx.beginPath();
    ctx.arc(attitudeIndMidX, attitudeIndMidY, attitudeIndWidth / 2, Math.PI / 10 + Math.PI, Math.PI - Math.PI / 10 + Math.PI);
    ctx.arc(attitudeIndMidX, attitudeIndMidY, attitudeIndWidth / 2, Math.PI / 10, Math.PI - Math.PI / 10);
//    //ctx.rect(attitudeIndLeft + attitudeIndCrop, attitudeIndTop, attitudeIndWidth - 2 * attitudeIndCrop, attitudeIndWidth);
 //   ctx.rect(attitudeIndLeft + attitudeIndCrop, attitudeIndTop, attitudeIndWidth - 2 * attitudeIndCrop, attitudeIndWidth);
    ctx.clip();


    // Background
    //ctx.beginPath();
    //ctx.fillStyle = cyanColor;
    //ctx.arc(attitudeIndMidX, attitudeIndMidY, attitudeIndWidth / 2, Math.PI / 10, Math.PI - Math.PI / 10);
    //ctx.arc(attitudeIndMidX, attitudeIndMidY, attitudeIndWidth / 2, Math.PI / 10 + Math.PI, Math.PI - Math.PI / 10 + Math.PI);
    //console.log(attitudeIndMidX, attitudeIndMidY, attitudeIndWidth / 2, 0, 2 * Math.PI);
    //ctx.fill();

    ctx.fillStyle = skyColor;
    ctx.fillRect(attitudeIndLeft, attitudeIndTop, attitudeIndWidth, attitudeIndWidth);

    const rollRad = -roll * Math.PI / 180;
    const radius = attitudeIndWidth / 2;

    ctx.fillStyle = groundColor;
    ctx.beginPath();
    ctx.arc(attitudeIndMidX, attitudeIndMidY, radius, 0 + rollRad, Math.PI + rollRad);
    ctx.fill();

    //const offsetX = Math.cos(rollRad) * radius;
    //const offsetY = Math.sin(rollRad) * radius;
    //ctx.beginPath();
    //ctx.moveTo(attitudeIndMidX - offsetX, attitudeIndMidY - offsetY);
    //ctx.lineTo(attitudeIndMidX + offsetX, attitudeIndMidY + offsetY);
    //ctx.lineWidth = 2;
    //ctx.strokeStyle = 'white';
    //ctx.stroke();

    // Disable clipping
    //ctx.restore();

    const twoAndHalfDegOffset = 15;
    const fiveDegOffset = twoAndHalfDegOffset * 2;
    const tenDegOffset = twoAndHalfDegOffset * 4;
    const fifteenDegOffset = twoAndHalfDegOffset * 2.5;
    const spacing = 50;

   // ctx.save();

    ctx.translate(attitudeIndMidX, attitudeIndMidY);
    ctx.rotate(rollRad);

    ctx.beginPath();
    ctx.moveTo(-radius, 0);
    ctx.lineTo(radius, 0);
    for (var i = -2; i < 4; i++) {
        ctx.moveTo(-twoAndHalfDegOffset, -i * spacing - spacing / 2);
        ctx.lineTo(twoAndHalfDegOffset, -i * spacing - spacing / 2);
    }
    for (var i = -1; i < 5; i++) {
        ctx.moveTo(-fiveDegOffset, -i * spacing);
        ctx.lineTo(fiveDegOffset, -i * spacing);
    }
    for (var i = -1; i < 3; i++) {
        ctx.moveTo(-tenDegOffset, -i * 2 * spacing);
        ctx.lineTo(tenDegOffset, -i * 2 * spacing);
    }
    ctx.moveTo(-fifteenDegOffset, 2.75 * spacing);
    ctx.lineTo(fifteenDegOffset, 2.75 * spacing);
    ctx.moveTo(-tenDegOffset, 3.5 * spacing);
    ctx.lineTo(tenDegOffset, 3.5 * spacing);
    ctx.lineWidth = 2;
    ctx.strokeStyle = 'white';
    ctx.stroke();

    const greenThingWidth = 20;
    ctx.beginPath();
    ctx.strokeStyle = greenColor;
    ctx.moveTo(-fifteenDegOffset - greenThingWidth, 2.75 * spacing - 3);
    ctx.lineTo(-fifteenDegOffset, 2.75 * spacing - 3);
    ctx.moveTo(-fifteenDegOffset - greenThingWidth, 2.75 * spacing + 3);
    ctx.lineTo(-fifteenDegOffset, 2.75 * spacing + 3);
    ctx.moveTo(fifteenDegOffset + greenThingWidth, 2.75 * spacing - 3);
    ctx.lineTo(fifteenDegOffset, 2.75 * spacing - 3);
    ctx.moveTo(fifteenDegOffset + greenThingWidth, 2.75 * spacing + 3);
    ctx.lineTo(fifteenDegOffset, 2.75 * spacing + 3);
    ctx.stroke();

    ctx.font = "20px Arial";
    ctx.fillStyle = 'white';
    const labelMetrics = ctx.measureText("20");
    const labelCenterY = labelMetrics.actualBoundingBoxAscent / 2;
    const labelOffset = tenDegOffset + 10;

    ctx.fillText("10", labelOffset, spacing * 2 + labelCenterY);
    ctx.fillText("10", -labelOffset - labelMetrics.width, spacing * 2 + labelCenterY);
    ctx.fillText("10", labelOffset, -spacing * 2 + labelCenterY);
    ctx.fillText("10", -labelOffset - labelMetrics.width, -spacing * 2 + labelCenterY);
    ctx.fillText("20", labelOffset, -spacing * 4 + labelCenterY);
    ctx.fillText("20", -labelOffset - labelMetrics.width, -spacing * 4 + labelCenterY);
    ctx.fillText("20", labelOffset, spacing * 3.5 + labelCenterY);
    ctx.fillText("20", -labelOffset - labelMetrics.width, spacing * 3.5 + labelCenterY);

    const rollBig = 18;
    const rollSmall = 10;

    const rollTriangleOffet = 4;

    ctx.beginPath();
    ctx.moveTo(0, -radius + rollTriangleOffet);
    ctx.lineTo(rollBig, -radius + rollBig + rollTriangleOffet);
    ctx.lineTo(-rollBig, -radius + rollBig + rollTriangleOffet);
    ctx.lineTo(0, -radius + rollTriangleOffet);
    ctx.lineWidth = 2;
    ctx.strokeStyle = yellowColor;
    ctx.stroke();

    const rollTrapX1 = 20;
    const rollTrapX2 = 30;
    const rollTrapY1 = -radius + rollBig + rollTriangleOffet + 5;
    const rollTrapY2 = rollTrapY1 + 10;

    ctx.beginPath();
    ctx.moveTo(-rollTrapX1, rollTrapY1);
    ctx.lineTo(rollTrapX1, rollTrapY1);
    ctx.lineTo(rollTrapX2, rollTrapY2);
    ctx.lineTo(-rollTrapX2, rollTrapY2);
    ctx.lineTo(-rollTrapX1, rollTrapY1);
    ctx.lineWidth = 2;
    ctx.strokeStyle = yellowColor;
    ctx.stroke();

    ctx.restore();

    // Roll scale

    ctx.save();
    ctx.translate(attitudeIndMidX, attitudeIndMidY);

    ctx.beginPath();
    ctx.rect(-radius, -radius - 2  *rollBig , radius * 2, radius);
    ctx.clip();

    ctx.beginPath();
    ctx.moveTo(0, -radius);
    ctx.lineTo(rollBig, -radius - rollBig);
    ctx.lineTo(-rollBig, -radius - rollBig);
    ctx.lineTo(0, -radius);
    ctx.lineWidth = 2;
    ctx.strokeStyle = yellowColor;
    ctx.stroke();

    ctx.beginPath();
    ctx.arc(0, 0, radius, (240 - 2) * Math.PI / 180, (300 + 2) * Math.PI / 180);
    ctx.lineWidth = 2;
    ctx.strokeStyle = 'white';
    ctx.stroke();

    ctx.rotate(45 * Math.PI / 180);
    drawBankAngleLineMarker(ctx, radius, rollBig);
    ctx.rotate(-15 * Math.PI / 180);
    drawBankAngleBoxMarker(ctx, radius, rollBig);
    ctx.rotate(-10 * Math.PI / 180);
    drawBankAngleBoxMarker(ctx, radius, 10);
    ctx.rotate(-10 * Math.PI / 180);
    drawBankAngleBoxMarker(ctx, radius, 10);
    ctx.rotate(-10 * Math.PI / 180);

    ctx.rotate(-10 * Math.PI / 180);
    drawBankAngleBoxMarker(ctx, radius, 10);
    ctx.rotate(-10 * Math.PI / 180);
    drawBankAngleBoxMarker(ctx, radius, 10);
    ctx.rotate(-10 * Math.PI / 180);
    drawBankAngleBoxMarker(ctx, radius, rollBig);
    ctx.rotate(-15 * Math.PI / 180);
    drawBankAngleLineMarker(ctx, radius, rollBig);


   // ctx.rotate(-rollRad);
   // ctx.translate(-attitudeIndMidX, -attitudeIndMidY);

    ctx.restore();

    // Wings
    const wingThickness = 14;
    const wingHeight = 35;
    const wingWidth = 90;
    const wingOffset = 130;

    ctx.beginPath();
    ctx.moveTo(attitudeIndMidX - wingOffset - wingWidth, attitudeIndMidY - wingThickness / 2);
    ctx.lineTo(attitudeIndMidX - wingOffset, attitudeIndMidY - wingThickness / 2);
    ctx.lineTo(attitudeIndMidX - wingOffset, attitudeIndMidY + wingHeight);
    ctx.lineTo(attitudeIndMidX - wingOffset - wingThickness, attitudeIndMidY + wingHeight);
    ctx.lineTo(attitudeIndMidX - wingOffset - wingThickness, attitudeIndMidY + wingThickness / 2);
    ctx.lineTo(attitudeIndMidX - wingOffset - wingWidth, attitudeIndMidY + wingThickness / 2);
    ctx.lineTo(attitudeIndMidX - wingOffset - wingWidth, attitudeIndMidY - wingThickness / 2);
    ctx.lineWidth = 4;
    ctx.fillStyle = 'black';
    ctx.strokeStyle = yellowColor;
    ctx.stroke();
    ctx.fill();

    ctx.beginPath();
    ctx.moveTo(attitudeIndMidX + wingOffset + wingWidth, attitudeIndMidY - wingThickness / 2);
    ctx.lineTo(attitudeIndMidX + wingOffset, attitudeIndMidY - wingThickness / 2);
    ctx.lineTo(attitudeIndMidX + wingOffset, attitudeIndMidY + wingHeight);
    ctx.lineTo(attitudeIndMidX + wingOffset + wingThickness, attitudeIndMidY + wingHeight);
    ctx.lineTo(attitudeIndMidX + wingOffset + wingThickness, attitudeIndMidY + wingThickness / 2);
    ctx.lineTo(attitudeIndMidX + wingOffset + wingWidth, attitudeIndMidY + wingThickness / 2);
    ctx.lineTo(attitudeIndMidX + wingOffset + wingWidth, attitudeIndMidY - wingThickness / 2);
    ctx.lineWidth = 4;
    ctx.fillStyle = 'black';
    ctx.strokeStyle = yellowColor;
    ctx.stroke();
    ctx.fill();

    ctx.strokeRect(attitudeIndMidX - wingThickness / 2, attitudeIndMidY - wingThickness / 2, wingThickness, wingThickness);

    // Radio altitude
    if ((radioAltitude !== undefined) && (radioAltitude <= 2500)) {
        ctx.font = "30px Arial";
        if (radioAltitude < 100) {
            ctx.fillStyle = orangeColor;
        } else {
            ctx.fillStyle = greenColor;
        }
        const raText = Math.round(radioAltitude).toString();
        const raTextMetrics = ctx.measureText(raText);
        ctx.fillText(raText, attitudeIndMidX - raTextMetrics.width / 2, attitudeIndTop + attitudeIndHeight - 4);
    }
}

function drawPressure(ctx, fm20, fm30) {

    if (pressure !== undefined) {

        const pressureLeft = 750; // Righthand side of altitude indicator
        const pressureMidX = 750 + 80; // Lefthand side of altitude indicator
        const pressureMidY = 850;

        const pressureInt = Math.round(pressure);

        if (isStd()) {

            const border = 3;
            ctx.font = "30px Arial";
            ctx.fillStyle = cyanColor;
            const pressureText = "STD";
            const pressureMetrics = ctx.measureText(pressureText);
            const height = pressureMetrics.actualBoundingBoxAscent + pressureMetrics.actualBoundingBoxDescent + 2 * border;
            ctx.fillText(pressureText, pressureMidX - pressureMetrics.width / 2, pressureMidY);

            ctx.lineWidth = 2;
            ctx.strokeStyle = yellowColor;
            ctx.strokeRect(pressureMidX - pressureMetrics.width / 2 - border, pressureMidY - pressureMetrics.actualBoundingBoxAscent - 2 * border, pressureMetrics.width + border * 2, height + border * 2);

        } else {
            ctx.font = "30px Arial";
            ctx.fillStyle = "white";
            ctx.fillText("QNH", pressureLeft, pressureMidY);

            ctx.fillStyle = cyanColor;
            const pressureText = pressureInt.toString();
            ctx.fillText(pressureText, pressureMidX, pressureMidY);
        }
    }
}

function drawMachIndicator(ctx) {
    if (mach !== undefined) {
        if (mach >= 0.45) { // Actually comes on at .5 and goes off at 0.45

            ctx.font = "38px Arial";
            ctx.fillStyle = greenColor;
            var machString;
            if (mach < 1) {
                machString = mach.toFixed(3).substring(1); // No leading 0
            } else {
                machString = mach.toFixed(2);
            }

            ctx.fillText(machString, 40, 840);
        }
    }
}
function drawVerticalSpeedIndicator(ctx, fm20, fm30) {

    const verticalSpeedIndWidth = 50;
    const verticalSpeedIndHeight = 620;
    const verticalSpeedIndLeft = 900;
    const verticalSpeedIndTop = 190;
    const verticalSpeedIndBottom = verticalSpeedIndTop + verticalSpeedIndHeight;
    const verticalSpeedIndMidY = verticalSpeedIndTop + + verticalSpeedIndHeight / 2;
    const verticalSpeedIndMidX = verticalSpeedIndLeft + verticalSpeedIndWidth / 2;
    const verticalSpeedIndRight = verticalSpeedIndLeft + verticalSpeedIndWidth;
    const verticalSpeedBevel = 80;
    const verticalSpeedMarkerSpacing = 35;
    const verticalSpeedMarkerWidth = verticalSpeedIndWidth / 4;
    const verticalSpeedMarkerLeft = verticalSpeedIndLeft + verticalSpeedMarkerWidth;
    const verticalSpeedIndText = verticalSpeedIndLeft + 1;
    const verticalSpeedLineRight = verticalSpeedIndRight + 30;
    const verticalSpeedLineLeft = verticalSpeedIndMidX - 5;


    // Background
    ctx.beginPath();
    ctx.moveTo(verticalSpeedIndLeft, verticalSpeedIndTop);
    ctx.lineTo(verticalSpeedIndMidX, verticalSpeedIndTop);
    ctx.lineTo(verticalSpeedIndRight, verticalSpeedIndTop + verticalSpeedBevel);
    ctx.lineTo(verticalSpeedIndRight, verticalSpeedIndBottom - verticalSpeedBevel);
    ctx.lineTo(verticalSpeedIndMidX, verticalSpeedIndBottom);
    ctx.lineTo(verticalSpeedIndLeft, verticalSpeedIndBottom);
    ctx.lineTo(verticalSpeedIndLeft, verticalSpeedIndTop);
    ctx.fillStyle = grayColor;
    ctx.fill();

    // Labels
    ctx.font = "20px Arial";
    ctx.fillStyle = 'white';
    ctx.fillText("6", verticalSpeedIndText, verticalSpeedIndMidY - 8 * verticalSpeedMarkerSpacing + fm20.actualBoundingBoxAscent / 2);
    ctx.fillText("2", verticalSpeedIndText, verticalSpeedIndMidY - 6 * verticalSpeedMarkerSpacing + fm20.actualBoundingBoxAscent / 2);
    ctx.fillText("1", verticalSpeedIndText, verticalSpeedIndMidY - 4 * verticalSpeedMarkerSpacing + fm20.actualBoundingBoxAscent / 2);
    ctx.fillText("1", verticalSpeedIndText, verticalSpeedIndMidY + 4 * verticalSpeedMarkerSpacing + fm20.actualBoundingBoxAscent / 2);
    ctx.fillText("2", verticalSpeedIndText, verticalSpeedIndMidY + 6 * verticalSpeedMarkerSpacing + fm20.actualBoundingBoxAscent / 2);
    ctx.fillText("6", verticalSpeedIndText, verticalSpeedIndMidY + 8 * verticalSpeedMarkerSpacing + fm20.actualBoundingBoxAscent / 2);

    // Markers
    ctx.fillStyle = 'white';
    for (var i = -8; i <= 8; i++)
    {
        const j = Math.abs(i);
        if (!((j == 1) || (j == 3))) {
            var height;
            if ((j == 4) || (j == 6) || (j == 8)) {
                height = 6;
            } else {
                height = 2;
            }
            ctx.fillRect(verticalSpeedMarkerLeft, verticalSpeedIndMidY - i * verticalSpeedMarkerSpacing - height / 2, verticalSpeedMarkerWidth, height);
        }
    }

    // 0 marker
    ctx.fillStyle = yellowColor;
    ctx.fillRect(verticalSpeedIndLeft, verticalSpeedIndMidY - 3, verticalSpeedIndWidth / 2, 6);

    // Indicator line

    const verticalSpeedAbsolute = Math.abs(verticalSpeed);
    var verticalOffset;
    var color = greenColor;

    if (verticalSpeedAbsolute <= 1000) {
        verticalOffset = verticalSpeedAbsolute / 1000 * 4 * verticalSpeedMarkerSpacing;
    } else if (verticalSpeedAbsolute < 2000) {
        verticalOffset = (verticalSpeedAbsolute - 1000) / 1000 * 2 * verticalSpeedMarkerSpacing + 4 * verticalSpeedMarkerSpacing;
    } else if (verticalSpeedAbsolute < 6000) {
        verticalOffset = (verticalSpeedAbsolute - 2000) / 4000 * 2 * verticalSpeedMarkerSpacing + 6 * verticalSpeedMarkerSpacing;
    } else {
        verticalOffset = 8 * verticalSpeedMarkerSpacing;
        color = orangeColor;
    }

    const verticalSpeedLineY = verticalSpeedIndMidY - verticalOffset * Math.sign(verticalSpeed);

    ctx.beginPath();
    ctx.moveTo(verticalSpeedLineRight, verticalSpeedIndMidY);
    ctx.lineTo(verticalSpeedLineLeft, verticalSpeedLineY);
    ctx.lineWidth = 4;
    ctx.strokeStyle = color;
    ctx.stroke();

    // Text
    if (verticalSpeedAbsolute > 200) {

        const verticalSpeedTextX = verticalSpeedLineLeft + 3;
        const verticalSpeedTextHeight = fm20.actualBoundingBoxAscent + fm20.actualBoundingBoxDescent + 2;
        const verticalSpeedText = Math.round(verticalSpeedAbsolute / 100);
        const verticalSpeedTextString = verticalSpeedText.toString().padStart(2, '0');

        // Background
        ctx.fillStyle = 'black';
        if (verticalSpeed > 0) {
            ctx.fillRect(verticalSpeedTextX, verticalSpeedLineY - verticalSpeedTextHeight, verticalSpeedIndWidth / 2, verticalSpeedTextHeight);
        } else {
            ctx.fillRect(verticalSpeedTextX, verticalSpeedLineY, verticalSpeedIndWidth / 2, verticalSpeedTextHeight);
        }

        ctx.fillStyle = color;
        if (verticalSpeed > 0) {
            ctx.fillText(verticalSpeedTextString, verticalSpeedTextX, verticalSpeedLineY - fm20.actualBoundingBoxDescent - 1);
        } else {
            ctx.fillText(verticalSpeedTextString, verticalSpeedTextX, verticalSpeedLineY + fm20.actualBoundingBoxAscent + 1);
        }

    }

}

// For testing only
var speedInc = 0.1;
var headingInc = 0.1;
var altitudeInc = 1;
var verticalSpeedInc = 10;
var rollInc = 0.1;
function animatePFD() {
    if (speed === undefined) {
        speed = 0;
        targetSpeed = 200;
    } else if (speed <= 0) {
        speedInc = 0.1;
    } else if (speed >= 350) {
        speedInc = -0.1;
    }
    if (heading === undefined) {
        heading = 0;
        targetHeading = 180;
    } else if (heading <= 0) {
        headingInc = 0.1;
    } else if (heading >= 360) {
        headingInc = -0.1;
    }
    if (altitude === undefined) {
        altitude = 0;
        targetAltitude = 1000;
    } else if (altitude <= 0) {
        altitudeInc = 1;
    } else if (altitude >= 45000) {
        altitudeInc = -1;
    }
    if (verticalSpeed === undefined) {
        verticalSpeed = -7000;
    } else if (verticalSpeed <= -7000) {
        verticalSpeedInc = 10;
    } else if (verticalSpeed >= 7000) {
        verticalSpeedInc = -10;
    }
    if (roll === undefined) {
        roll = 45;
    } else if (roll <= -45) {
        rollInc = 0.1;
    } else if (roll >= 45) {
        rollInc = -0.1;
    }
    speed = speed + speedInc;
    groundspeed = 150;
    trueAirspeed = 130;
    heading = heading + headingInc;
    altitude = altitude + altitudeInc;
    altitude = 39;
    radioAltitude = 1720;
    onSurface = 0;
    verticalSpeed = verticalSpeed + verticalSpeedInc;
    roll = roll + rollInc;
    callsign = "BAW123G";
    aircraftType = "A320";
    windDirection = 85;
    windSpeed = 15;
    staticAirTemperature = 7;
    verticalMode = 3;
    lateralMode = 2;
    autopilot = 1;
    tcasMode = 1;
    pressure = 1013;
    //pressure = 1018;
}
function drawPFD() {

    const canvas = document.getElementById("pfdCanvas");
    const ctx = canvas.getContext("2d");

    ctx.font = "20px Arial";
    const fm20 = ctx.measureText("180");
    ctx.font = "30px Arial";
    const fm30 = ctx.measureText("180");

    // Background

    ctx.fillStyle = "black";
    ctx.fillRect(0, 0, 1000, 1000);

    // Airspeed indicator

    drawAirspeedIndicator(ctx, fm20, fm30);

    // Attitude indicator

    drawAttitudeIndicator(ctx, fm20, fm30);

    // Altimeter

    drawAltitudeIndicator(ctx, fm20, fm30);

    // Vertical speed indicator

    drawVerticalSpeedIndicator(ctx, fm20, fm30);

    // Heading indicator

    drawHeadingIndicator(ctx, fm20, fm30);

    // Pressure setting

    drawPressure(ctx, fm20, fm30);

    // Mach indicator

    drawMachIndicator(ctx);

    // Flight mode announciators

    ctx.beginPath();
    ctx.moveTo(220, 5);
    ctx.lineTo(220, 140);
    ctx.lineWidth = 1;
    ctx.strokeStyle = "white";
    ctx.stroke();
    if (targetSpeed !== undefined) {
        ctx.font = "30px Arial";
        ctx.fillStyle = greenColor;
        ctx.fillText("SPEED", 55, 50); // THR CLB is climbing?
    } else if (toga) {
        ctx.font = "30px Arial";
        ctx.fillStyle = "white";
        ctx.fillText("TOGA", 70, 50);
    }

    // FIXME:
    //ctx.font = "30px Arial";
    //ctx.fillStyle = "white";
    //ctx.fillText("toga " + toga + " roll " + rollOut + " Surf " + onSurface + " wasSurf " + wasOnSurface60SecsAgo + " MS " + modelSpeed + " r/w Alt " + runwayAltitude, 50, 150);

    var landing = false;
    if (verticalMode !== undefined) {
        ctx.font = "30px Arial";
        ctx.fillStyle = greenColor;
        if (verticalMode == 1) {
            if (verticalSpeed !== undefined) {
                if (verticalSpeed > 0) {
                    ctx.fillText("CLB", 290, 50);
                } else if (verticalSpeed < 0) {
                    ctx.fillText("DES", 290, 50);
                } else {
                    ctx.fillText("ALT", 290, 50);
                }
            } else if (targetAltitude !== undefined && altitude !== undefined) {
                if (targetAltitude > altitude) {
                    ctx.fillText("CLB", 290, 50);
                } else if (targetAltitude < altitude) {
                    ctx.fillText("DES", 290, 50);
                } else {
                    ctx.fillText("ALT", 290, 50);
                }
            }
        } else if (verticalMode == 2) {
            ctx.fillText("ALT", 290, 50);
        } else if (verticalMode == 3) {
            if (rollOut) {
                ctx.fillText("ROLL OUT", 370, 50);
                landing = true;
            } else if ((onSurface === 0) && (radioAltitude < 40)) {
                ctx.fillText("FLARE", 390, 50);
                landing = true;
            } else if ((onSurface === 0) && (radioAltitude < 400)) {
                ctx.fillText("LAND", 400, 50);
                landing = true;
            } else {
                ctx.fillText("G/S", 290, 50);
            }
        }
    }
    if (!landing) {
        ctx.beginPath();
        ctx.moveTo(420, 5);
        ctx.lineTo(420, 140);
        ctx.lineWidth = 1;
        ctx.strokeStyle = "white";
        ctx.stroke();
    }

    ctx.beginPath();
    ctx.moveTo(660, 5);
    ctx.lineTo(660, 140);
    ctx.lineWidth = 1;
    ctx.strokeStyle = "white";
    ctx.stroke();
    if (lateralMode !== undefined) {
        ctx.font = "30px Arial";
        ctx.fillStyle = greenColor;
        if (lateralMode == 1) {
            ctx.fillText("NAV", 510, 50);
        } else if (lateralMode == 2) {
            if (!landing) {
                ctx.fillText("LOC", 510, 50);
            }
        }
    }

    ctx.beginPath();
    ctx.moveTo(850, 5);
    ctx.lineTo(850, 140);
    ctx.lineWidth = 1;
    ctx.strokeStyle = "white";
    ctx.stroke();
    ctx.font = "30px Arial";
    ctx.fillStyle = "white";
    if (autopilot !== undefined) {
        if (autopilot) {
            if (verticalMode == 3) {
                // We can't tell whether both APs are enabled - but they typically would be
                ctx.fillText("AP1+2", 870, 40);
                ctx.fillText("CAT3", 720, 40);
                ctx.fillText("DUAL", 720, 85);
            } else {
                ctx.fillText("AP1", 890, 40);
            }
        }
    }
    ctx.fillText("1 FD 2", 870, 85);
    if (((targetSpeed !== undefined) || autopilot) && !rollOut) {
        ctx.fillText("A/THR", 870, 130);
    }

    // Aircraft callsign and type

    ctx.fillStyle = "white";
    ctx.font = "30px Arial";
    if (callsign !== undefined) {
        ctx.fillText(callsign, 20, 940);
    }
    ctx.font = "21px Arial";
    if (aircraftType !== undefined) {
        ctx.fillText(aircraftType, 20, 980);
    }

    const dataLeft = 750; // Aligned with QNH
    const dataTop = 890;

    // TCAS - As displayed on ND/ECAM
    if (tcasMode === 1) {
        ctx.fillStyle = orangeColor;
        ctx.font = "21px Arial";
        ctx.fillText("TCAS STBY", dataLeft, dataTop);
    } else if (tcasMode === 2) {
        ctx.fillStyle = "white";
        ctx.font = "21px Arial";
        ctx.fillText("TA ONLY", dataLeft, dataTop);
    }

    // Groundspeed and TAS as displayed on ND
    if ((groundspeed !== undefined) || (trueAirspeed !== undefined)) {
        ctx.font = "21px Arial";
        if (groundspeed !== undefined) {
            ctx.fillStyle = "white";
            ctx.fillText("GS", dataLeft, dataTop + 30);
            ctx.fillStyle = greenColor;
            ctx.fillText(Math.round(groundspeed).toString(), dataLeft + 40, dataTop + 30);
        }
        if (trueAirspeed !== undefined) {
            ctx.fillStyle = "white";
            ctx.fillText("TAS", dataLeft + 90, dataTop + 30);
            ctx.fillStyle = greenColor;
            ctx.fillText(Math.round(trueAirspeed).toString(), dataLeft + 140, dataTop + 30);
        }
    }

    // Wind speed and direction as displayed on ND
    if ((windSpeed !== undefined) && (windDirection !== undefined)) {
        const windText = pad(Math.round(windDirection), 3) + "/" + Math.round(windSpeed);
        ctx.fillStyle = greenColor;
        ctx.font = "21px Arial";
        ctx.fillText(windText, dataLeft, dataTop + 60);

        if (heading !== undefined) {
            // An arrow showing wind direction relative to heading
            var angle = (windDirection - heading) * Math.PI / 180;

            ctx.save();
            ctx.translate(dataLeft + 80, dataTop + 53);
            ctx.rotate(angle);

            ctx.strokeStyle = greenColor;
            ctx.beginPath();
            ctx.moveTo(0, -10);
            ctx.lineTo(0, 10);
            ctx.lineTo(-5, 7);
            ctx.moveTo(0, 10);
            ctx.lineTo(5, 7);
            ctx.stroke();

            ctx.restore();
        }
    }

    // Static air temperature as displayed on lower ECAM
    if (staticAirTemperature !== undefined) {
        var satText;
        if (staticAirTemperature > 0) {
            satText = "+" + Math.round(staticAirTemperature);
        } else {
            satText = Math.round(staticAirTemperature).toString();
        }

        ctx.font = "21px Arial";
        ctx.fillStyle = "white";
        ctx.fillText("SAT", dataLeft, dataTop + 90);
        ctx.fillStyle = greenColor;
        ctx.fillText(satText, dataLeft + 60, dataTop + 90);
        ctx.fillStyle = cyanColor;
        ctx.fillText("\u00B0C", dataLeft + 100, dataTop + 90);
    }
}

const canvas = document.getElementById('pfdCanvas');
canvas.onmousedown = pfdMouseDown;
canvas.onmouseup = pfdMouseUp;
canvas.onmousemove = pfdMouseMove;
canvas.onmouseleave = pfdMouseLeave;
canvas.onmouseenter = pfdMouseEnter;
var docMouseMove = null;
var docMouseUp = null;
var movePFD = false;
var scalePFD = false;
var posX = 0;
var posY = 0;
var canvasWidth = 0;
var canvasHeight = 0;

function pad(num, size) {
    num = num.toString();
    while (num.length < size) num = "0" + num;
    return num;
}

function pfdMouseDown(e) {
    e.preventDefault();
    e.stopPropagation();
    const r = canvas.getBoundingClientRect();
    posX = r.left;
    posY = r.top;
    canvasWidth = r.width;
    canvasHeight = r.height;
    if ((e.offsetX > 0.95 * canvasWidth) && (e.offsetY > 0.95 * canvasHeight)) {
        scalePFD = true;
    } else {
        movePFD = true;
    }
}
function pfdMouseUp(e) {
    e.preventDefault();
    e.stopPropagation();
    movePFD = false;
    scalePFD = false;
    pfdRestoreDocMouse();
}
function pfdMouseMove(e) {
    e.preventDefault();
    e.stopPropagation();
    if (movePFD) {
        posX = posX + e.movementX;
        posY = posY + e.movementY;
        canvas.style.position = "absolute";
        canvas.style.left = posX + "px";
        canvas.style.top = posY + "px";
    } else if (scalePFD) {
        canvasWidth = canvasWidth + e.movementX;
        canvasHeight = canvasHeight + e.movementY;
        const constrainedWidth = Math.max(canvasWidth, 250);
        const constrainedHeight = Math.max(canvasHeight, 250);
        canvas.style.width = constrainedWidth + "px";
        canvas.style.height = constrainedHeight + "px";
    }
}

function pfdMouseLeave(e) {
    //movePFD = false;
    //scalePFD = false;
    if (scalePFD || movePFD) {
        docMouseUp = document.onmouseup;
        docMouseMove = document.onmousemove;
        document.onmouseup = pfdMouseUp;
        document.onmousemove = pfdMouseMove;
    }
}

function pfdRestoreDocMouse() {
    if (docMouseUp) {
        document.onmouseup = docMouseUp;
        document.onmousemove = docMouseMove;
        docMouseDown = null;
        docMouseMove = null;
    }
}
function pfdMouseEnter(e) {
    pfdRestoreDocMouse();
}
