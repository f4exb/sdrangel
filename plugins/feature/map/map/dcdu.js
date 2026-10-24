// Airbus like DCDU (Datalink Control and Display Unit), for CPDLC/PDC messages
// Must be loaded after cockpit.js, as it shares its color constants
// All globals are prefixed dcdu, to avoid clashing in the page's shared scope

var dcduMessage;        // Parsed dcduMessage property: {id, rx, up, stn, txt}
var dcduHeader = "";
var dcduLines = [];     // Message text, word wrapped
var dcduPage = 0;

const dcduLinesPerPage = 9;
const dcduTextTop = 130;
const dcduLineHeight = 56;
// On-canvas controls, in the 1000x800 canvas coordinate space
const dcduPgRect = { x: 600, y: 680, w: 220, h: 100 };
const dcduCloseRect = { x: 830, y: 680, w: 170, h: 100 };

function dcduPages() {
    return Math.max(1, Math.ceil(dcduLines.length / dcduLinesPerPage));
}

function dcduWrapText(ctx, text, maxWidth) {
    var lines = [];
    for (const paragraph of text.split("\n")) {
        var line = "";
        for (const word of paragraph.split(/\s+/)) {
            const candidate = (line.length > 0) ? line + " " + word : word;
            if ((ctx.measureText(candidate).width <= maxWidth) || (line.length === 0)) {
                line = candidate;
            } else {
                lines.push(line);
                line = word;
            }
        }
        lines.push(line);
    }
    return lines;
}

function setDCDUMessage(message) {
    dcduMessage = message;

    // Header - e.g. "1412Z FROM EDDS"
    var header = "";
    if ((message.rx !== undefined) && (message.rx.length >= 16)) {
        header = message.rx.substring(11, 13) + message.rx.substring(14, 16) + "Z";
    }
    header += (message.up === 1) ? " FROM" : " TO";
    if (message.stn !== undefined) {
        header += " " + message.stn;
    }
    dcduHeader = header.trim();

    const ctx = dcduCanvas.getContext("2d");
    ctx.font = "40px Courier New";
    dcduLines = dcduWrapText(ctx, message.txt !== undefined ? message.txt : "", 940);
    dcduPage = 0;
    drawDCDU();
}

// Match the canvas backing store to the size it is displayed at, so text stays sharp
// however the window is resized - a fixed backing store scaled by CSS goes soft.
// Drawing code stays in the 1000x800 logical space via the transform.
function dcduPrepareCanvas() {
    // clientWidth/Height rather than getBoundingClientRect(), so the border is not
    // included in the drawn area
    if ((dcduCanvas.clientWidth > 0) && (dcduCanvas.clientHeight > 0)) {
        const dpr = window.devicePixelRatio || 1;
        const w = Math.round(dcduCanvas.clientWidth * dpr);
        const h = Math.round(dcduCanvas.clientHeight * dpr);
        if ((dcduCanvas.width !== w) || (dcduCanvas.height !== h)) {
            dcduCanvas.width = w;
            dcduCanvas.height = h;
        }
    }
    const ctx = dcduCanvas.getContext("2d");
    ctx.setTransform(dcduCanvas.width / 1000, 0, 0, dcduCanvas.height / 800, 0, 0);
    return ctx;
}

function drawDCDU() {
    const ctx = dcduPrepareCanvas();

    ctx.fillStyle = "#0c0c0c";
    ctx.fillRect(0, 0, 1000, 800);

    // Header
    ctx.font = "40px Courier New";
    ctx.textAlign = "left";
    ctx.fillStyle = "white";
    ctx.fillText(dcduHeader, 30, 60);

    // Message text
    ctx.fillStyle = greenColor;
    const first = dcduPage * dcduLinesPerPage;
    for (var i = 0; (i < dcduLinesPerPage) && (first + i < dcduLines.length); i++) {
        ctx.fillText(dcduLines[first + i], 30, dcduTextTop + i * dcduLineHeight);
    }

    // Divider above the bottom row
    ctx.strokeStyle = grayColor;
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.moveTo(0, 665);
    ctx.lineTo(1000, 665);
    ctx.stroke();

    // Page indicator, click to flip
    if (dcduPages() > 1) {
        ctx.textAlign = "center";
        ctx.fillStyle = "white";
        ctx.fillText("PG", dcduPgRect.x + dcduPgRect.w / 2, 725);
        ctx.fillStyle = cyanColor;
        ctx.fillText((dcduPage + 1) + " " + dcduPages(), dcduPgRect.x + dcduPgRect.w / 2, 775);
    }

    // Close button
    ctx.textAlign = "right";
    ctx.fillStyle = cyanColor;
    ctx.fillText("CLOSE*", dcduCloseRect.x + dcduCloseRect.w - 10, 755);

    panelDrawClose(ctx);
}

// The panel-visibility menu on the Map toolbar can veto the DCDU entirely
var dcduEnabled = true;

function showDCDUMessage(message, playSound) {
    if (!dcduEnabled) {
        return;
    }
    // Unhidden first: a hidden canvas has no size, and the backing store is matched
    // to the displayed size when it is drawn
    dcduCanvas.removeAttribute("hidden");
    setDCDUMessage(message);
    // The ring's volume - and whether it is heard at all - is the cockpit volume
    // slider in Map Display Settings
    if (playSound) {
        dcduRing();
    }
}

