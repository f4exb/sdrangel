// PFD (Primary Flight Display) - Airbus (A320) style by default, Boeing (737NG/777)
// style when the selected aircraft's type is a Boeing. See the themes below cockpitThemeFor()

var icao;
var callsign;
var aircraftType;
var onSurface;
var wasOnSurface60SecsAgo;
var speed;
var modelSpeed; // Speed of 3D model in Cesium
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

var toga = false;
var rollOut = false;
var runwayAltitude;

// State for the aural callouts, so each one is played once, on a crossing
var previousRadioAltitude;
var previousSpeed;
// TCAS resolution advisory bitfield: bit 0 active, 1 corrective, 2 downward sense,
// 3 positive, 4 clear of conflict, 5 multiple threats. The previous value gives the
// edges the aurals fire on
var tcasRA = 0;
var previousTcasRA = 0;
// Lowest first, so when more than one threshold is crossed between radio altitude
// updates only the lowest - the most urgent - is called
const radioAltCallouts = [5, 10, 30, 40, 50, 100, 500, 1000, 2500];

// The audio files are served from the qrc audio directory, a sibling of the
// directory map3d.html is in
function pfdPlayAudio(name) {
    try {
        const audio = new Audio("../audio/" + name);
        // The cockpit volume slider in Map Display Settings - map3d.html holds the value
        audio.volume = (typeof cockpitSoundVolume !== "undefined") ? cockpitSoundVolume : 1.0;
        // play() returns a promise, which rejects rather than throws on failure
        audio.play().catch((e) => {
            console.log("PFD audio " + name + " failed: " + e);
        });
    } catch (e) {
        console.log("PFD audio failed: " + e);
    }
}

function pfdIsAirbus(type) {
    // ICAO type designators: the A3xx families plus the A320neo family A19N/A20N/A21N
    return (type !== undefined) && (/^A3[0-9][0-9]/.test(type) || /^A(19|20|21)N/.test(type));
}

const grayColor = "#6A75AE";
const redColor = "#FC2020";
const greenColor = "#20E966";
const cyanColor = "#2FF7FE";
const yellowColor = "#F4F82F";
const orangeColor = "#FEDA30";
const skyColor = "#26C9FF";
const groundColor = "#D35E34";
const magentaColor = "#FF2CF5";

// Manufacturer look. Everything that differs between an Airbus and a Boeing display -
// colours, symbols, mode vocabulary, layout - comes from one of these two themes,
// chosen from the selected aircraft's ICAO type. Airbus is the default for every other
// type. navdisplay.js (loaded after this file) uses the nd section
const airbusTheme = {
    name: "airbus",
    sky: skyColor,
    ground: groundColor,
    tape: grayColor,                // Tape backgrounds (speed, altitude, heading, VSI)
    symbolColor: yellowColor,       // Aircraft symbol, pointers, lubber line, zero marker
    targetColor: cyanColor,         // Selected speed / altitude / heading
    readoutColor: greenColor,       // Current altitude digits
    readoutBoxColor: yellowColor,
    vsiColor: greenColor,
    machColor: greenColor,
    raltColor: greenColor,
    raltLowColor: orangeColor,      // Radio altitude below 100 ft
    baroColor: cyanColor,
    baroBox: true,                  // STD in a box
    armedColor: cyanColor,          // Armed FMA modes
    adi: "circle",                  // Attitude indicator outline
    vsi: "airbus",                  // Filled strip, or a Boeing outline
    tapeTop: 250,                   // Speed and altitude tapes, which on a Boeing are
    tapeHeight: 500,                // taller than the attitude indicator between them
    vsiTop: 190,
    vsiHeight: 620,
    vsiSpacing: 35,                 // Per marker on the vertical speed scale
    machY: 840,
    baroY: 850,                     // Clear of the off-scale selected altitude above it
    trendColor: yellowColor,        // Speed trend vector
    altitudeLabels: "fl",           // Flight levels on the tape, or full altitudes
    fdLabel: undefined,             // Boeing annunciates the flight director in words
    headingReadout: false,          // Digits under the heading display
    wingHeight: 35,                 // Aircraft symbol: length of the inner leg
    wingThickness: 14,
    fmaTcas: true,                  // TCAS annunciated as a vertical mode
    fdColor: greenColor,            // Flight director cross bars
    pitchBrackets: true,            // The green brackets at -15 deg pitch
    bankBoxes: true,                // Bank angle graduations as boxes, or plain ticks
    bankArc: true,                  // An arc drawn through the bank graduations
    rollPointer: true,              // Fixed zero bank index above the scale
    headingBackground: false,       // Grey behind the heading display, as the tapes have
    speedReadout: "pointer",        // Current speed as a pointer onto the tape
    headingStyle: "tape",           // Heading tape under the ADI
    fmaHighlight: false,            // Box a mode for 10 s after it changes
    landingSpans: true,             // LAND / FLARE / ROLL OUT span both mode columns
    verticalX: 290,                 // FMA column for the vertical (pitch) mode
    lateralX: 510,                  // FMA column for the lateral (roll) mode
    labels: {
        speed: "SPEED", toga: "TOGA", clb: "CLB", des: "DES", alt: "ALT", gs: "G/S",
        land: "LAND", flare: "FLARE", rollout: "ROLL OUT", nav: "NAV", loc: "LOC"
    },
    nd: {
        lubber: yellowColor,
        compassTicks: "outward",    // Graduations point out of the compass ring
        rangeRings: "dashed",
        aircraft: "airbus",         // Stick aircraft
        track: greenColor,
        trackDiamond: true,
        route: greenColor,          // Route legs
        activeRoute: "white",       // The leg to the active waypoint
        waypoint: greenColor,       // Waypoint symbols and names
        activeWaypoint: "white",
        selectedHeading: cyanColor,
        selectedHeadingLine: false, // Dashed line from own ship to the bug
        headingBox: "airbus",       // Grey box with the digits
        rangeLabel: cyanColor,
        airports: cyanColor,
        airportSymbol: "star",
        navaids: "white",
        dataValue: greenColor,      // GS / TAS / wind / distance values
        unit: cyanColor,
        waypointIdent: "white",     // Active waypoint ident, top right
        showSat: true,
        showDepArr: true,
        tcas: "airbus"              // TCAS STBY / TA ONLY / TCAS RA box
    }
};

const boeingTheme = {
    name: "boeing",
    sky: "#2C7FD6",
    ground: "#A87A31",
    tape: "#3A3A44",
    symbolColor: "white",
    targetColor: magentaColor,
    readoutColor: "white",
    readoutBoxColor: "white",
    vsiColor: "white",
    machColor: "white",
    raltColor: "white",
    raltLowColor: "white",
    baroColor: greenColor,
    baroBox: false,
    armedColor: "white",
    adi: "rounded",                 // Rounded square, not a circle
    vsi: "boeing",                  // Outlined scale, not a filled strip
    tapeTop: 165,                   // Taller than the 500 the attitude indicator spans,
    tapeHeight: 670,                // and centred on it
    vsiTop: 150,
    vsiHeight: 700,
    vsiSpacing: 40,
    machY: 875,
    baroY: 895,
    trendColor: greenColor,
    altitudeLabels: "full",
    fdLabel: "FLT DIR",
    headingReadout: true,
    wingHeight: 22,                 // Shorter, thicker wings than the Airbus symbol
    wingThickness: 18,
    fmaTcas: false,                 // Boeing does not annunciate TCAS on the FMA
    fdColor: magentaColor,          // Flight director cross bars
    pitchBrackets: false,
    bankBoxes: false,
    bankArc: false,
    rollPointer: false,             // Boeing reads bank off the moving pointer alone,
                                    // which keeps the scale clear of FLT DIR above it
    headingBackground: true,
    speedReadout: "box",            // Current speed boxed on the tape
    headingStyle: "arc",            // Partial compass under the ADI
    fmaHighlight: true,
    landingSpans: false,            // FLARE is a pitch mode, ROLLOUT a roll mode
    verticalX: 510,                 // Boeing: roll left of pitch
    lateralX: 290,
    labels: {
        speed: "SPD", toga: "TO/GA", clb: "VNAV SPD", des: "VNAV PTH", alt: "ALT", gs: "G/S",
        land: "G/S", flare: "FLARE", rollout: "ROLLOUT", nav: "LNAV", loc: "LOC"
    },
    nd: {
        lubber: "white",
        compassTicks: "inward",
        rangeRings: "solid",
        aircraft: "boeing",         // Triangle
        track: "white",
        trackDiamond: false,
        route: magentaColor,
        activeRoute: magentaColor,
        waypoint: "white",
        activeWaypoint: magentaColor,
        selectedHeading: magentaColor,
        selectedHeadingLine: true,
        headingBox: "boeing",       // HDG xxx MAG
        rangeLabel: "white",
        airports: cyanColor,
        airportSymbol: "circle",
        navaids: cyanColor,
        dataValue: "white",
        unit: "white",
        waypointIdent: magentaColor,
        showSat: false,
        showDepArr: false,
        tcas: "boeing"              // TRAFFIC / TCAS OFF / TA ONLY
    }
};

