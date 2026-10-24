// RMP - a simplified radio management panel in the style of the A350's DRAIMS: two VHF
// COM channels, each with an ACTIVE and a STANDBY frequency, a keypad to enter a standby
// frequency and a swap key per channel that makes it active. Each VHF channel is linked
// to an SDRangel AM Demod, chosen in the Map's display settings, which is retuned when
// the active frequency changes. Also carries the transponder code and TCAS traffic
// selector, which on the real aircraft live on the ATC panel next to it.
// Drawn in a 1000x400 logical space. Uses the EFIS panel's knob and button primitives
// and the cockpit colours, so navdisplay.js and cockpit.js must be loaded first.

// The band the keypad accepts, in Hz. The airband, with 8.33 kHz channels named on
// 5 kHz boundaries. Widen the lower limit to 108 MHz for NAV audio
const rmpMinFrequency = 118000000;
const rmpMaxFrequency = 136975000;
const rmpFrequencyStep = 5000;
const rmpWheelStep = 25000;       // Per wheel tick over a standby display

const rmpTcasLabels = ["THRT", "ALL", "ABV", "BLW"];
const rmpTcasKnob = { cx: 470, cy: 335, r: 42, titleY: 240 };
const rmpSquawkRect = { x: 90, y: 275, w: 220, h: 75 };

// Per VHF channel: what the linked AM Demod is tuned to (null when it cannot be read),
// the standby frequency, whether an AM Demod is linked, the digits being typed for a new
// standby (null when none) and when a rejected entry's amber flash ends
const rmpVhf = [
    { active: null, standby: 121500000, available: false, entry: null, flashUntil: 0 },
    { active: null, standby: 121500000, available: false, entry: null, flashUntil: 0 }
];
var rmpSelected = 0;   // Which channel's standby the keypad edits

// Layout. The two VHF rows fill the left two thirds, the keypad the right third
const rmpRows = [
    { y: 40, label: "VHF1" },
    { y: 135, label: "VHF2" }
];
const rmpActiveX = 90;
const rmpStandbyX = 400;
const rmpDisplayW = 220;
const rmpDisplayH = 75;
const rmpSwapX = 325;
function rmpActiveRect(i) { return { x: rmpActiveX, y: rmpRows[i].y, w: rmpDisplayW, h: rmpDisplayH }; }
function rmpStandbyRect(i) { return { x: rmpStandbyX, y: rmpRows[i].y, w: rmpDisplayW, h: rmpDisplayH }; }
function rmpSwapRect(i) { return { x: rmpSwapX, y: rmpRows[i].y + 7, w: 60, h: 60 }; }

const rmpKeys = [];
(function () {
    const labels = ["1", "2", "3", "4", "5", "6", "7", "8", "9", "CLR", "0", "."];
    for (let i = 0; i < labels.length; i++) {
        rmpKeys.push({
            label: labels[i],
            x: 670 + (i % 3) * 107,
            y: 40 + Math.floor(i / 3) * 85,
            w: 95,
            h: 70
        });
    }
})();

function rmpInRect(x, y, r) {
    return (x >= r.x) && (x < r.x + r.w) && (y >= r.y) && (y < r.y + r.h);
}

function rmpFormat(hz) {
    if ((hz === null) || (hz === undefined)) {
        return "---.---";
    }
    return (hz / 1e6).toFixed(3);
}

// The keypad entry as Hz, or null when it is not a usable channel frequency
function rmpParseEntry(entry) {
    if ((entry === null) || !/^\d{3}(\.\d{1,3})?$/.test(entry)) {
        return null;
    }
    const hz = Math.round(parseFloat(entry) * 1e6);
    if ((hz < rmpMinFrequency) || (hz > rmpMaxFrequency) || ((hz % rmpFrequencyStep) !== 0)) {
        return null;
    }
    return hz;
}

function rmpSend(obj) {
    // No socket in a test harness
    if ((typeof socket !== "undefined") && socket) {
        socket.send(JSON.stringify(obj));
    }
}

