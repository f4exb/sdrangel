/*
 * Aircraft engine sound configuration support for the 3D Map.
 *
 * engine.json is a compact representation of the useful exterior portions of
 * an FSX sound.cfg.  This file deliberately contains no Cesium or WebAudio
 * objects, so the curve and configuration handling stays easy to test.
 */
(function (global) {
    "use strict";

    const defaultFullScaleDistance = 50.0;
    const configCache = new Map();

    function finiteNumber(value, fallback) {
        return (typeof value === "number") && Number.isFinite(value) ? value : fallback;
    }

    function clamp(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, value));
    }

    function safePathComponent(value, extension) {
        return (typeof value === "string")
            && (value.length > 0)
            && (value !== ".")
            && (value !== "..")
            && !value.includes("..")
            && /^[A-Za-z0-9._-]+$/.test(value)
            && ((extension === undefined) || value.toLowerCase().endsWith(extension));
    }

    function normalizeCurve(value, description, minimumOutput, maximumOutput) {
        if (!Array.isArray(value) || (value.length === 0)) {
            throw new Error(description + " must contain at least one point");
        }

        const curve = value.map((point) => {
            if (!Array.isArray(point) || (point.length !== 2)
                || !Number.isFinite(point[0]) || !Number.isFinite(point[1])) {
                throw new Error(description + " contains an invalid point");
            }
            if ((point[0] < 0.0) || (point[0] > 1.0)
                || (point[1] < minimumOutput) || (point[1] > maximumOutput)) {
                throw new Error(description + " contains a point outside its allowed range");
            }
            return [point[0], point[1]];
        });
        curve.sort((a, b) => a[0] - b[0]);
        return curve;
    }

    function normalizeLayer(value, kind, index) {
        if ((value === null) || (typeof value !== "object")) {
            throw new Error(kind + " layer " + index + " is not an object");
        }
        if (!safePathComponent(value.filename, ".ogg")) {
            throw new Error(kind + " layer " + index + " has an unsafe or non-OGG filename");
        }

        const hasCompleteCone = Number.isFinite(value.conePitch)
            && Number.isFinite(value.coneHeading)
            && Number.isFinite(value.insideConeAngle)
            && Number.isFinite(value.outsideConeAngle)
            && Number.isFinite(value.outsideConeVolume);
        let insideConeAngle = hasCompleteCone ? clamp(value.insideConeAngle, 0.0, 360.0) : 360.0;
        let outsideConeAngle = hasCompleteCone ? clamp(value.outsideConeAngle, 0.0, 360.0) : 360.0;
        outsideConeAngle = Math.max(insideConeAngle, outsideConeAngle);

        return {
            id: (typeof value.id === "string") ? value.id : kind.toUpperCase() + "." + (index + 1),
            kind: kind,
            filename: value.filename,
            vparams: normalizeCurve(value.vparams, value.filename + " vparams", 0.0, 100.0),
            // Some FSX layers explicitly use a zero rate at zero spool. The matching
            // volume is silent there, and WebAudio accepts a zero playback rate.
            rparams: normalizeCurve(value.rparams, value.filename + " rparams", 0.0, 8.0),
            spatial: {
                conePitch: hasCompleteCone ? value.conePitch : 0.0,
                coneHeading: hasCompleteCone ? value.coneHeading : 0.0,
                insideConeAngle: insideConeAngle,
                outsideConeAngle: outsideConeAngle,
                outsideConeVolume: hasCompleteCone ? clamp(value.outsideConeVolume, 0.0, 10000.0) : 10000.0,
                fullScaleDistance: (Number.isFinite(value.fullScaleDistance) && (value.fullScaleDistance > 0.0))
                    ? value.fullScaleDistance : defaultFullScaleDistance
            }
        };
    }

    function normalizeConfiguration(value, name) {
        if ((value === null) || (typeof value !== "object")) {
            throw new Error(name + " engine.json is not an object");
        }
        if (value.formatVersion !== 1) {
            throw new Error(name + " engine.json has unsupported formatVersion " + value.formatVersion);
        }

        const n1 = Array.isArray(value.n1) ? value.n1.map((layer, index) => normalizeLayer(layer, "n1", index)) : [];
        const n2 = Array.isArray(value.n2) ? value.n2.map((layer, index) => normalizeLayer(layer, "n2", index)) : [];
        if ((n1.length + n2.length) === 0) {
            throw new Error(name + " engine.json contains no N1 or N2 layers");
        }

        return {
            formatVersion: 1,
            numberOfEngines: finiteNumber(value.numberOfEngines, 1),
            engine: finiteNumber(value.engine, 1),
            layers: n1.concat(n2)
        };
    }

    function configurationBaseURL(name) {
        if (!safePathComponent(name)) {
            throw new Error("Unsafe engine sound set name " + name);
        }
        return "/3d/Sounds/" + encodeURIComponent(name) + "/";
    }

    function loadConfiguration(name) {
        if (!configCache.has(name)) {
            let baseURL;
            try {
                baseURL = configurationBaseURL(name);
            } catch (e) {
                return Promise.reject(e);
            }
            const request = fetch(baseURL + "engine.json")
                .then((response) => {
                    if (!response.ok) {
                        throw new Error("HTTP " + response.status + " loading " + baseURL + "engine.json");
                    }
                    return response.json();
                })
                .then((value) => {
                    const configuration = normalizeConfiguration(value, name);
                    configuration.baseURL = baseURL;
                    return configuration;
                })
                .catch((e) => {
                    configCache.delete(name); // An install completed while the Map is open can be retried
                    throw e;
                });
            configCache.set(name, request);
        }
        return configCache.get(name);
    }

    function interpolateCurve(curve, input) {
        const x = clamp(finiteNumber(input, 0.0), 0.0, 1.0);
        if (x <= curve[0][0]) {
            return curve[0][1];
        }
        for (let i = 1; i < curve.length; i++) {
            if (x <= curve[i][0]) {
                const previous = curve[i - 1];
                const next = curve[i];
                if (next[0] === previous[0]) {
                    return next[1];
                }
                const fraction = (x - previous[0]) / (next[0] - previous[0]);
                return previous[1] + fraction * (next[1] - previous[1]);
            }
        }
        return curve[curve.length - 1][1];
    }

    // FSX volume curves are linear power percentages: 50 is approximately -3 dB.
    function volumePercentToGain(value) {
        return Math.sqrt(clamp(finiteNumber(value, 0.0), 0.0, 100.0) / 100.0);
    }

    // OutsideConeVolume is hundredths of a decibel below the 10000 full-volume value.
    function outsideConeVolumeToGain(value) {
        const volume = clamp(finiteNumber(value, 10000.0), 0.0, 10000.0);
        return volume <= 0.0 ? 0.0 : Math.pow(10.0, (volume - 10000.0) / 2000.0);
    }

    function estimateEngineState(property) {
        const onSurface = property("pfdOnSurface");
        const groundspeedValue = property("pfdGroundspeed");
        const groundspeed = Number.isFinite(groundspeedValue) ? Math.max(0.0, groundspeedValue) : 0.0;

        if (Number.isFinite(onSurface) && (onSurface > 0)) {
            if (groundspeed > 60.0) {
                return { n1: 0.98, n2: 0.98 }; // Take-off roll
            }
            if (groundspeed > 15.0) {
                const fraction = clamp((groundspeed - 15.0) / 45.0, 0.0, 1.0);
                return { n1: 0.28 + 0.32 * fraction, n2: 0.62 + 0.22 * fraction };
            }
            return { n1: 0.20, n2: 0.55 }; // Parked or taxi idle
        }

        const verticalSpeed = property("pfdVerticalSpeed");
        if (Number.isFinite(verticalSpeed)) {
            if (verticalSpeed > 500.0) {
                return { n1: 0.90, n2: 0.95 }; // Climb
            }
            if (verticalSpeed < -500.0) {
                return { n1: 0.34, n2: 0.68 }; // Descent or approach
            }
        }
        return { n1: 0.78, n2: 0.90 }; // Cruise, or the best airborne estimate
    }

    function smoothEngineState(current, target, elapsedSeconds) {
        if (current === undefined) {
            return { n1: target.n1, n2: target.n2 };
        }
        const alpha = 1.0 - Math.exp(-clamp(finiteNumber(elapsedSeconds, 0.25), 0.0, 2.0) / 2.5);
        current.n1 += (target.n1 - current.n1) * alpha;
        current.n2 += (target.n2 - current.n2) * alpha;
        return current;
    }

    function spatialProfileKey(spatial) {
        return [spatial.conePitch, spatial.coneHeading, spatial.insideConeAngle,
            spatial.outsideConeAngle, spatial.outsideConeVolume, spatial.fullScaleDistance].join("|");
    }

    global.AircraftAudio = Object.freeze({
        defaultFullScaleDistance: defaultFullScaleDistance,
        configurationBaseURL: configurationBaseURL,
        loadConfiguration: loadConfiguration,
        normalizeConfiguration: normalizeConfiguration,
        interpolateCurve: interpolateCurve,
        volumePercentToGain: volumePercentToGain,
        outsideConeVolumeToGain: outsideConeVolumeToGain,
        estimateEngineState: estimateEngineState,
        smoothEngineState: smoothEngineState,
        spatialProfileKey: spatialProfileKey
    });
})(window);