function pfdIsBoeing(type) {
    // ICAO type designators: B7xx (B737-B789, B77W, B78X, B712...) and the 737 MAX B3xM
    return (type !== undefined) && (/^B7[0-9][0-9]/.test(type) || /^B3[0-9X]M/.test(type));
}

function cockpitThemeFor(type) {
    return pfdIsBoeing(type) ? boeingTheme : airbusTheme;
}

var pfdTheme = airbusTheme;

// ---------------------------------------------------------------------------------
// TCAS altitude inhibit. Near the ground a resolution advisory could fly the aircraft
// into terrain, and the ordinary traffic on and around a runway would raise advisories
// continuously, so TCAS II suppresses RAs and its aural annunciations below a height
// above ground. Traffic advisories are still shown - the symbols are useful on the
// approach and the departure - they just stop speaking.
// The PFD and the ND always show the same aircraft, so the inhibit is worked out from
// whichever of them is being updated and read by both

const tcasInhibitHeight = 1000;   // ft AGL
var tcasInhibited = false;

// radioAlt in feet AGL, onGround the aircraft's on-surface flag. Either may be unknown,
// in which case the last answer stands rather than TCAS being silently disabled
function updateTcasInhibit(radioAlt, onGround) {
    if (onGround > 0) {
        tcasInhibited = true;
    } else if (radioAlt !== undefined) {
        tcasInhibited = (radioAlt < tcasInhibitHeight);
    }
    return tcasInhibited;
}

// ---------------------------------------------------------------------------------
// Cockpit panel furniture shared by the PFD, ND, EFIS, RMP and DCDU. Each is drawn in
// a 1000-unit wide logical space whose aspect ratio matches its displayed size, so one
// rectangle puts an equally sized close button in the top right corner of any of them

const panelCloseRect = { x: 956, y: 6, w: 32, h: 32 };

function panelDrawClose(ctx) {
    const r = panelCloseRect;
    ctx.fillStyle = "#26262E";
    ctx.fillRect(r.x, r.y, r.w, r.h);
    ctx.strokeStyle = "#8A8A95";
    ctx.lineWidth = 2;
    ctx.strokeRect(r.x, r.y, r.w, r.h);
    ctx.beginPath();
    ctx.moveTo(r.x + 9, r.y + 9);
    ctx.lineTo(r.x + r.w - 9, r.y + r.h - 9);
    ctx.moveTo(r.x + r.w - 9, r.y + 9);
    ctx.lineTo(r.x + 9, r.y + r.h - 9);
    ctx.strokeStyle = "white";
    ctx.lineWidth = 3;
    ctx.stroke();
}

function panelInClose(x, y) {
    const r = panelCloseRect;
    return (x >= r.x) && (x < r.x + r.w) && (y >= r.y) && (y < r.y + r.h);
}

// No socket in a test harness
function panelSend(obj) {
    if ((typeof socket !== "undefined") && socket) {
        socket.send(JSON.stringify(obj));
    }
}

// Closing a panel is remembered: the GUI unticks it in the toolbar panel menu and it
// stays closed until it is asked for again
function panelClose(name, canvas) {
    canvas.setAttribute("hidden", "hidden");
    panelSend({ event: "panelClosed", panel: name });
}

// Where the panel was left, in CSS pixels, saved at the end of a drag or a resize.
// A click that moved nothing reports nothing - it would only rewrite the same setting
const panelLastGeometry = new Map();

function panelSaveGeometry(name, canvas) {
    const r = canvas.getBoundingClientRect();
    const key = Math.round(r.left) + "," + Math.round(r.top) + "," + Math.round(r.width) + "," + Math.round(r.height);
    if (panelLastGeometry.get(name) === key) {
        return;
    }
    panelLastGeometry.set(name, key);
    panelSend({
        event: "panelGeometry", panel: name,
        left: Math.round(r.left), top: Math.round(r.top),
        width: Math.round(r.width), height: Math.round(r.height)
    });
}

function panelApplyGeometry(canvas, g) {
    if ((canvas === null) || (canvas === undefined) || (g === undefined) || (g === null)) {
        return;
    }
    if ((g.w > 0) && (g.h > 0)) {
        canvas.style.width = g.w + "px";
        canvas.style.height = g.h + "px";
    }
    if ((g.l !== undefined) && (g.t !== undefined)) {
        // Clamped, so a panel saved against a larger window is still reachable
        canvas.style.position = "absolute";
        canvas.style.left = Math.max(0, Math.min(g.l, window.innerWidth - 80)) + "px";
        canvas.style.top = Math.max(0, Math.min(g.t, window.innerHeight - 40)) + "px";
    }
}

function setPFDData(forward, newICAO, newCallsign, newAircraftType, newOnSurface, newWasOnSurface60SecsAgo, newRunwayAltitudeEstimate,
    newModelSpeed, newIndicatedAirspeed, newGroundspeed, newMach, newAltitude, newRadioAltitude, newQnh, newVerticalSpeed,
    newHeading, newTrack, newRoll, newSelectedAltitude, newSelectedHeading,
    newAutopilot, newVerticalMode, newLateralMode, newTcasRA, newPitch) {

    //console.log('PFD', newIndicatedAirspeed, newMach, newAltitude, newQnh, newVerticalSpeed, newHeading, newTrack, newRoll, newSelectedAltitude, newSelectedHeading);

    const newAircraft = icao !== newICAO;

    if (newAircraft) {
        toga = false;
        rollOut = false;
        runwayAltitude = undefined;
        previousRadioAltitude = undefined;
        previousSpeed = undefined;
        previousTcasRA = 0;
        // Height above ground is unknown for a moment after a selection change, and the
        // inhibit holds its last answer while it is - which would be the old aircraft's
        tcasInhibited = false;
        pfdPitch = 0;
        pfdFmaState = {};
        trendReset();
    }

    icao = newICAO;
    callsign = newCallsign;
    aircraftType = newAircraftType;
    pfdTheme = cockpitThemeFor(aircraftType);
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
    mach = newMach;
    altitude = newAltitude;
    radioAltitude = newRadioAltitude;
    pressure = newQnh;
    verticalSpeed = newVerticalSpeed;
    reportedPitch = newPitch;
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
    tcasRA = (newTcasRA === undefined) ? 0 : newTcasRA;
    // Below the inhibit height the RA is dropped entirely, so neither the flight mode
    // annunciator nor the vertical speed guidance bands show one
    if (updateTcasInhibit(radioAltitude, onSurface)) {
        tcasRA = 0;
    }

    updateSpeedTrend(forward);
    updatePitch();

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

    // Aural callouts. setPFDData only runs while the PFD is displayed, so they are
    // only heard when it is. Each fires on a crossing in the direction flown, so
    // nothing repeats and playback in reverse is silent.
    if (forward)
    {
        // Radio altitude callouts on approach
        if ((radioAltitude !== undefined) && (previousRadioAltitude !== undefined)
            && (radioAltitude < previousRadioAltitude))
        {
            for (const callout of radioAltCallouts)
            {
                if ((previousRadioAltitude > callout) && (radioAltitude <= callout))
                {
                    pfdPlayAudio(callout + ".wav");
                    break;
                }
            }
            // The RETARD callout, which only Airbus aircraft make
            if ((previousRadioAltitude > 20) && (radioAltitude <= 20) && pfdIsAirbus(aircraftType)) {
                pfdPlayAudio("retard.wav");
            }
        }
        // V1 when accelerating through 120 knots on the takeoff roll
        if ((speed !== undefined) && (previousSpeed !== undefined) && (onSurface > 0)
            && (previousSpeed < 120) && (speed >= 120)) {
            pfdPlayAudio("v1.wav");
        }
        // TCAS resolution advisory aurals: the RA sound on becoming active or on a
        // change of sense while active, clear of conflict when it ends (whether
        // signalled as terminated or simply no longer broadcast). Silent below the
        // inhibit height, including the clear of conflict the inhibit itself provokes
        // by zeroing the RA
        const raActive = (tcasRA & 1) !== 0;
        const previousRaActive = (previousTcasRA & 1) !== 0;
        if (tcasInhibited)
        {
            // Nothing to say
        }
        else if (raActive && (!previousRaActive || ((tcasRA & 70) !== (previousTcasRA & 70))))
        {
            if ((tcasRA & 2) && !(tcasRA & 64)) {
                pfdPlayAudio((tcasRA & 4) ? "descend.wav" : "climb.wav");
            } else {
                pfdPlayAudio("traffic.wav");
            }
        }
        else if (!raActive && previousRaActive)
        {
            pfdPlayAudio("clear-of-conflict.wav");
        }
    }
    previousRadioAltitude = radioAltitude;
    previousSpeed = speed;
    previousTcasRA = tcasRA;
}

// ---------------------------------------------------------------------------------
// Speed trend vector: where the speed will be in ten seconds at the current
// acceleration. ADS-B carries no acceleration, so it is differentiated from the speed
// itself - reports arrive every few seconds and are quantised, so each sample spans at
// least one report and the result is smoothed before it is drawn

const trendSeconds = 10;
const trendMinKnots = 6;        // Below this the arrow is not worth drawing
const trendInterval = 2000;     // Milliseconds a sample spans, so it covers a report
const trendSmoothing = 0.3;
var trendAcceleration = 0;      // Knots per second
var trendLastSpeed;
var trendLastTime;