function rmpSendStandby(i) {
    rmpSend({ event: "rmpStandby", vhf: i + 1, frequency: rmpVhf[i].standby });
}

function rmpPrepareCanvas() {
    if ((rmpCanvas.clientWidth > 0) && (rmpCanvas.clientHeight > 0)) {
        const dpr = window.devicePixelRatio || 1;
        const w = Math.round(rmpCanvas.clientWidth * dpr);
        const h = Math.round(rmpCanvas.clientHeight * dpr);
        if ((rmpCanvas.width !== w) || (rmpCanvas.height !== h)) {
            rmpCanvas.width = w;
            rmpCanvas.height = h;
        }
    }
    const ctx = rmpCanvas.getContext("2d");
    ctx.setTransform(rmpCanvas.width / 1000, 0, 0, rmpCanvas.height / 400, 0, 0);
    return ctx;
}

// An LCD style inset frequency display. Amber while a rejected entry is flashed, a
// cyan frame on the standby the keypad is editing
function rmpDrawDisplay(ctx, rect, text, frameColor, textColor) {
    ctx.fillStyle = "#101014";
    ctx.fillRect(rect.x, rect.y, rect.w, rect.h);
    ctx.strokeStyle = frameColor;
    ctx.lineWidth = 3;
    ctx.strokeRect(rect.x, rect.y, rect.w, rect.h);
    ctx.font = "52px monospace";
    ctx.textAlign = "center";
    ctx.fillStyle = textColor;
    ctx.fillText(text, rect.x + rect.w / 2, rect.y + rect.h / 2 + 19);
}

function drawRMP() {
    if (rmpCanvas === null) {
        return;
    }
    const ctx = rmpPrepareCanvas();
    const now = Date.now();

    ctx.fillStyle = "#33333C";
    ctx.fillRect(0, 0, 1000, 400);
    ctx.strokeStyle = "#55555F";
    ctx.lineWidth = 4;
    ctx.strokeRect(2, 2, 996, 396);

    ctx.font = "20px Arial";
    ctx.fillStyle = "white";
    ctx.textAlign = "center";
    ctx.fillText("ACTIVE", rmpActiveX + rmpDisplayW / 2, 30);
    ctx.fillText("STBY", rmpStandbyX + rmpDisplayW / 2, 30);

    for (let i = 0; i < 2; i++) {
        const v = rmpVhf[i];
        const row = rmpRows[i];
        ctx.font = "24px Arial";
        ctx.fillStyle = "white";
        ctx.textAlign = "left";
        ctx.fillText(row.label, 12, row.y + rmpDisplayH / 2 + 9);

        rmpDrawDisplay(ctx, rmpActiveRect(i), v.available ? rmpFormat(v.active) : "---.---", "#55555F", greenColor);

        const flashing = now < v.flashUntil;
        const text = (v.entry !== null) ? v.entry : rmpFormat(v.standby);
        rmpDrawDisplay(ctx, rmpStandbyRect(i), text,
            (i === rmpSelected) ? cyanColor : "#55555F",
            flashing ? orangeColor : greenColor);

        efisDrawButton(ctx, rmpSwapRect(i), "\u21C4", false);   // Left-right arrows
    }

    for (const key of rmpKeys) {
        efisDrawButton(ctx, key, key.label, false);
    }

    // The transponder code
    ctx.font = "20px Arial";
    ctx.textAlign = "center";
    ctx.fillStyle = "white";
    ctx.fillText("XPDR", rmpSquawkRect.x + rmpSquawkRect.w / 2, 262);
    const code = (ndSquawk !== undefined) && (ndSquawk >= 0)
        ? ndSquawk.toString().padStart(4, "0") : "----";
    rmpDrawDisplay(ctx, rmpSquawkRect, code, "#55555F", greenColor);

    efisDrawKnob(ctx, rmpTcasKnob, "TCAS", rmpTcasLabels, ndTcasFilterIdx);

    panelDrawClose(ctx);

    // The TCAS selector is saved with the EFIS settings, as it was once on that panel
    efisCheckState();
}