// Ring for an incoming message. The audio files are served from the qrc audio
// directory, a sibling of the directory map3d.html is in
function dcduRing() {
    try {
        const audio = new Audio("../audio/ATC.wav");
        // The cockpit volume slider in Map Display Settings - map3d.html holds the value
        audio.volume = (typeof cockpitSoundVolume !== "undefined") ? cockpitSoundVolume : 1.0;
        // play() returns a promise, which rejects rather than throws on failure
        audio.play().catch((e) => {
            console.log("DCDU ring failed: " + e);
        });
    } catch (e) {
        console.log("DCDU ring failed: " + e);
    }
}

// Self-test with fake data, callable from the DevTools console while developing
function animateDCDU() {
    showDCDUMessage({
        id: 1,
        rx: "2026-09-01T14:12:03Z",
        up: 1,
        stn: "EDDS",
        txt: "STATION PDC 001 . . . . .\nCLD 2042 220117 EDDS PDC 001 FBW291 CLRD TO EDDM OFF 25 VIA ABTAL4B "
            + "CLIMB 5000 SQUAWK 1000 ADT MDI NEXT FREQ 121.905 ATIS K QNH 1021 "
            + "TRANS LEVEL FL70 EXPECT FL230 TEN MIN AFTER DEP CONTACT LANGEN RADAR 118.800"
    }, true);
}

const dcduCanvas = document.getElementById('dcduCanvas');
dcduCanvas.onmousedown = dcduMouseDown;
dcduCanvas.onmouseup = dcduMouseUp;
dcduCanvas.onmousemove = dcduMouseMove;
dcduCanvas.onmouseleave = dcduMouseLeave;
dcduCanvas.onmouseenter = dcduMouseEnter;
var dcduDocMouseMove = null;
var dcduDocMouseUp = null;
var dcduDocMouseCaptured = false;
var dcduMove = false;
var dcduScale = false;
var dcduPosX = 0;
// Where the cursor was when the drag started, so each move places the panel
// absolutely from it rather than accumulating deltas
var dcduDragX = 0;
var dcduDragY = 0;
var dcduPosY = 0;
var dcduCanvasWidth = 0;
var dcduCanvasHeight = 0;

function dcduInRect(x, y, rect) {
    return (x >= rect.x) && (x <= rect.x + rect.w) && (y >= rect.y) && (y <= rect.y + rect.h);
}

function dcduMouseDown(e) {
    e.preventDefault();
    e.stopPropagation();
    const r = dcduCanvas.getBoundingClientRect();
    // Canvas is drawn in a 1000x800 space, then CSS scaled
    const x = e.offsetX * 1000 / r.width;
    const y = e.offsetY * 800 / r.height;
    // The corner button closes the panel for good; CLOSE* only dismisses the message
    if (panelInClose(x, y)) {
        panelClose("dcdu", dcduCanvas);
        return;
    }
    if (dcduInRect(x, y, dcduCloseRect)) {
        dcduCanvas.setAttribute("hidden", "hidden");
        return;
    }
    if ((dcduPages() > 1) && dcduInRect(x, y, dcduPgRect)) {
        dcduPage = (dcduPage + 1) % dcduPages();
        drawDCDU();
        return;
    }
    dcduPosX = r.left;
    dcduPosY = r.top;
    dcduCanvasWidth = r.width;
    dcduCanvasHeight = r.height;
    dcduDragX = e.clientX;
    dcduDragY = e.clientY;
    if ((e.offsetX > 0.95 * dcduCanvasWidth) && (e.offsetY > 0.95 * dcduCanvasHeight)) {
        dcduScale = true;
    } else {
        dcduMove = true;
    }
}
function dcduMouseUp(e) {
    e.preventDefault();
    e.stopPropagation();
    if (dcduMove || dcduScale) {
        panelSaveGeometry("dcdu", dcduCanvas);
    }
    dcduMove = false;
    dcduScale = false;
    dcduRestoreDocMouse();
}
function dcduMouseMove(e) {
    e.preventDefault();
    e.stopPropagation();
    if (dcduMove) {
        dcduCanvas.style.position = "absolute";
        dcduCanvas.style.left = (dcduPosX + e.clientX - dcduDragX) + "px";
        dcduCanvas.style.top = (dcduPosY + e.clientY - dcduDragY) + "px";
    } else if (dcduScale) {
        const newWidth = dcduCanvasWidth + e.clientX - dcduDragX;
        const newHeight = dcduCanvasHeight + e.clientY - dcduDragY;
        const constrainedWidth = Math.max(newWidth, 250);
        const constrainedHeight = Math.max(newHeight, 200);
        dcduCanvas.style.width = constrainedWidth + "px";
        dcduCanvas.style.height = constrainedHeight + "px";
        drawDCDU(); // Redrawn at the new size, so it stays sharp while resizing
    }
}

function dcduMouseLeave(e) {
    if ((dcduScale || dcduMove) && !dcduDocMouseCaptured) {
        dcduDocMouseUp = document.onmouseup;
        dcduDocMouseMove = document.onmousemove;
        dcduDocMouseCaptured = true;
        document.onmouseup = dcduMouseUp;
        document.onmousemove = dcduMouseMove;
    }
}

function dcduRestoreDocMouse() {
    if (dcduDocMouseCaptured) {
        document.onmouseup = dcduDocMouseUp;
        document.onmousemove = dcduDocMouseMove;
        dcduDocMouseUp = null;
        dcduDocMouseMove = null;
        dcduDocMouseCaptured = false;
    }
}
function dcduMouseEnter(e) {
    dcduRestoreDocMouse();
}