function trendReset() {
    trendAcceleration = 0;
    trendLastSpeed = undefined;
    trendLastTime = undefined;
}

function updateSpeedTrend(forward) {
    // Nothing to predict when the track is being played backwards
    if (!forward || (speed === undefined) || isNaN(speed)) {
        trendReset();
        return;
    }
    const now = Date.now();
    if (trendLastSpeed === undefined) {
        trendLastSpeed = speed;
        trendLastTime = now;
        return;
    }
    const dt = now - trendLastTime;
    if (dt < trendInterval) {
        return;
    }
    const acceleration = (speed - trendLastSpeed) / (dt / 1000);
    trendAcceleration = trendAcceleration * (1 - trendSmoothing) + acceleration * trendSmoothing;
    trendLastSpeed = speed;
    trendLastTime = now;
}

function isStd() {
    return (pressure >= 1012) && (pressure < 1014);
}

function drawAirspeedIndicator(ctx, fm20, fm30) {
    // Airspeed indicator

    const speedIndWidth = 100;
    const speedIndHeight = pfdTheme.tapeHeight;
    const speedIndLeft = 30;
    const speedIndRight = speedIndLeft + speedIndWidth;
    const speedIndTop = pfdTheme.tapeTop;
    const speedIndBottom = speedIndTop + speedIndHeight;
    const speedIndMid = speedIndTop + speedIndHeight / 2;

    // Background
    ctx.fillStyle = pfdTheme.tape;
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
    var speedTape = Math.round(speed - speed % speedTextStep + 8 * speedTapeStep);
    for (var i = -8; i < 8; i++) {
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
    var speedText = Math.round(speed - speed % speedTextStep + 4 * speedTextStep);
    for (var i = -4; i < 4; i++) {
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
    if (pfdTheme.speedReadout === "box") {
        // Boeing: the current speed in a box over the tape, pointing at it
        const boxHeight = 56;
        const boxLeft = speedIndLeft - 10;
        const boxRight = speedIndRight + 5;
        ctx.beginPath();
        ctx.moveTo(boxLeft, speedIndMid - boxHeight / 2);
        ctx.lineTo(boxRight, speedIndMid - boxHeight / 2);
        ctx.lineTo(boxRight, speedIndMid - 12);
        ctx.lineTo(boxRight + 18, speedIndMid);
        ctx.lineTo(boxRight, speedIndMid + 12);
        ctx.lineTo(boxRight, speedIndMid + boxHeight / 2);
        ctx.lineTo(boxLeft, speedIndMid + boxHeight / 2);
        ctx.closePath();
        ctx.fillStyle = "black";
        ctx.fill();
        ctx.lineWidth = 3;
        ctx.strokeStyle = "white";
        ctx.stroke();
        if ((speed !== undefined) && !isNaN(speed)) {
            ctx.font = "36px Arial";
            ctx.fillStyle = "white";
            const speedString = Math.round(speed).toString().padStart(3, '0');
            const speedMetrics = ctx.measureText(speedString);
            ctx.fillText(speedString, boxRight - 8 - speedMetrics.width, speedIndMid + speedMetrics.actualBoundingBoxAscent / 2);
        }
    } else {
        ctx.beginPath();
        ctx.moveTo(speedIndLeft - 10, speedIndMid);
        ctx.lineTo(speedIndLeft, speedIndMid);
        ctx.moveTo(speedTapeLeft - 5, speedIndMid);
        ctx.lineTo(speedTapeRight + 20, speedIndMid);
        ctx.lineWidth = 2;
        ctx.strokeStyle = pfdTheme.symbolColor;
        ctx.stroke();

        ctx.beginPath();
        ctx.moveTo(speedTapeRight + 10, speedIndMid);
        ctx.lineTo(speedTapeRight + 35, speedIndMid - 10);
        ctx.lineTo(speedTapeRight + 35, speedIndMid + 10);
        ctx.lineTo(speedTapeRight + 10, speedIndMid);
        ctx.fillStyle = pfdTheme.symbolColor;
        ctx.fill();
    }

    // Speed trend vector, from beside the readout to the speed ten seconds ahead
    const trendKnots = trendAcceleration * trendSeconds;
    if (Math.abs(trendKnots) >= trendMinKnots) {
        const up = (trendKnots > 0) ? -1 : 1;   // Faster is up the tape
        const trendX = speedIndRight + 10;
        const trendFrom = speedIndMid + up * 30;
        const trendTo = Math.max(speedIndTop, Math.min(speedIndBottom,
            speedIndMid - trendKnots * speedPerPixel));
        ctx.strokeStyle = pfdTheme.trendColor;
        ctx.fillStyle = pfdTheme.trendColor;
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.moveTo(trendX, trendFrom);
        ctx.lineTo(trendX, trendTo);
        ctx.stroke();
        // Arrow head at the predicted speed
        ctx.beginPath();
        ctx.moveTo(trendX, trendTo);
        ctx.lineTo(trendX - 7, trendTo - 14 * up);
        ctx.lineTo(trendX + 7, trendTo - 14 * up);
        ctx.closePath();
        ctx.fill();
    }

    // Target speed

    if (targetSpeed !== undefined) {
        var targetSpeedY = (speed - targetSpeed) * speedPerPixel;
        targetSpeedY = targetSpeedY + speedIndMid;
        if (targetSpeedY < speedIndTop) {
            // Target speed as text on top
            ctx.font = "20px Arial";
            ctx.fillStyle = pfdTheme.targetColor;
            const targetSpeedMetrics = ctx.measureText(targetSpeed);
            ctx.fillText(targetSpeed, speedIndRight - targetSpeedMetrics.width / 2, speedIndTop - 4);

        } else if (targetSpeedY > speedIndBottom) {
            // Target speed as text underneath
            ctx.font = "20px Arial";
            ctx.fillStyle = pfdTheme.targetColor;
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
            ctx.strokeStyle = pfdTheme.targetColor;
            ctx.stroke();
        }
    }
}

function drawAltitudeIndicator(ctx, fm20, fm30) {
    // Altitude indicator

    const altitudeIndWidth = 70;
    const altitudeIndHeight = pfdTheme.tapeHeight;
    const altitudeIndLeft = 750;
    const altitudeIndRight = altitudeIndLeft + altitudeIndWidth;
    const altitudeIndTop = pfdTheme.tapeTop;
    const altitudeIndBottom = altitudeIndTop + altitudeIndHeight;
    const altitudeIndMid = altitudeIndTop + altitudeIndHeight / 2;

    // Background
    ctx.fillStyle = pfdTheme.tape;
    ctx.fillRect(altitudeIndLeft, altitudeIndTop, altitudeIndWidth, altitudeIndHeight);

    ctx.font = "30px Arial";
    const greaterThanMetrics = ctx.measureText(">");

    // Mid point marker (for glideslope, but always visible)
    ctx.fillStyle = pfdTheme.symbolColor;
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
    for (var i = -12; i < 12; i++) {
        ctx.moveTo(altitudeTapeLeft, altitudeOffset + i * altitudeTapeSpacing + altitudeIndMid);
        ctx.lineTo(altitudeTapeRight, altitudeOffset + i * altitudeTapeSpacing + altitudeIndMid);
    }
    ctx.lineWidth = 2;
    ctx.strokeStyle = "white";
    ctx.stroke();

    // Flight level texts every 500ft
    ctx.font = "30px Arial";
    ctx.fillStyle = "white";
    var altitudeText = Math.round(altitude - altitude % altitudeTextStep + 3 * altitudeTextStep);
    for (var i = -3; i < 3; i++) {
        // Airbus marks the tape in flight levels, Boeing in full altitudes
        const flightLevelText = Math.abs(altitudeText / 100); // Don't display - if negative
        const flightLevelTextString = (pfdTheme.altitudeLabels === "full")
            ? Math.abs(altitudeText).toString()
            : ">" + flightLevelText.toString().padStart(3, '0');
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

        // The selected altitude reads as a flight level on an Airbus above the
        // transition altitude, but a Boeing always shows it in full
        const asFlightLevel = isStd() && (pfdTheme.altitudeLabels !== "full");
        var targetAltitudeText = asFlightLevel
            ? Math.round(targetAltitude / 100) : Math.round(targetAltitude);

        if (targetAltitudeY < altitudeIndTop) {
            // Target altitude as text on top
            ctx.font = "30px Arial";
            ctx.fillStyle = pfdTheme.targetColor;
            const targetAltitudeMetrics = ctx.measureText(targetAltitudeText);
            if (asFlightLevel) {
                ctx.fillText("FL", altitudeIndLeft, altitudeIndTop - 4);
            }
            ctx.fillText(targetAltitudeText, altitudeIndRight - targetAltitudeMetrics.width / 2, altitudeIndTop - 4);
        } else if (targetAltitudeY > altitudeIndBottom) {
            // Target altitude as text underneath
            ctx.font = "30px Arial";
            ctx.fillStyle = pfdTheme.targetColor;
            const targetAltitudeMetrics = ctx.measureText(targetAltitudeText);
            if (asFlightLevel) {
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
            ctx.fillStyle = pfdTheme.targetColor;
            const targetAltitudeMetrics = ctx.measureText(targetAltitudeText);

            // Cyan (Airbus) or magenta (Boeing) box
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
            ctx.strokeStyle = pfdTheme.targetColor;
            ctx.stroke();

            ctx.fillStyle = 'black';
            ctx.fillRect(altitudeIndLeft, targetAltitudeY - targetAltitudeBoxHeight / 2, altitudeIndWidth - 2, targetAltitudeBoxHeight);
            ctx.fillStyle = pfdTheme.targetColor;
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
    ctx.fillStyle = pfdTheme.readoutColor;
    if (levelText !== "") {
        ctx.fillText(levelText, altitudeIndRight - levelTextMetrics.width, altitudeIndMid + levelTextMetrics.actualBoundingBoxAscent / 2);
    }

    // Readout box - yellow on an Airbus, white on a Boeing
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
        ctx.strokeStyle = pfdTheme.readoutBoxColor;
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
// Boeing style heading indicator: a partial compass rose under the ADI, its centre well
// below the display, the current heading read against a pointer at the top. Occupies the
// same area as the Airbus heading tape
function drawHeadingArc(ctx, fm20, fm30) {
    if (heading === undefined) {
        return;
    }
    const areaLeft = 215;
    const areaRight = 685;
    const areaTop = 860;
    const areaBottom = 955;
    const cx = 450;
    const cy = 1330;
    const radius = 440; // Arc top at y = 890
    const visible = 33; // Degrees either side of the pointer

    ctx.save();
    ctx.beginPath();
    ctx.rect(areaLeft, areaTop, areaRight - areaLeft, areaBottom - areaTop);
    ctx.clip();

    // Background, the same grey as the tapes
    if (pfdTheme.headingBackground) {
        ctx.fillStyle = pfdTheme.tape;
        ctx.fillRect(areaLeft, areaTop, areaRight - areaLeft, areaBottom - areaTop);
    }

    // The arc itself
    ctx.beginPath();
    ctx.arc(cx, cy, radius, (-90 - visible) * Math.PI / 180, (-90 + visible) * Math.PI / 180);
    ctx.lineWidth = 2;
    ctx.strokeStyle = "white";
    ctx.stroke();

    // Ticks inside the arc every 5 degrees, numerals every 10 (tens digits, the 30s larger)
    ctx.textAlign = "center";
    const first = Math.floor((heading - visible) / 5) * 5;
    for (var deg = first; deg <= heading + visible; deg += 5) {
        const a = (deg - heading) * Math.PI / 180;
        const tens = ((deg % 10) + 10) % 10 === 0;
        const len = tens ? 20 : 12;
        ctx.beginPath();
        ctx.moveTo(cx + radius * Math.sin(a), cy - radius * Math.cos(a));
        ctx.lineTo(cx + (radius - len) * Math.sin(a), cy - (radius - len) * Math.cos(a));
        ctx.lineWidth = 2;
        ctx.strokeStyle = "white";
        ctx.stroke();
        if (tens) {
            var label = Math.round(deg / 10) % 36;
            if (label < 0) {
                label += 36;
            }
            ctx.font = (label % 3 === 0) ? "30px Arial" : "24px Arial";
            ctx.fillStyle = "white";
            ctx.save();
            ctx.translate(cx + (radius - 44) * Math.sin(a), cy - (radius - 44) * Math.cos(a));
            ctx.rotate(a);
            ctx.fillText(label.toString(), 0, 10);
            ctx.restore();
        }
    }

    // Track: a line from the rose centre, seen where it crosses the arc
    if (track !== undefined) {
        var rel = track - heading;
        while (rel > 180) rel -= 360;
        while (rel < -180) rel += 360;
        const a = rel * Math.PI / 180;
        ctx.beginPath();
        ctx.moveTo(cx + (radius - 70) * Math.sin(a), cy - (radius - 70) * Math.cos(a));
        ctx.lineTo(cx + (radius + 12) * Math.sin(a), cy - (radius + 12) * Math.cos(a));
        ctx.lineWidth = 3;
        ctx.strokeStyle = "white";
        ctx.stroke();
    }

    // Selected heading: a magenta bug riding the outside of the arc, or its value at
    // the nearer end when off the visible part
    if (targetHeading !== undefined) {
        var rel = targetHeading - heading;
        while (rel > 180) rel -= 360;
        while (rel < -180) rel += 360;
        ctx.strokeStyle = pfdTheme.targetColor;
        ctx.fillStyle = pfdTheme.targetColor;
        if (Math.abs(rel) <= visible) {
            const a = rel * Math.PI / 180;
            ctx.save();
            ctx.translate(cx + radius * Math.sin(a), cy - radius * Math.cos(a));
            ctx.rotate(a);
            ctx.beginPath();
            ctx.moveTo(-16, -18);
            ctx.lineTo(-8, 0);
            ctx.lineTo(8, 0);
            ctx.lineTo(16, -18);
            ctx.closePath();
            ctx.lineWidth = 3;
            ctx.stroke();
            ctx.restore();
        } else {
            ctx.font = "24px Arial";
            ctx.textAlign = (rel > 0) ? "right" : "left";
            ctx.fillText(Math.round(targetHeading).toString().padStart(3, '0'), (rel > 0) ? areaRight - 6 : areaLeft + 6, areaTop + 24);
        }
    }
    ctx.restore();

    // Heading pointer above the arc
    ctx.beginPath();
    ctx.moveTo(cx, 892);
    ctx.lineTo(cx - 12, 870);
    ctx.lineTo(cx + 12, 870);
    ctx.closePath();
    ctx.lineWidth = 3;
    ctx.strokeStyle = "white";
    ctx.stroke();

    // The heading in figures under the arc, with the reference it is measured from
    if (pfdTheme.headingReadout) {
        ctx.font = "32px Arial";
        ctx.fillStyle = "white";
        ctx.textAlign = "right";
        ctx.fillText(Math.round(heading).toString().padStart(3, '0'), cx + 14, 990);
        ctx.font = "24px Arial";
        ctx.fillStyle = greenColor;
        ctx.textAlign = "left";
        ctx.fillText("MAG", cx + 26, 990);
    }
    ctx.textAlign = "left";
}

function drawHeadingIndicator(ctx, fm20, fm30) {

    if (pfdTheme.headingStyle === "arc") {
        drawHeadingArc(ctx, fm20, fm30);
        return;
    }

    const headingIndWidth = 450;
    const headingIndHeight = 60;
    const headingIndLeft = 225;
    const headingIndRight = headingIndLeft + headingIndWidth;
    const headingIndTop = 890;
    const headingIndBottom = headingIndTop + headingIndHeight;
    const headingIndMid = headingIndLeft + headingIndWidth / 2;

    ctx.fillStyle = pfdTheme.tape;
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
    ctx.strokeStyle = pfdTheme.symbolColor;
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
            ctx.fillStyle = pfdTheme.targetColor;
            const targetHeadingText = Math.round(targetHeading).toString().padStart(3, '0');
            const targetHeadingMetrics = ctx.measureText(targetHeadingText);
            ctx.fillText(targetHeadingText, headingIndLeft, headingIndTop - 4);

        } else if (targetMid > headingIndRight) {

            // Display target heading as text on right
            ctx.font = "20px Arial";
            ctx.fillStyle = pfdTheme.targetColor;
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
            ctx.strokeStyle = pfdTheme.targetColor;
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
// ---------------------------------------------------------------------------------
// Pitch. ADS-B carries no attitude, but it does carry vertical speed and speed, and
// those give the flight path angle - the angle of the aircraft's path through the air.
// That is what the attitude indicator's ladder is read against here. True pitch is the
// flight path angle plus the angle of attack, and nothing reported tells us the angle of
// attack, so in a climb the ladder reads a few degrees shallower than a real one would

const pfdPitchScale = 10;           // Pixels per degree on the pitch ladder
const pfdMaxPitch = 30;             // Clamped, so a wild vertical speed cannot throw the horizon off
const pfdPitchSmoothing = 0.15;     // Per frame - reported vertical speed is coarse and steps about
const pfdMinPathSpeed = 50;         // Knots, below which the angle is meaningless
// An aircraft flies a few degrees nose up of its own flight path. Only used for map items
// with no pitch of their own - for an ADS-B aircraft the demod has already added it
const pfdAngleOfAttack = 3;
var pfdPitch = 0;                   // Degrees, nose up positive, as drawn
var reportedPitch;                  // What the item's own source estimated, when it has one

// How fast the aircraft is going forwards. The speed of the 3D model is measured from
// its own track, so it is ground referenced; the indicated airspeed the tapes show is
// far slower than that at altitude and would overstate the angle
function pfdPathSpeed() {
    if ((modelSpeed !== undefined) && (modelSpeed > pfdMinPathSpeed)) {
        return modelSpeed;
    }
    return ((speed !== undefined) && (speed > pfdMinPathSpeed)) ? speed : undefined;
}

function flightPathAngle() {
    const v = pfdPathSpeed();
    if ((v === undefined) || (verticalSpeed === undefined)) {
        return 0;
    }
    return Math.atan2(verticalSpeed, v * ftPerMinutePerKnot) * 180 / Math.PI;
}

// The ADS-B demod works pitch out from the aircraft's own reports and orients its 3D
// model with it, so taking that here keeps the attitude indicator and the model on the
// map showing the same thing - and picks up the attitudes it sets on rotation, gear up
// and the approach, which no calculation from the reported rates would give. Anything on
// the map without a source for it falls back to the flight path angle
function updatePitch() {
    const source = (reportedPitch !== undefined) ? reportedPitch
        : (flightPathAngle() + pfdAngleOfAttack);
    const target = Math.max(-pfdMaxPitch, Math.min(pfdMaxPitch, source));
    pfdPitch += (target - pfdPitch) * pfdPitchSmoothing;
}

// ---------------------------------------------------------------------------------
// Flight director. ADS-B carries neither pitch nor the aircraft's own FD command, so
// the guidance is derived from what it does report: the selected heading gives a
// commanded bank, proportional to the heading error and limited as an airliner's
// autopilot limits it, and the selected altitude a commanded vertical speed, which
// becomes a commanded flight path angle. The bars therefore show what the aircraft is
// being asked to do rather than a recorded command. The pitch bar is the angle still to
// fly, which is where it belongs on a ladder that shows the current flight path angle

const fdMaxBank = 25;               // Degrees, as an airliner's autopilot
const fdMaxVerticalSpeed = 1800;    // ft/min commanded towards the selected altitude
const fdBankGain = 1.3;             // Commanded bank per degree of heading error
const fdVerticalGain = 3;           // Commanded ft/min per foot of altitude error
const glideslopeAngle = 3;          // Degrees of descent on a standard ILS glidepath

// The course an ILS is flown on, taken from the arrival runway: a runway designator is
// its magnetic heading rounded to ten degrees, so 18R is 180. The flight plan belongs to
// the ND, which is loaded after this file, hence the guards. undefined when no plan names
// a runway, which is the common case for traffic that filed nothing
function pfdApproachCourse() {
    const plan = (typeof ndFlightPlan !== "undefined") ? ndFlightPlan : undefined;
    if ((plan === undefined) || (plan === null)) {
        return undefined;
    }
    var runway = plan.rwy;
    if ((runway === undefined) && (typeof ndApproachRunway === "function")) {
        runway = ndApproachRunway(plan.app);
    }
    if (runway === undefined) {
        return undefined;
    }
    const designator = parseInt(String(runway).substring(0, 2), 10);
    if (isNaN(designator) || (designator < 1) || (designator > 36)) {
        return undefined;
    }
    return designator * 10;
}
// Pixels per degree on the pitch ladder, which the flight director shares - its bar
// is placed on the same scale
const fdBankScale = 4;              // Pixels per degree of bank still to be flown
const ftPerMinutePerKnot = 101.269;

// The commanded bank and the pitch change still to fly, in degrees. undefined when
// nothing is guiding the aircraft, or when it is on the ground
function fdCommands() {
    if ((roll === undefined) || (onSurface > 0)) {
        return undefined;
    }
    if (!autopilot && (verticalMode === undefined) && (lateralMode === undefined)) {
        return undefined;
    }
    const cmd = {};
    // On the localiser the selected heading is often left at the last vector the aircraft
    // was given rather than the course it is now tracking, so steering to it turns the
    // aircraft off the approach. The centreline is what is being flown, and the runway
    // gives its course. As with the glidepath, beam deviation is not reported, so this
    // asks for the course itself rather than for a correction back onto the centreline -
    // and it falls back to the selected heading when no flight plan names a runway
    const approachCourse = (lateralMode === 2) ? pfdApproachCourse() : undefined;
    const commandedHeading = (approachCourse !== undefined) ? approachCourse : targetHeading;
    if ((commandedHeading !== undefined) && (heading !== undefined)) {
        var error = commandedHeading - heading;
        while (error > 180) error -= 360;
        while (error < -180) error += 360;
        cmd.bank = Math.max(-fdMaxBank, Math.min(fdMaxBank, error * fdBankGain));
    }
    // During a resolution advisory TCAS is flying the vertical path, as the FMA says,
    // and the VSI carries its guidance - so there is no pitch command to show
    const pathSpeed = pfdPathSpeed();
    if (!(tcasRA & 1) && (pathSpeed !== undefined))
    {
        if (verticalMode === 3)
        {
            // On the glideslope the selected altitude is the missed approach altitude -
            // it is set for the go-around, not as somewhere to fly to now. Capturing it
            // commands a climb the whole way down the approach, which saturated the bar
            // at its limit. What is being flown is the glidepath, and an ILS one is 3
            // degrees by definition, so the bar is referenced to that instead. Beam
            // deviation is not reported, so this asks for the nominal angle rather than
            // for a correction back onto the beam: on a correct glidepath the bar sits
            // on the aircraft symbol, and it deflects when the descent is too shallow
            // or too steep
            cmd.pitch = -glideslopeAngle - flightPathAngle();
        }
        else if ((targetAltitude !== undefined) && (altitude !== undefined))
        {
            const error = targetAltitude - altitude;
            const commandedVs = Math.max(-fdMaxVerticalSpeed, Math.min(fdMaxVerticalSpeed, error * fdVerticalGain));
            const rate = pathSpeed * ftPerMinutePerKnot;
            const commandedAngle = Math.atan2(commandedVs, rate) * 180 / Math.PI;
            cmd.pitch = commandedAngle - flightPathAngle();
        }
    }
    return ((cmd.bank === undefined) && (cmd.pitch === undefined)) ? undefined : cmd;
}

function drawFlightDirector(ctx, cx, cy, radius) {
    const cmd = fdCommands();
    if (cmd === undefined) {
        return;
    }
    const limit = radius * 0.45;
    const clamp = function (v) { return Math.max(-limit, Math.min(limit, v)); };
    // Up is a climb command, so the pitch bar moves opposite to the offset's sign
    const dy = (cmd.pitch === undefined) ? 0 : clamp(-cmd.pitch * pfdPitchScale);

    ctx.save();
    ctx.beginPath();
    ctx.arc(cx, cy, radius, 0, 2 * Math.PI);
    ctx.clip();
    ctx.strokeStyle = pfdTheme.fdColor;
    ctx.lineWidth = 5;

    // Cross bars, each showing one axis, neither rotating with the horizon - green on
    // an Airbus, magenta on a Boeing
    if (cmd.pitch !== undefined) {
        ctx.beginPath();
        ctx.moveTo(cx - radius * 0.5, cy + dy);
        ctx.lineTo(cx + radius * 0.5, cy + dy);
        ctx.stroke();
    }
    if (cmd.bank !== undefined) {
        const dx = clamp((cmd.bank - roll) * fdBankScale);
        ctx.beginPath();
        ctx.moveTo(cx + dx, cy - radius * 0.5);
        ctx.lineTo(cx + dx, cy + radius * 0.5);
        ctx.stroke();
    }
    ctx.restore();
}

// A rounded rectangle path, built from arcTo rather than the newer roundRect so it does
// not depend on how new the embedded browser is
function pfdRoundRectPath(ctx, x, y, w, h, r) {
    ctx.moveTo(x + r, y);
    ctx.arcTo(x + w, y, x + w, y + h, r);
    ctx.arcTo(x + w, y + h, x, y + h, r);
    ctx.arcTo(x, y + h, x, y, r);
    ctx.arcTo(x, y, x + w, y, r);
    ctx.closePath();
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

    // Clip. An Airbus attitude indicator is a circle with the sides flattened off; a
    // Boeing one is close to a square with rounded corners
    ctx.save();
    ctx.beginPath();
    if (pfdTheme.adi === "rounded") {
        pfdRoundRectPath(ctx, attitudeIndLeft, attitudeIndTop, attitudeIndWidth, attitudeIndHeight, 70);
    } else {
        ctx.arc(attitudeIndMidX, attitudeIndMidY, attitudeIndWidth / 2, Math.PI / 10 + Math.PI, Math.PI - Math.PI / 10 + Math.PI);
        ctx.arc(attitudeIndMidX, attitudeIndMidY, attitudeIndWidth / 2, Math.PI / 10, Math.PI - Math.PI / 10);
    }
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

    ctx.fillStyle = pfdTheme.sky;
    ctx.fillRect(attitudeIndLeft, attitudeIndTop, attitudeIndWidth, attitudeIndWidth);

    const rollRad = -roll * Math.PI / 180;
    const radius = attitudeIndWidth / 2;

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

    // Bank, then pitch: the ladder slides down the display by the angle it is showing,
    // so the aircraft symbol fixed at the centre reads that angle straight off it
    ctx.translate(attitudeIndMidX, attitudeIndMidY);
    ctx.rotate(rollRad);
    ctx.translate(0, pfdPitch * pfdPitchScale);

    // Everything below the horizon. A rectangle rather than the half disc this used to
    // be, so it still covers the corners of a square attitude indicator however far the
    // horizon has moved from the centre - the clip trims it
    ctx.fillStyle = pfdTheme.ground;
    ctx.fillRect(-radius * 3, 0, radius * 6, radius * 6);

    // The pitch ladder, linear at pfdPitchScale pixels per degree: a bar every 2.5
    // degrees, wider at 5 and wider still at 10, which is where the numbers go. It runs
    // well beyond the attitude indicator, or a climb would slide the end of it into view
    const ladderSteps = Math.round((pfdMaxPitch + 25) / 2.5);
    ctx.beginPath();
    ctx.moveTo(-radius, 0);
    ctx.lineTo(radius, 0);
    for (var k = -ladderSteps; k <= ladderSteps; k++) {
        if (k === 0) {
            continue;   // The horizon itself, drawn full width above
        }
        const y = -k * 2.5 * pfdPitchScale;
        const half = ((k % 4) === 0) ? tenDegOffset
            : (((k % 2) === 0) ? fiveDegOffset : twoAndHalfDegOffset);
        ctx.moveTo(-half, y);
        ctx.lineTo(half, y);
    }
    ctx.lineWidth = 2;
    ctx.strokeStyle = 'white';
    ctx.stroke();

    // The green brackets either side of 15 degrees nose down
    if (pfdTheme.pitchBrackets) {
        const bracketWidth = 20;
        const bracketHalf = twoAndHalfDegOffset * 2.5;
        const bracketY = 15 * pfdPitchScale;
        ctx.beginPath();
        ctx.strokeStyle = greenColor;
        for (const side of [-1, 1]) {
            for (const dy of [-3, 3]) {
                ctx.moveTo(side * (bracketHalf + bracketWidth), bracketY + dy);
                ctx.lineTo(side * bracketHalf, bracketY + dy);
            }
        }
        ctx.stroke();
    }

    ctx.font = "20px Arial";
    ctx.fillStyle = 'white';
    const labelCenterY = ctx.measureText("20").actualBoundingBoxAscent / 2;
    const labelOffset = tenDegOffset + 10;
    for (var k = -ladderSteps; k <= ladderSteps; k++) {
        if ((k === 0) || ((k % 4) !== 0)) {
            continue;   // Numbers go on the ten degree bars
        }
        const text = Math.abs(k * 2.5).toString();
        const y = -k * 2.5 * pfdPitchScale + labelCenterY;
        ctx.fillText(text, labelOffset, y);
        ctx.fillText(text, -labelOffset - ctx.measureText(text).width, y);
    }

    const rollBig = 18;
    const rollSmall = 10;

    const rollTriangleOffet = 4;

    ctx.beginPath();
    ctx.moveTo(0, -radius + rollTriangleOffet);
    ctx.lineTo(rollBig, -radius + rollBig + rollTriangleOffet);
    ctx.lineTo(-rollBig, -radius + rollBig + rollTriangleOffet);
    ctx.lineTo(0, -radius + rollTriangleOffet);
    ctx.lineWidth = 2;
    ctx.strokeStyle = pfdTheme.symbolColor;
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
    ctx.strokeStyle = pfdTheme.symbolColor;
    ctx.stroke();

    ctx.restore();

    // Roll scale

    ctx.save();
    ctx.translate(attitudeIndMidX, attitudeIndMidY);

    ctx.beginPath();
    ctx.rect(-radius, -radius - 2  *rollBig , radius * 2, radius);
    ctx.clip();

    // Airbus marks zero bank with a fixed index above the scale. Boeing has none: bank
    // is read off the moving pointer within the sky, which also leaves the space above
    // the attitude indicator to the FLT DIR annunciation
    if (pfdTheme.rollPointer) {
        ctx.beginPath();
        ctx.moveTo(0, -radius);
        ctx.lineTo(rollBig, -radius - rollBig);
        ctx.lineTo(-rollBig, -radius - rollBig);
        ctx.closePath();
        ctx.lineWidth = 2;
        ctx.strokeStyle = pfdTheme.symbolColor;
        ctx.stroke();
    }

    if (pfdTheme.bankArc) {
        ctx.beginPath();
        ctx.arc(0, 0, radius, (240 - 2) * Math.PI / 180, (300 + 2) * Math.PI / 180);
        ctx.lineWidth = 2;
        ctx.strokeStyle = 'white';
        ctx.stroke();
    }

    // 10, 20 and 30 degrees are boxed on an Airbus and plain ticks on a Boeing; 45 is a
    // tick on both
    const bankMarker = pfdTheme.bankBoxes ? drawBankAngleBoxMarker : drawBankAngleLineMarker;
    ctx.rotate(45 * Math.PI / 180);
    drawBankAngleLineMarker(ctx, radius, rollBig);
    ctx.rotate(-15 * Math.PI / 180);
    bankMarker(ctx, radius, rollBig);
    ctx.rotate(-10 * Math.PI / 180);
    bankMarker(ctx, radius, 10);
    ctx.rotate(-10 * Math.PI / 180);
    bankMarker(ctx, radius, 10);
    ctx.rotate(-10 * Math.PI / 180);

    ctx.rotate(-10 * Math.PI / 180);
    bankMarker(ctx, radius, 10);
    ctx.rotate(-10 * Math.PI / 180);
    bankMarker(ctx, radius, 10);
    ctx.rotate(-10 * Math.PI / 180);
    bankMarker(ctx, radius, rollBig);
    ctx.rotate(-15 * Math.PI / 180);
    drawBankAngleLineMarker(ctx, radius, rollBig);


   // ctx.rotate(-rollRad);
   // ctx.translate(-attitudeIndMidX, -attitudeIndMidY);

    ctx.restore();

    // Flight director, under the aircraft symbol that is flown into it
    drawFlightDirector(ctx, attitudeIndMidX, attitudeIndMidY, radius);
    if ((pfdTheme.fdLabel !== undefined) && (fdCommands() !== undefined)) {
        ctx.font = "30px Arial";
        ctx.fillStyle = greenColor;
        const fdMetrics = ctx.measureText(pfdTheme.fdLabel);
        ctx.fillText(pfdTheme.fdLabel, attitudeIndMidX - fdMetrics.width / 2, attitudeIndTop - 12);
    }

    // Wings: a bar either side with a leg dropped at its inner end, which both makers
    // draw - the proportions and the colour come from the theme
    const wingThickness = pfdTheme.wingThickness;
    const wingHeight = pfdTheme.wingHeight;
    const wingWidth = 90;
    const wingOffset = 130;

    ctx.lineWidth = 4;
    ctx.fillStyle = 'black';
    ctx.strokeStyle = pfdTheme.symbolColor;
    ctx.beginPath();
    ctx.moveTo(attitudeIndMidX - wingOffset - wingWidth, attitudeIndMidY - wingThickness / 2);
    ctx.lineTo(attitudeIndMidX - wingOffset, attitudeIndMidY - wingThickness / 2);
    ctx.lineTo(attitudeIndMidX - wingOffset, attitudeIndMidY + wingHeight);
    ctx.lineTo(attitudeIndMidX - wingOffset - wingThickness, attitudeIndMidY + wingHeight);
    ctx.lineTo(attitudeIndMidX - wingOffset - wingThickness, attitudeIndMidY + wingThickness / 2);
    ctx.lineTo(attitudeIndMidX - wingOffset - wingWidth, attitudeIndMidY + wingThickness / 2);
    ctx.lineTo(attitudeIndMidX - wingOffset - wingWidth, attitudeIndMidY - wingThickness / 2);
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
    ctx.stroke();
    ctx.fill();

    ctx.strokeRect(attitudeIndMidX - wingThickness / 2, attitudeIndMidY - wingThickness / 2, wingThickness, wingThickness);

    // Radio altitude
    if ((radioAltitude !== undefined) && (radioAltitude <= 2500)) {
        ctx.font = "30px Arial";
        if (radioAltitude < 100) {
            ctx.fillStyle = pfdTheme.raltLowColor;
        } else {
            ctx.fillStyle = pfdTheme.raltColor;
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
        const pressureMidY = pfdTheme.baroY;

        const pressureInt = Math.round(pressure);

        if (isStd()) {

            const border = 3;
            ctx.font = "30px Arial";
            ctx.fillStyle = pfdTheme.baroColor;
            const pressureText = "STD";
            const pressureMetrics = ctx.measureText(pressureText);
            const height = pressureMetrics.actualBoundingBoxAscent + pressureMetrics.actualBoundingBoxDescent + 2 * border;
            ctx.fillText(pressureText, pressureMidX - pressureMetrics.width / 2, pressureMidY);

            if (pfdTheme.baroBox) {
                ctx.lineWidth = 2;
                ctx.strokeStyle = pfdTheme.symbolColor;
                ctx.strokeRect(pressureMidX - pressureMetrics.width / 2 - border, pressureMidY - pressureMetrics.actualBoundingBoxAscent - 2 * border, pressureMetrics.width + border * 2, height + border * 2);
            }

        } else if (pfdTheme.name === "boeing") {
            // Boeing: the setting and its unit, in white
            ctx.font = "30px Arial";
            ctx.fillStyle = "white";
            ctx.fillText(pressureInt.toString() + " HPA", pressureLeft, pressureMidY);
        } else {
            ctx.font = "30px Arial";
            ctx.fillStyle = "white";
            ctx.fillText("QNH", pressureLeft, pressureMidY);

            ctx.fillStyle = pfdTheme.baroColor;
            const pressureText = pressureInt.toString();
            ctx.fillText(pressureText, pressureMidX, pressureMidY);
        }
    }
}

function drawMachIndicator(ctx) {
    if (mach !== undefined) {
        if (mach >= 0.45) { // Actually comes on at .5 and goes off at 0.45

            ctx.font = "38px Arial";
            ctx.fillStyle = pfdTheme.machColor;
            var machString;
            if (mach < 1) {
                machString = mach.toFixed(3).substring(1); // No leading 0
            } else {
                machString = mach.toFixed(2);
            }

            ctx.fillText(machString, 40, pfdTheme.machY);
        }
    }
}
// The VSI scale is piecewise: 0-1000 ft/min covers 4 marker spacings, 1000-2000 the
// next 2, 2000-6000 the last 2, clamped beyond. Signed offset in pixels above (+) or
// below (-) the centre for a signed vertical speed
function vsToOffset(vs, spacing) {
    const a = Math.abs(vs);
    var offset;
    if (a <= 1000) {
        offset = a / 1000 * 4 * spacing;
    } else if (a < 2000) {
        offset = (a - 1000) / 1000 * 2 * spacing + 4 * spacing;
    } else if (a < 6000) {
        offset = (a - 2000) / 4000 * 2 * spacing + 6 * spacing;
    } else {
        offset = 8 * spacing;
    }
    return offset * Math.sign(vs);
}

function drawVerticalSpeedIndicator(ctx, fm20, fm30) {

    const verticalSpeedIndWidth = 50;
    const verticalSpeedIndHeight = pfdTheme.vsiHeight;
    const verticalSpeedIndLeft = 900;
    const verticalSpeedIndTop = pfdTheme.vsiTop;
    const verticalSpeedIndBottom = verticalSpeedIndTop + verticalSpeedIndHeight;
    const verticalSpeedIndMidY = verticalSpeedIndTop + + verticalSpeedIndHeight / 2;
    const verticalSpeedIndMidX = verticalSpeedIndLeft + verticalSpeedIndWidth / 2;
    const verticalSpeedIndRight = verticalSpeedIndLeft + verticalSpeedIndWidth;
    const verticalSpeedBevel = 80;
    const verticalSpeedMarkerSpacing = pfdTheme.vsiSpacing;
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
    ctx.fillStyle = pfdTheme.tape;
    ctx.fill();
    if (pfdTheme.vsi === "boeing") {
        // Boeing outlines the scale as well as filling it
        ctx.strokeStyle = "white";
        ctx.lineWidth = 2;
        ctx.stroke();
    }

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
    ctx.fillStyle = pfdTheme.symbolColor;
    ctx.fillRect(verticalSpeedIndLeft, verticalSpeedIndMidY - 3, verticalSpeedIndWidth / 2, 6);

    // TCAS RA guidance bands over the marker strip: red across the prohibited vertical
    // speeds, with a green band beside it across the vertical speed to fly to avoid the
    // traffic. The ARA does not carry a vertical speed limit's magnitude, so a limit is
    // shown from level flight: the whole side towards the threat is red, and the green
    // band asks for a gentle rate away from it
    if ((tcasRA & 1) && !(tcasRA & 64))
    {
        const raBand = function (vsFrom, vsTo, color) {
            const y1 = verticalSpeedIndMidY - vsToOffset(vsFrom, verticalSpeedMarkerSpacing);
            const y2 = verticalSpeedIndMidY - vsToOffset(vsTo, verticalSpeedMarkerSpacing);
            ctx.fillStyle = color;
            ctx.fillRect(verticalSpeedMarkerLeft, Math.min(y1, y2), verticalSpeedMarkerWidth, Math.abs(y2 - y1));
        };
        const raDown = (tcasRA & 4) !== 0;
        const sense = raDown ? -1 : 1;
        if ((tcasRA & 2) && (tcasRA & 8))
        {
            // Corrective, positive: fly 1500-2000 ft/min in the advised sense
            raBand(-6000 * sense, 1500 * sense, redColor);
            raBand(1500 * sense, 2000 * sense, greenColor);
        }
        else
        {
            // Vertical speed limit (corrective "adjust vertical speed" or preventive
            // "monitor vertical speed"): do not fly towards the threat, hold 0-500
            // ft/min away from it
            raBand(0, -6000 * sense, redColor);
            raBand(0, 500 * sense, greenColor);
        }
    }

    // Indicator line

    const verticalSpeedAbsolute = Math.abs(verticalSpeed);
    var color = (verticalSpeedAbsolute >= 6000) ? orangeColor : pfdTheme.vsiColor;
    const verticalOffset = vsToOffset(verticalSpeedAbsolute, verticalSpeedMarkerSpacing);

    const verticalSpeedLineY = verticalSpeedIndMidY - verticalOffset * Math.sign(verticalSpeed);

    ctx.beginPath();
    ctx.moveTo(verticalSpeedLineRight, verticalSpeedIndMidY);
    ctx.lineTo(verticalSpeedLineLeft, verticalSpeedLineY);
    ctx.lineWidth = 4;
    ctx.strokeStyle = color;
    ctx.stroke();

    // Boeing shows the rate in figures above the scale once it is worth reading
    if (pfdTheme.vsi === "boeing")
    {
        if (verticalSpeedAbsolute >= 400) {
            ctx.font = "30px Arial";
            ctx.fillStyle = color;
            const vsText = (Math.round(verticalSpeedAbsolute / 50) * 50).toString();
            const vsMetrics = ctx.measureText(vsText);
            ctx.fillText(vsText, verticalSpeedIndMidX - vsMetrics.width / 2, verticalSpeedIndTop - 10);
        }
    }
    // Text
    else if (verticalSpeedAbsolute > 200) {

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
// type: the ICAO aircraft type to show as, e.g. "B738" for the Boeing look; A320 by default
function animatePFD(type) {
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
    heading = heading + headingInc;
    altitude = altitude + altitudeInc;
    altitude = 39;
    radioAltitude = 1720;
    onSurface = 0;
    verticalSpeed = verticalSpeed + verticalSpeedInc;
    roll = roll + rollInc;
    reportedPitch = undefined;      // Let the flight path angle drive the ladder
    callsign = "BAW123G";
    aircraftType = (type === undefined) ? "A320" : type;
    pfdTheme = cockpitThemeFor(aircraftType);
    verticalMode = 3;
    lateralMode = 2;
    autopilot = 1;
    pressure = 1013;
    //pressure = 1018;
    // Cycle the TCAS RA through none / preventive / corrective climb / corrective
    // descend / clear of conflict, ten seconds each, exercising the FMA TCAS
    // annunciator, the VSI bands and (via the aural edge logic) each sound
    if (animateRaStart === undefined) {
        animateRaStart = Date.now();
    }
    const raStates = [0, 1, 1 | 2 | 8, 1 | 2 | 4 | 8, 0];
    const raPhase = Math.floor((Date.now() - animateRaStart) / 10000) % raStates.length;
    tcasRA = raStates[raPhase];
    if (tcasRA !== previousTcasRA) {
        const raActive = (tcasRA & 1) !== 0;
        if (raActive) {
            pfdPlayAudio((tcasRA & 2) ? ((tcasRA & 4) ? "descend.wav" : "climb.wav") : "traffic.wav");
        } else if (previousTcasRA & 1) {
            pfdPlayAudio("clear-of-conflict.wav");
        }
        previousTcasRA = tcasRA;
    }
}
var animateRaStart;
// Match the canvas backing store to the size it is displayed at, so text stays sharp
// however the window is resized - a fixed backing store scaled by CSS goes soft.
// Drawing code stays in the 1000x1000 logical space via the transform.
function pfdPrepareCanvas(pfd) {
    // clientWidth/Height rather than getBoundingClientRect(), so the border is not
    // included in the drawn area
    if ((pfd.clientWidth > 0) && (pfd.clientHeight > 0)) {
        const dpr = window.devicePixelRatio || 1;
        const w = Math.round(pfd.clientWidth * dpr);
        const h = Math.round(pfd.clientHeight * dpr);
        if ((pfd.width !== w) || (pfd.height !== h)) {
            pfd.width = w;
            pfd.height = h;
        }
    }
    const ctx = pfd.getContext("2d");
    ctx.setTransform(pfd.width / 1000, 0, 0, pfd.height / 1000, 0, 0);
    return ctx;
}

function drawPFD() {

    const canvas = document.getElementById("pfdCanvas");
    const ctx = pfdPrepareCanvas(canvas);

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

    // Flight mode annunciators. Airbus: A/THR | vertical | lateral | approach capability |
    // AP, FD, A/THR. Boeing: A/T | roll | pitch | AFDS status (LAND 3, CMD / FD). The
    // column each mode goes in comes from the theme, as does the vocabulary

    drawFMA(ctx);

    panelDrawClose(ctx);

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

    // TCAS mode, groundspeed, TAS, wind and SAT used to be drawn here, but they belong
    // to the ND (navdisplay.js), which is where they are now - and setPFDData no longer
    // takes them
}

// Per FMA slot, the label last shown there and when it appeared - a Boeing boxes a mode
// in green for ten seconds after it changes
var pfdFmaState = {};
var pfdFmaDrawn = new Set();

function pfdFmaText(ctx, key, text, x, y, color) {
    ctx.font = "30px Arial";
    ctx.fillStyle = color;
    ctx.fillText(text, x, y);
    pfdFmaDrawn.add(key);
    if (pfdTheme.fmaHighlight) {
        const now = Date.now();
        var s = pfdFmaState[key];
        if ((s === undefined) || (s.text !== text)) {
            s = { text: text, since: now };
            pfdFmaState[key] = s;
        }
        if (now - s.since < 10000) {
            const width = ctx.measureText(text).width;
            ctx.lineWidth = 2;
            ctx.strokeStyle = greenColor;
            ctx.strokeRect(x - 6, y - 30, width + 12, 40);
        }
    }
}

function drawFMA(ctx) {
    const th = pfdTheme;
    const L = th.labels;
    const boeing = (th.name === "boeing");
    pfdFmaDrawn.clear();

    const separator = function (x) {
        ctx.beginPath();
        ctx.moveTo(x, 5);
        ctx.lineTo(x, 140);
        ctx.lineWidth = 1;
        ctx.strokeStyle = "white";
        ctx.stroke();
    };

    // Autothrust / autothrottle column
    separator(220);
    // Nothing sets targetSpeed: ADS-B carries a selected altitude and heading but no
    // selected speed. The autopilot being on is the only signal there is, and both
    // makers' displays are driven from it
    if ((targetSpeed !== undefined) || (autopilot && !rollOut)) {
        pfdFmaText(ctx, "athr", L.speed, 55, 50, greenColor);
    } else if (toga) {
        pfdFmaText(ctx, "athr", L.toga, 70, 50, "white");
    }

    // Vertical (pitch) and lateral (roll) modes. On an Airbus LAND, FLARE and ROLL OUT
    // span both columns; on a Boeing FLARE is a pitch mode and ROLLOUT a roll mode
    var landing = false;
    var spanX;
    var pitchText;
    var pitchColor = greenColor;
    var rollText;
    if (th.fmaTcas && (tcasRA & 1)) {
        // An active resolution advisory takes the vertical mode column: TCAS is now
        // flying the vertical path. Red for a corrective RA, white for preventive.
        // Boeing does not annunciate TCAS there, so its pitch mode carries on
        pitchText = "TCAS";
        pitchColor = (tcasRA & 2) ? redColor : "white";
    } else if (verticalMode !== undefined) {
        if (verticalMode == 1) {
            if (verticalSpeed !== undefined) {
                pitchText = (verticalSpeed > 0) ? L.clb : ((verticalSpeed < 0) ? L.des : L.alt);
            } else if (targetAltitude !== undefined && altitude !== undefined) {
                pitchText = (targetAltitude > altitude) ? L.clb : ((targetAltitude < altitude) ? L.des : L.alt);
            }
        } else if (verticalMode == 2) {
            pitchText = L.alt;
        } else if (verticalMode == 3) {
            if (rollOut) {
                if (th.landingSpans) {
                    pitchText = L.rollout;
                    spanX = 370;
                    landing = true;
                } else {
                    rollText = L.rollout;
                    pitchText = L.flare;
                }
            } else if ((onSurface === 0) && (radioAltitude < 40)) {
                pitchText = L.flare;
                if (th.landingSpans) {
                    spanX = 390;
                    landing = true;
                }
            } else if ((onSurface === 0) && (radioAltitude < 400)) {
                pitchText = L.land;
                if (th.landingSpans) {
                    spanX = 400;
                    landing = true;
                }
            } else {
                pitchText = L.gs;
            }
        }
    }
    if (pitchText !== undefined) {
        pfdFmaText(ctx, "pitch", pitchText, landing ? spanX : th.verticalX, 50, pitchColor);
    }
    if (th.fmaTcas && !(tcasRA & 1) && !tcasInhibited
        && (typeof ndHasTA === "function") && ndHasTA()) {
        // A traffic advisory arms TCAS: it appears on the armed row beneath the active
        // vertical mode, which carries on. Not below the inhibit height, where TCAS
        // cannot resolve anything, so there is nothing for it to be armed for - the
        // advisories themselves are still tracked and still drawn on the ND
        pfdFmaText(ctx, "pitchArmed", "TCAS", th.verticalX, 85, th.armedColor);
    }
    if (!landing) {
        separator(420);
    }
    separator(660);

    if ((rollText === undefined) && (lateralMode !== undefined) && !landing) {
        if (lateralMode == 1) {
            rollText = L.nav;
        } else if (lateralMode == 2) {
            rollText = L.loc;
        }
    }
    if (rollText !== undefined) {
        pfdFmaText(ctx, "roll", rollText, th.lateralX, 50, greenColor);
    }

    // Autoflight status
    separator(850);
    if (boeing) {
        if (autopilot) {
            pfdFmaText(ctx, "afds", "CMD", 870, 40, greenColor);
            if (verticalMode == 3) {
                pfdFmaText(ctx, "land", "LAND 3", 690, 40, greenColor);
            }
        } else if (autopilot !== undefined) {
            pfdFmaText(ctx, "afds", "FD", 890, 40, "white");
        }
    } else {
        ctx.font = "30px Arial";
        ctx.fillStyle = "white";
        if (autopilot) {
            if (verticalMode == 3) {
                // We can't tell whether both APs are enabled - but they typically would be
                ctx.fillText("AP1+2", 856, 40);
                ctx.fillText("CAT3", 720, 40);
                ctx.fillText("DUAL", 720, 85);
            } else {
                ctx.fillText("AP1", 890, 40);
            }
        }
        // Which flight directors are on is not reported; drawn for an aircraft that
        // reports its autoflight state at all, rather than unconditionally
        if (autopilot !== undefined) {
            ctx.fillText("1 FD 2", 870, 85);
        }
        if (((targetSpeed !== undefined) || autopilot) && !rollOut) {
            ctx.fillText("A/THR", 870, 130);
        }
    }

    // Forget slots that are empty this frame, so a mode that returns is boxed again
    for (const key of Object.keys(pfdFmaState)) {
        if (!pfdFmaDrawn.has(key)) {
            delete pfdFmaState[key];
        }
    }
}


const canvas = document.getElementById('pfdCanvas');
canvas.onmousedown = pfdMouseDown;
canvas.onmouseup = pfdMouseUp;
canvas.onmousemove = pfdMouseMove;
canvas.onmouseleave = pfdMouseLeave;
canvas.onmouseenter = pfdMouseEnter;
// A drag that leaves the canvas carries on against the document, so the panel borrows
// its mouse handlers and gives them back when the drag ends. Whether they were borrowed
// has to be its own flag: nothing else in the page sets them, so what is saved is
// normally null, and testing that for truth never gave them back at all
var docMouseMove = null;
var docMouseUp = null;
var docMouseCaptured = false;
var movePFD = false;
var scalePFD = false;
var posX = 0;
// Where the cursor was when the drag started, so each move places the panel
// absolutely from it rather than accumulating deltas
var pfdDragX = 0;
var pfdDragY = 0;
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
    if (panelInClose(e.offsetX * 1000 / r.width, e.offsetY * 1000 / r.height)) {
        panelClose("pfd", canvas);
        return;
    }
    posX = r.left;
    posY = r.top;
    canvasWidth = r.width;
    canvasHeight = r.height;
    pfdDragX = e.clientX;
    pfdDragY = e.clientY;
    if ((e.offsetX > 0.95 * canvasWidth) && (e.offsetY > 0.95 * canvasHeight)) {
        scalePFD = true;
    } else {
        movePFD = true;
    }
}
function pfdMouseUp(e) {
    e.preventDefault();
    e.stopPropagation();
    if (movePFD || scalePFD) {
        panelSaveGeometry("pfd", canvas);
    }
    movePFD = false;
    scalePFD = false;
    pfdRestoreDocMouse();
}
function pfdMouseMove(e) {
    e.preventDefault();
    e.stopPropagation();
    if (movePFD) {
        canvas.style.position = "absolute";
        canvas.style.left = (posX + e.clientX - pfdDragX) + "px";
        canvas.style.top = (posY + e.clientY - pfdDragY) + "px";
    } else if (scalePFD) {
        const newWidth = canvasWidth + e.clientX - pfdDragX;
        const newHeight = canvasHeight + e.clientY - pfdDragY;
        const constrainedWidth = Math.max(newWidth, 250);
        const constrainedHeight = Math.max(newHeight, 250);
        canvas.style.width = constrainedWidth + "px";
        canvas.style.height = constrainedHeight + "px";
        drawPFD(); // Redrawn at the new size, so it stays sharp while resizing
    }
}

function pfdMouseLeave(e) {
    //movePFD = false;
    //scalePFD = false;
    if ((scalePFD || movePFD) && !docMouseCaptured) {
        docMouseUp = document.onmouseup;
        docMouseMove = document.onmousemove;
        docMouseCaptured = true;
        document.onmouseup = pfdMouseUp;
        document.onmousemove = pfdMouseMove;
    }
}

function pfdRestoreDocMouse() {
    if (docMouseCaptured) {
        document.onmouseup = docMouseUp;
        document.onmousemove = docMouseMove;
        docMouseUp = null;
        docMouseMove = null;
        docMouseCaptured = false;
    }
}
function pfdMouseEnter(e) {
    pfdRestoreDocMouse();
}