function rmpStepTcas(step) {
    ndTcasFilterIdx = Math.min(Math.max(ndTcasFilterIdx + step, 0), rmpTcasLabels.length - 1);
    drawND();
    drawRMP();
}

function rmpKey(label) {
    const v = rmpVhf[rmpSelected];
    if (label === "CLR") {
        if (v.entry !== null) {
            v.entry = v.entry.slice(0, -1);
            if (v.entry.length === 0) {
                v.entry = null;
            }
        }
    } else if (label === ".") {
        if ((v.entry !== null) && (v.entry.length === 3)) {
            v.entry += ".";
        }
    } else {
        const entry = (v.entry === null) ? "" : v.entry;
        const dot = entry.indexOf(".");
        if (dot < 0) {
            if (entry.length < 3) {
                v.entry = entry + label;
                if (v.entry.length === 3) {
                    v.entry += ".";   // The point is always there, so save a keystroke
                }
            }
        } else if (entry.length - dot - 1 < 3) {
            v.entry = entry + label;
        }
    }
    drawRMP();
}

// The swap key: a typed entry becomes the standby first, then standby and active are
// exchanged and the linked AM Demod is retuned to the new active frequency
function rmpSwap(i) {
    const v = rmpVhf[i];
    if (v.entry !== null) {
        const hz = rmpParseEntry(v.entry);
        if (hz === null) {
            v.flashUntil = Date.now() + 700;
            drawRMP();
            return;
        }
        v.standby = hz;
        v.entry = null;
    }
    if (v.available) {
        const newActive = v.standby;
        if (v.active !== null) {
            v.standby = v.active;
        }
        v.active = newActive;   // Confirmed, or corrected, by the next setRMPState
        rmpSend({ event: "rmpTune", vhf: i + 1, frequency: newActive });
    }
    rmpSendStandby(i);
    drawRMP();
}

// From the GUI: what the linked AM Demods are actually tuned to. A standby being
// typed over is left alone
function rmpSetState(state) {
    for (let i = 0; i < 2; i++) {
        const s = state["vhf" + (i + 1)];
        if (s === undefined) {
            continue;
        }
        const v = rmpVhf[i];
        v.available = (s.available === true);
        v.active = ((s.active === undefined) || (s.active === null)) ? null : s.active;
        if ((v.entry === null) && (s.standby !== undefined)) {
            v.standby = s.standby;
        }
    }
    drawRMP();
}

var rmpMove = false;
var rmpScale = false;
var rmpPosX = 0;
var rmpPosY = 0;
// Where the cursor was when the drag started, so each move places the panel
// absolutely from it rather than accumulating deltas
var rmpDragX = 0;
var rmpDragY = 0;
var rmpWidth = 0;
var rmpHeight = 0;
var rmpDocMouseMove = null;
var rmpDocMouseUp = null;
var rmpDocMouseCaptured = false;

function rmpMouseDown(e) {
    e.preventDefault();
    e.stopPropagation();
    const r = rmpCanvas.getBoundingClientRect();
    const x = e.offsetX * 1000 / r.width;
    const y = e.offsetY * 400 / r.height;
    if (panelInClose(x, y)) {
        panelClose("rmp", rmpCanvas);
        return;
    }
    for (const key of rmpKeys) {
        if (rmpInRect(x, y, key)) {
            rmpKey(key.label);
            return;
        }
    }
    for (let i = 0; i < 2; i++) {
        if (rmpInRect(x, y, rmpSwapRect(i))) {
            rmpSwap(i);
            return;
        }
        if (rmpInRect(x, y, rmpStandbyRect(i))) {
            rmpSelected = i;
            drawRMP();
            return;
        }
    }
    if (efisInKnob(x, y, rmpTcasKnob)) {
        rmpStepTcas((x < rmpTcasKnob.cx) ? -1 : 1);
        return;
    }
    rmpPosX = r.left;
    rmpPosY = r.top;
    rmpWidth = r.width;
    rmpHeight = r.height;
    rmpDragX = e.clientX;
    rmpDragY = e.clientY;
    if ((e.offsetX > rmpWidth - 25) && (e.offsetY > rmpHeight - 25)) {
        rmpScale = true;
    } else {
        rmpMove = true;
    }
}

function rmpMouseUp(e) {
    e.preventDefault();
    e.stopPropagation();
    if (rmpMove || rmpScale) {
        panelSaveGeometry("rmp", rmpCanvas);
    }
    rmpMove = false;
    rmpScale = false;
    rmpRestoreDocMouse();
}

function rmpMouseMove(e) {
    e.preventDefault();
    e.stopPropagation();
    if (rmpMove) {
        rmpCanvas.style.position = "absolute";
        rmpCanvas.style.left = (rmpPosX + e.clientX - rmpDragX) + "px";
        rmpCanvas.style.top = (rmpPosY + e.clientY - rmpDragY) + "px";
    } else if (rmpScale) {
        const newWidth = rmpWidth + e.clientX - rmpDragX;
        const newHeight = rmpHeight + e.clientY - rmpDragY;
        rmpCanvas.style.width = Math.max(newWidth, 250) + "px";
        rmpCanvas.style.height = Math.max(newHeight, 100) + "px";
        drawRMP();
    }
}

function rmpMouseLeave(e) {
    if ((rmpScale || rmpMove) && !rmpDocMouseCaptured) {
        rmpDocMouseUp = document.onmouseup;
        rmpDocMouseMove = document.onmousemove;
        rmpDocMouseCaptured = true;
        document.onmouseup = rmpMouseUp;
        document.onmousemove = rmpMouseMove;
    }
}

function rmpRestoreDocMouse() {
    if (rmpDocMouseCaptured) {
        document.onmouseup = rmpDocMouseUp;
        document.onmousemove = rmpDocMouseMove;
        rmpDocMouseUp = null;
        rmpDocMouseMove = null;
        rmpDocMouseCaptured = false;
    }
}

function rmpMouseEnter(e) {
    rmpRestoreDocMouse();
}

function rmpWheel(e) {
    e.preventDefault();
    e.stopPropagation();
    const r = rmpCanvas.getBoundingClientRect();
    const x = e.offsetX * 1000 / r.width;
    const y = e.offsetY * 400 / r.height;
    // Wheel up turns the selector clockwise, as on the EFIS panel
    if (efisInKnob(x, y, rmpTcasKnob)) {
        rmpStepTcas((e.deltaY < 0) ? 1 : -1);
        return;
    }
    // Over a standby display, the wheel steps it a channel at a time, dropping any
    // entry being typed
    for (let i = 0; i < 2; i++) {
        if (rmpInRect(x, y, rmpStandbyRect(i))) {
            const v = rmpVhf[i];
            v.entry = null;
            const step = (e.deltaY < 0) ? rmpWheelStep : -rmpWheelStep;
            v.standby = Math.min(Math.max(v.standby + step, rmpMinFrequency), rmpMaxFrequency);
            rmpSelected = i;
            rmpSendStandby(i);
            drawRMP();
            return;
        }
    }
}

// Null in a page (such as a test harness) without the panel
const rmpCanvas = document.getElementById('rmpCanvas');
if (rmpCanvas !== null) {
    rmpCanvas.onmousedown = rmpMouseDown;
    rmpCanvas.onmouseup = rmpMouseUp;
    rmpCanvas.onmousemove = rmpMouseMove;
    rmpCanvas.onmouseleave = rmpMouseLeave;
    rmpCanvas.onmouseenter = rmpMouseEnter;
    rmpCanvas.onwheel = rmpWheel;
}
