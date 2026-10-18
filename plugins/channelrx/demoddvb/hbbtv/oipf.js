// The objects an HbbTV application (TS 102 796, with the OIPF DAE objects it uses) expects of the TV it runs on, for
// applications run by SDRangel's DVB demodulator. Injected into each page before its own scripts run, after
// window.__sdrangelHbbTVConfig is set to the TV's state: the service played and the others of the multiplex,
// the application's ids, and the languages and country. Messages go to SDRangel as console messages (\u0001sdrangel:
// and JSON), and SDRangel calls window.__sdrangelHbbTV's functions: key() for keys in the application's keyset,
// update() when the service or the list changes, channelChanged() after a setChannel()
(function () {
    'use strict';

    if (window.__sdrangelHbbTV) {
        return;
    }

    var config = window.__sdrangelHbbTVConfig || {};
    // The console's own log, for messages to SDRangel and its log: an application may replace it (as BBC's do, with one
    // that does nothing), which would cut SDRangel off from the page. Taken before the application's scripts run
    var consoleLog = console.log.bind(console);

    // A page whose root (html) has an opaque background when it's loaded is drawn opaque from then on (Qt WebEngine
    // keeps that colour behind it): the video under it, though the application later makes the root transparent (or
    // it's cleared over the video), stays hidden. Its root's held transparent until a while after it's loaded, and its
    // body's (which is the root's when the root has none). A constructed sheet, as there's no element yet to put a
    // style in
    if ((window.top === window) && document.adoptedStyleSheets && window.CSSStyleSheet) {
        try {
            var rootHeld = new CSSStyleSheet();
            rootHeld.replaceSync('html, body { background-color: transparent !important; }');
            document.adoptedStyleSheets = document.adoptedStyleSheets.concat([rootHeld]);
            window.addEventListener('load', function () {
                setTimeout(function () {
                    document.adoptedStyleSheets = document.adoptedStyleSheets.filter(function (s) { return s !== rootHeld; });
                }, 500);
            });
        } catch (e) {
            consoleLog('[sdrangel] root not held transparent: ' + e);
        }
    }

    // For SDRangel's log, as applications' own errors seldom say what failed: requests that fail (4xx, 5xx), with what
    // the server said, and objects logged as errors written out (else "[object Object]")
    function describeValue(value) {
        if (!value || (typeof value !== 'object')) {
            return String(value);
        }

        var parts = [];

        if (value instanceof Error) {
            parts.push(value.name + ': ' + value.message);
        }

        // As an axios error's: its code, the response's status, and the request
        ['code', 'status'].forEach(function (key) {
            if ((value[key] !== undefined) && (typeof value[key] !== 'object')) {
                parts.push(key + ' ' + value[key]);
            }
        });

        if (value.response && (value.response.status !== undefined)) {
            parts.push('response ' + value.response.status);
        }

        if (value.config && value.config.url) {
            parts.push((value.config.method || '').toUpperCase() + ' ' + value.config.url);
        }

        if (!parts.length) {
            try {
                parts.push(JSON.stringify(value).slice(0, 300));
            } catch (e) {
                parts.push(Object.prototype.toString.call(value));
            }
        }

        return parts.join(', ');
    }

    ['error', 'warn'].forEach(function (level) {
        var original = console[level];

        console[level] = function () {
            // As it was if it can't be described (as an object whose properties throw, or another origin's window):
            // the application's own logging mustn't fail
            var args = Array.prototype.map.call(arguments, function (a) {
                if (!a || (typeof a !== 'object')) {
                    return a;
                }

                try {
                    return describeValue(a);
                } catch (e) {
                    return a;
                }
            });
            return original.apply(console, args);
        };
    });

    function logFailedRequest(request, status, body) {
        consoleLog('[sdrangel] request failed: ' + request + ': ' + status + (body ? ': ' + String(body).replace(/\s+/g, ' ').slice(0, 300) : ''));
    }

    var loggedXhrOpen = XMLHttpRequest.prototype.open;
    var loggedXhrSend = XMLHttpRequest.prototype.send;

    XMLHttpRequest.prototype.open = function (method, url) {
        this.__sdrangelRequest = String(method).toUpperCase() + ' ' + String(url);
        return loggedXhrOpen.apply(this, arguments);
    };

    XMLHttpRequest.prototype.send = function () {
        var xhr = this;

        xhr.addEventListener('loadend', function () {
            if (xhr.status >= 400) {
                var body = '';

                try {
                    body = ((xhr.responseType === '') || (xhr.responseType === 'text')) ? xhr.responseText
                        : (xhr.responseType === 'json') ? JSON.stringify(xhr.response) : '';
                } catch (e) {
                    body = '';
                }

                logFailedRequest(xhr.__sdrangelRequest, xhr.status, body);
            }
        });

        return loggedXhrSend.apply(this, arguments);
    };

    if (window.fetch) {
        var loggedFetch = window.fetch;

        window.fetch = function (input, init) {
            var request = ((init && init.method) || (input && input.method) || 'GET').toUpperCase() + ' ' + (input && input.url ? input.url : String(input));

            return loggedFetch.apply(this, arguments).then(function (response) {
                if (response.status >= 400) {
                    response.clone().text().then(function (body) { logFailedRequest(request, response.status, body); }, function () {
                        logFailedRequest(request, response.status, '');
                    });
                }

                return response;
            });
        };
    }

    var keysetValue = 0;
    var ownerApplication = null;
    var videoObjects = [];          // video/broadcast objects bound to the service, or with a channel set
    var state = new WeakMap();      // Each object's state
    var lastVideo = '';

    function send(message) {
        consoleLog('\u0001sdrangel:' + JSON.stringify(message));
    }

    // The application closing its window (as BBC iPlayer exits, on devices it takes for older ones): on a TV that ends
    // it, as destroyApplication() does. A browser won't close a window a script didn't open
    if (window.top === window) {
        window.close = function () {
            consoleLog('[sdrangel] window.close(): the application destroyed');
            send({ t: 'destroy' });
        };
    }

    // Keys (OIPF DAE 7.2.5.2, TS 102 796 Annex A)
    var keys = {
        VK_LEFT: 37, VK_UP: 38, VK_RIGHT: 39, VK_DOWN: 40, VK_ENTER: 13, VK_BACK: 461,
        VK_RED: 403, VK_GREEN: 404, VK_YELLOW: 405, VK_BLUE: 406,
        VK_0: 48, VK_1: 49, VK_2: 50, VK_3: 51, VK_4: 52, VK_5: 53, VK_6: 54, VK_7: 55, VK_8: 56, VK_9: 57,
        VK_PLAY: 415, VK_PAUSE: 19, VK_PLAY_PAUSE: 402, VK_STOP: 413, VK_FAST_FWD: 417, VK_REWIND: 412,
        VK_TRACK_NEXT: 425, VK_TRACK_PREV: 424, VK_RECORD: 416, VK_PAGE_UP: 33, VK_PAGE_DOWN: 34,
        VK_INFO: 457, VK_TELETEXT: 459, VK_SUBTITLE: 460
    };

    window.KeyEvent = window.KeyEvent || {};

    Object.keys(keys).forEach(function (name) {
        window.KeyEvent[name] = keys[name];
        window[name] = keys[name];
    });

    var keyNames = {
        37: 'ArrowLeft', 38: 'ArrowUp', 39: 'ArrowRight', 40: 'ArrowDown', 13: 'Enter', 461: 'GoBack',
        403: 'ColorF0Red', 404: 'ColorF1Green', 405: 'ColorF2Yellow', 406: 'ColorF3Blue',
        415: 'MediaPlay', 19: 'MediaPause', 402: 'MediaPlayPause', 413: 'MediaStop', 417: 'MediaFastForward',
        412: 'MediaRewind', 425: 'MediaTrackNext', 424: 'MediaTrackPrevious', 416: 'MediaRecord',
        33: 'PageUp', 34: 'PageDown', 457: 'Info', 459: 'Teletext', 460: 'Subtitle'
    };

    // The physical key, as code gives it: applications that map keys by their own tables fall back to it (as the BBC's)
    var codeNames = {
        37: 'ArrowLeft', 38: 'ArrowUp', 39: 'ArrowRight', 40: 'ArrowDown', 13: 'Enter', 461: 'BrowserBack',
        403: 'ColorF0Red', 404: 'ColorF1Green', 405: 'ColorF2Yellow', 406: 'ColorF3Blue',
        415: 'MediaPlay', 19: 'MediaPause', 402: 'MediaPlayPause', 413: 'MediaStop', 417: 'MediaFastForward',
        412: 'MediaRewind', 425: 'MediaTrackNext', 424: 'MediaTrackPrevious', 416: 'MediaRecord',
        33: 'PageUp', 34: 'PageDown', 457: 'Info', 460: 'Subtitle'
    };

    function keyEvent(type, code) {
        var key = keyNames[code] || ((code >= 48) && (code <= 57) ? String.fromCharCode(code) : 'Unidentified');
        var physical = codeNames[code] || ((code >= 48) && (code <= 57) ? 'Digit' + String.fromCharCode(code) : '');
        var event = new KeyboardEvent(type, { bubbles: true, cancelable: true, key: key, code: physical });
        // Applications read keyCode (or which), which a KeyboardEvent's dictionary doesn't set
        Object.defineProperty(event, 'keyCode', { get: function () { return code; } });
        Object.defineProperty(event, 'which', { get: function () { return code; } });
        Object.defineProperty(event, 'charCode', { get: function () { return type === 'keypress' ? code : 0; } });
        var target = document.activeElement || document.body || document;
        target.dispatchEvent(event);
    }

    // The keyset (OIPF DAE 7.2.5)
    var keyset = {
        RED: 0x1, GREEN: 0x2, YELLOW: 0x4, BLUE: 0x8, NAVIGATION: 0x10, VCR: 0x20, SCROLL: 0x40, INFO: 0x80,
        NUMERIC: 0x100, ALPHA: 0x200, OTHER: 0x400,
        maximumValue: 0x1FF,
        otherKeys: [],
        maximumOtherKeys: [],
        supportsPointer: false,
        setValue: function (value, otherKeys) {
            keysetValue = (value | 0) & 0x7FF;
            send({ t: 'keyset', v: keysetValue });
            return keysetValue;
        },
        getKey: function (code) { return null; },
        getKeyLabel: function (code) { return null; }
    };

    Object.defineProperty(keyset, 'value', { get: function () { return keysetValue; } });

    // Channels: of the configuration, as objects of OIPF DAE 7.13.11
    var ID_DVB_C = 10, ID_DVB_S = 11, ID_DVB_T = 12, ID_DVB_S2 = 15, ID_DVB_T2 = 16;

    function makeChannel(c) {
        if (!c) {
            return null;
        }

        return {
            channelType: c.radio ? 1 : 0,       // TYPE_TV 0, TYPE_RADIO 1
            idType: c.idType || ID_DVB_T,
            ccid: 'ccid:' + c.onid + '.' + c.tsid + '.' + c.sid,
            onid: c.onid,
            tsid: c.tsid,
            sid: c.sid,
            nid: c.onid,
            name: c.name || '',
            majorChannel: c.lcn > 0 ? c.lcn : 0,
            minorChannel: 0,
            dsd: null,
            favourite: false,
            favIDs: [],
            locked: false,
            manualBlock: false,
            hidden: false,
            ipBroadcastID: null,
            longName: c.name || '',
            description: '',
            authorised: true,
            genre: [],
            isHD: !!c.hd,
            is3D: false,
            logoURL: null
        };
    }

    function channelList() {
        var list = (config.channels || []).map(makeChannel);
        list.item = function (index) { return list[index] || null; };
        list.getChannel = function (ccid) {
            for (var i = 0; i < list.length; i++) {
                if (list[i].ccid === ccid) {
                    return list[i];
                }
            }
            return null;
        };
        list.getChannelByTriplet = function (onid, tsid, sid) {
            for (var i = 0; i < list.length; i++) {
                if ((list[i].onid === onid) && ((tsid === undefined) || (tsid === null) || (tsid < 0) || (list[i].tsid === tsid)) && (list[i].sid === sid)) {
                    return list[i];
                }
            }
            return null;
        };
        list.getChannelBySourceID = function () { return null; };
        return list;
    }

    function programmes() {
        var list = (config.programmes || []).map(function (p) {
            return {
                name: p.name || '', description: p.description || '', longDescription: p.description || '',
                startTime: p.start, duration: p.duration, programmeID: p.id !== undefined ? String(p.id) : '',
                programmeIDType: 0, channelID: config.channel ? makeChannel(config.channel).ccid : '',
                parentalRatings: [], getSIDescriptors: function () { return []; }
            };
        });
        list.item = function (index) { return list[index] || null; };
        return list;
    }

    // The owner application (OIPF DAE 7.2.2)
    function application() {
        if (ownerApplication) {
            return ownerApplication;
        }

        var privateData = { keyset: keyset, getFreeMem: function () { return 64 * 1024 * 1024; } };
        Object.defineProperty(privateData, 'currentChannel', { get: function () { return makeChannel(config.channel); } });

        ownerApplication = {
            visible: true,
            active: true,
            permissions: [],
            isPrimaryReceiver: true,
            privateData: privateData,
            show: function () { this.visible = true; send({ t: 'show' }); return true; },
            hide: function () { this.visible = false; send({ t: 'hide' }); return true; },
            activate: function () { this.active = true; },
            deactivate: function () { this.active = false; },
            destroyApplication: function () { send({ t: 'destroy' }); },
            createApplication: function (uri, createChild) {
                send({ t: 'create', uri: String(uri), child: !!createChild });
                return { visible: false, privateData: {}, show: function () {}, hide: function () {}, destroyApplication: function () {} };
            },
            startWidget: function () { return null; },
            stopWidget: function () {},
            addEventListener: function () {},
            removeEventListener: function () {}
        };
        return ownerApplication;
    }

    var configuration = {
        preferredAudioLanguage: config.language || 'eng',
        preferredSubtitleLanguage: config.language || 'eng',
        preferredUILanguage: config.language || 'eng',
        countryId: config.country || 'GBR',
        regionId: 0,
        pvrPolicy: 0,
        pvrSaveEpisodes: 0,
        pvrSaveDays: 0,
        pvrStartPadding: 0,
        pvrEndPadding: 0,
        preferredTimeShiftMode: 0,
        subtitlesEnabled: false,
        audioDescriptionEnabled: false,
        getText: function () { return null; },
        setText: function () {}
    };

    var localSystem = {
        deviceID: 'sdrangel',
        systemReady: true,
        vendorName: 'SDRangel',
        modelName: 'DVBDemod',
        familyName: 'SDRangel',
        softwareVersion: config.version || '',
        hardwareVersion: '',
        serialNumber: '',
        releaseVersion: 0,
        majorVersion: 1,
        minorVersion: 0,
        oipfProfile: 'OITF_HD_UIPROF+DVB_T+DVB_T2+DVB_S+DVB_S2',
        pvrEnabled: false,
        ciplusEnabled: false,
        powerState: 1,
        previousPowerState: 1,
        timeCurrentPowerState: 0,
        volume: 100,
        mute: false,
        tvStandardsSupported: 0,
        tvStandard: 0,
        setPowerState: function () { return false; },
        setPvrSupport: function () { return 0; }
    };

    function capabilitiesXml() {
        var xml = '<profilelist><ui_profile name="OITF_HD_UIPROF+DVB_T+DVB_T2+DVB_S+DVB_S2"><ext><colorkeys>true</colorkeys>' +
            '<video_broadcast type="ID_DVB_T" scaling="arbitrary" minSize="0">true</video_broadcast></ext></ui_profile>' +
            '<broadcast>urn:dvb:broadcast:ird:video:25_Hz_H.264_AVC_HDTV_IRD</broadcast></profilelist>';
        return new DOMParser().parseFromString(xml, 'text/xml');
    }

    // video/broadcast (OIPF DAE 7.13.1)
    var PLAYSTATE_UNREALIZED = 0, PLAYSTATE_CONNECTING = 1, PLAYSTATE_PRESENTING = 2, PLAYSTATE_STOPPED = 3;

    function videoState(object) {
        var s = state.get(object);

        if (!s) {
            s = { playState: PLAYSTATE_UNREALIZED, fullScreen: false, pendingChannel: null };
            state.set(object, s);
        }

        return s;
    }

    function fire(object, name, properties, handlerArguments) {
        var handler = object['on' + name];

        if (typeof handler === 'function') {
            try {
                handler.apply(object, handlerArguments);
            } catch (e) {
                console.error(e);
            }
        }

        var event = new CustomEvent(name, { bubbles: false });

        Object.keys(properties).forEach(function (key) {
            Object.defineProperty(event, key, { value: properties[key] });
        });

        object.dispatchEvent(event);
    }

    function setPlayState(object, playState, error) {
        var s = videoState(object);

        if (s.playState === playState) {
            return;
        }

        s.playState = playState;

        if ((playState === PLAYSTATE_PRESENTING) || (playState === PLAYSTATE_CONNECTING)) {
            if (videoObjects.indexOf(object) < 0) {
                videoObjects.push(object);
            }
        } else {
            videoObjects = videoObjects.filter(function (o) { return o !== object; });
        }

        fire(object, 'PlayStateChange', { state: playState, error: error }, [playState, error]);
        reportVideo();
    }

    var videoBroadcast = {
        bindToCurrentChannel: function () {
            setPlayState(this, PLAYSTATE_PRESENTING);
            return makeChannel(config.channel);
        },
        setChannel: function (channel, trickplay, contentAccessDescriptorURL, quiet) {
            if (!channel) {
                setPlayState(this, PLAYSTATE_UNREALIZED);
                return;
            }

            var current = config.channel;

            if (current && (channel.sid === current.sid) && (channel.onid === current.onid)) {
                setPlayState(this, PLAYSTATE_PRESENTING);
                var self = this;
                setTimeout(function () { fire(self, 'ChannelChangeSucceeded', { channel: makeChannel(current) }, [makeChannel(current)]); }, 0);
                return;
            }

            videoState(this).pendingChannel = channel;
            setPlayState(this, PLAYSTATE_CONNECTING);
            send({ t: 'setChannel', onid: channel.onid, tsid: channel.tsid, sid: channel.sid });
        },
        prevChannel: function () { send({ t: 'step', d: -1 }); },
        nextChannel: function () { send({ t: 'step', d: 1 }); },
        stop: function () { setPlayState(this, PLAYSTATE_STOPPED); },
        release: function () { setPlayState(this, PLAYSTATE_UNREALIZED); },
        setFullScreen: function (fullScreen) {
            videoState(this).fullScreen = !!fullScreen;
            fire(this, 'FullScreenChange', {}, []);
            reportVideo();
        },
        getChannelConfig: function () {
            return { channelList: channelList(), favouriteLists: [], currentFavouriteList: null, onChannelScan: null };
        },
        getComponents: function (type) {
            // VIDEO 0, AUDIO 1, SUBTITLE 2
            if ((type === 1) || (type === undefined) || (type === null)) {
                var audio = [{ type: 1, componentTag: 0, pid: 0, encoding: 'MPEG1_L2', language: config.language || 'eng', audioDescription: false, audioChannels: 2 }];
                return type === 1 ? audio : [{ type: 0, componentTag: 0, pid: 0, encoding: 'H264', aspectRatio: 1.78 }].concat(audio);
            }
            return type === 0 ? [{ type: 0, componentTag: 0, pid: 0, encoding: 'H264', aspectRatio: 1.78 }] : [];
        },
        getCurrentActiveComponents: function (type) { return this.getComponents(type); },
        selectComponent: function () {},
        unselectComponent: function () {},
        setVolume: function () { return true; },
        getVolume: function () { return 100; },
        createChannelObject: function (idType, onid, tsid, sid) {
            return makeChannel({ idType: idType, onid: onid, tsid: tsid, sid: sid });
        }
    };

    var nextDrmMessage = 1;

    var applicationManager = {
        getOwnerApplication: function (doc) { return application(); },
        getChildApplications: function () { return []; },
        gc: function () {},
        onLowMemory: null,
        onApplicationLoaded: null,
        onApplicationUnloaded: null,
        onApplicationLoadError: null
    };

    var objectTypes = {
        'application/oipfapplicationmanager': applicationManager,
        'application/oipfconfiguration': { },
        'application/oipfcapabilities': {
            hasCapability: function (capability) { return false; },
            extraSDVideoDecodes: 0,
            extraHDVideoDecodes: 0
        },
        'video/broadcast': videoBroadcast,
        'application/oipfdrmagent': {
            // There's no DRM system (as PlayReady, an application asking for a licence for its protected video): its
            // messages answered so (OIPF DAE 7.6.1.1, result 5, unknown DRM system), else it waits for ever. And
            // SDRangel told, to say why the video won't play
            sendDRMMessage: function (msgType, msg, DRMSystemID) {
                var agent = this;
                var msgID = 'sdrangel-drm-' + (nextDrmMessage++);
                send({ t: 'drm', system: String(DRMSystemID || ''), type: String(msgType || '') });

                setTimeout(function () {
                    if (typeof agent.onDRMMessageResult === 'function') {
                        agent.onDRMMessageResult(msgID, '', 5);
                    }

                    var event = new Event('DRMMessageResult');
                    event.msgID = msgID;
                    event.resultMsg = '';
                    event.resultCode = 5;
                    agent.dispatchEvent(event);
                }, 0);

                return msgID;
            },
            DRMSystemStatus: function () { return 1; }     // DRM_UNKNOWN
        },
        'application/oipfparentalcontrolmanager': {
            parentalRatingSchemes: [],
            isPINEntryLocked: false,
            getBlockUnrated: function () { return false; }
        },
        'application/oipfsearchmanager': {
            createSearch: function () { return null; },
            getChannelConfig: function () { return videoBroadcast.getChannelConfig(); }
        }
    };

    function typeOf(object) {
        return (object.getAttribute('type') || '').toLowerCase();
    }

    // The objects' methods and properties, on every <object>, of the ones of its type: an application's objects may be
    // in its markup, before any script of its runs
    var methods = {};

    Object.keys(objectTypes).forEach(function (type) {
        Object.keys(objectTypes[type]).forEach(function (name) {
            if (typeof objectTypes[type][name] === 'function') {
                methods[name] = methods[name] || {};
                methods[name][type] = objectTypes[type][name];
            }
        });
    });

    Object.keys(methods).forEach(function (name) {
        if (name in HTMLObjectElement.prototype) {
            return;
        }

        Object.defineProperty(HTMLObjectElement.prototype, name, {
            configurable: true,
            value: function () {
                var method = methods[name][typeOf(this)];
                return method ? method.apply(this, arguments) : undefined;
            }
        });
    });

    var properties = {
        playState: function (o) { return typeOf(o) === 'video/broadcast' ? videoState(o).playState : undefined; },
        fullScreen: function (o) { return typeOf(o) === 'video/broadcast' ? videoState(o).fullScreen : undefined; },
        currentChannel: function (o) {
            return (typeOf(o) === 'video/broadcast') && (videoState(o).playState !== PLAYSTATE_UNREALIZED) ? makeChannel(config.channel) : null;
        },
        programmes: function (o) { return typeOf(o) === 'video/broadcast' ? programmes() : undefined; },
        configuration: function (o) { return typeOf(o) === 'application/oipfconfiguration' ? configuration : undefined; },
        localSystem: function (o) { return typeOf(o) === 'application/oipfconfiguration' ? localSystem : undefined; },
        xmlCapabilities: function (o) { return typeOf(o) === 'application/oipfcapabilities' ? capabilitiesXml() : undefined; },
        DRMSystemStatus: function (o) { return undefined; }
    };

    Object.keys(properties).forEach(function (name) {
        if (name in HTMLObjectElement.prototype) {
            return;
        }

        Object.defineProperty(HTMLObjectElement.prototype, name, {
            configurable: true,
            get: function () { return properties[name](this); }
        });
    });

    // The constants of video/broadcast
    [['PLAYSTATE_UNREALIZED', 0], ['PLAYSTATE_CONNECTING', 1], ['PLAYSTATE_PRESENTING', 2], ['PLAYSTATE_STOPPED', 3],
     ['COMPONENT_TYPE_VIDEO', 0], ['COMPONENT_TYPE_AUDIO', 1], ['COMPONENT_TYPE_SUBTITLE', 2]].forEach(function (c) {
        if (!(c[0] in HTMLObjectElement.prototype)) {
            Object.defineProperty(HTMLObjectElement.prototype, c[0], { configurable: true, value: c[1] });
        }
    });

    window.oipfObjectFactory = {
        isObjectSupported: function (type) { return String(type).toLowerCase() in objectTypes; },
        createVideoBroadcastObject: function () { return createObject('video/broadcast'); },
        createApplicationManagerObject: function () { return createObject('application/oipfApplicationManager'); },
        createConfigurationObject: function () { return createObject('application/oipfConfiguration'); },
        createCapabilitiesObject: function () { return createObject('application/oipfCapabilities'); },
        createDrmAgentObject: function () { return createObject('application/oipfDrmAgent'); },
        createParentalControlManagerObject: function () { return createObject('application/oipfParentalControlManager'); },
        createSearchManagerObject: function () { return createObject('application/oipfSearchManager'); },
        createVideoMpegObject: function () { return document.createElement('video'); }
    };

    function createObject(type) {
        var object = document.createElement('object');
        object.setAttribute('type', type);
        return object;
    }

    // Where the video's shown: in the bound video/broadcast object, as fractions of the page (1280 by 720 pixels), or
    // full screen if there's none, or it's set to full screen
    function reportVideo() {
        // The application's page's: not a frame's in it (as an advertising SDK's), which would say the video's full
        // screen, and its layout's width, in place of the page's
        if (window.top !== window) {
            return;
        }

        var message = { t: 'video', full: true };
        var width = window.innerWidth || 1280, height = window.innerHeight || 720;

        // Media SDRangel's playing first: it's the video shown
        var objects = (typeof playingMedia === 'function' ? playingMedia() : []).concat(videoObjects);

        for (var i = 0; i < objects.length; i++) {
            var object = objects[i];

            if (!object.isConnected || ((object instanceof HTMLObjectElement) && videoState(object).fullScreen)) {
                continue;
            }

            var style = window.getComputedStyle(object);
            var rect = object.getBoundingClientRect();

            // Which it is, for the log
            var what = object.tagName.toLowerCase() + (object.id ? '#' + object.id : '') + (object.type ? ' ' + object.type : '');

            if ((style.display === 'none') || (style.visibility === 'hidden') || (rect.width <= 0) || (rect.height <= 0)) {
                message = { t: 'video', full: false, hidden: true, what: what };
                continue;
            }

            message = { t: 'video', full: false, x: rect.left / width, y: rect.top / height, w: rect.width / width, h: rect.height / height, what: what };
            punch(object, rect);
            break;
        }

        if (message.full || message.hidden) {
            unpunch();
        }

        var text = JSON.stringify(message);

        if (text !== lastVideo) {
            lastVideo = text;
            send(message);
        }

        reportLayout();
    }

    // The width the page is laid out for: an application made for 1920 by 1080 (its html or body that wide) is zoomed
    // to fit, as a TV with a 1920 by 1080 browser would show it, rather than shown in part
    var lastLayout = '';

    function reportLayout() {
        var layoutWidth = 0;

        [document.documentElement, document.body].forEach(function (element) {
            if (element) {
                layoutWidth = Math.max(layoutWidth, parseFloat(window.getComputedStyle(element).width) || 0);
            }
        });

        layoutWidth = Math.round(layoutWidth);
        // With the viewport's width: what it was laid out in (while the view's resized, it's briefly not the width
        // it's zoomed to, which an application choosing its layout by media queries may follow)
        var key = layoutWidth + ',' + window.innerWidth;

        if (layoutWidth && (key !== lastLayout)) {
            lastLayout = key;
            send({ t: 'layout', w: layoutWidth, vw: window.innerWidth });
        }
    }

    // SDRangel draws the video under the page. On a TV it's shown over what's under its element in the page (it's drawn
    // in the element, or through a hole punched in the graphics to the video plane), so what's under it here mustn't
    // hide it: its own background, its ancestors' (as an opaque page's, cleared where it is, their colour drawn around
    // it), and other elements drawn under it (as an application's own player, loading, under an advert's video):
    // hidden if they're within it, else their backgrounds cleared where it is. Until it's no longer shown
    var punchProperties = ['background-color', 'background-image', 'background-position', 'background-size', 'background-repeat',
        'background-origin', 'background-clip', 'visibility'];
    var punched = [];       // Each element changed, with its own style's values before
    var punchKey = '';
    var punchObject = null;
    var punchRect = null;
    var punchColors = [];   // Each ancestor left as it was, with its colour then: done again if one changes

    function unpunch() {
        punched.forEach(function (p) {
            punchProperties.forEach(function (name) {
                p.element.style.setProperty(name, p.saved[name].value, p.saved[name].priority);
            });
        });
        punched = [];
        punchKey = '';
        punchObject = null;
        punchRect = null;
        punchColors = [];
    }

    function isTransparent(color) {
        return (color === 'transparent') || /rgba\(.*,\s*0\)$/.test(color);
    }

    function isPunched(element) {
        return punched.some(function (p) { return p.element === element; });
    }

    // Its style saved, to be put back
    function save(element) {
        var saved = {};

        punchProperties.forEach(function (name) {
            saved[name] = { value: element.style.getPropertyValue(name), priority: element.style.getPropertyPriority(name) };
        });

        punched.push({ element: element, saved: saved });
    }

    // Where an element's background's drawn: its border box, but the whole page for the root's (and the body's, when
    // it's given to the root, as the root has none), from the root's box (CSS 2 14.2)
    function backgroundBox(element) {
        var root = document.documentElement;
        var rootStyle = window.getComputedStyle(root);
        var canvas = (element === root) || ((element === document.body) && isTransparent(rootStyle.backgroundColor)
            && (rootStyle.backgroundImage === 'none'));

        if (!canvas) {
            return element.getBoundingClientRect();
        }

        var r = root.getBoundingClientRect();
        return { left: r.left, top: r.top, width: Math.max(r.width, root.scrollWidth, window.innerWidth),
            height: Math.max(r.height, root.scrollHeight, window.innerHeight) };
    }

    // An element's background cleared where the video is, its colour drawn around it. False if it has none to clear
    // (transparent), or can't be (an image the video doesn't cover)
    function cutBackground(element, rect) {
        var style = window.getComputedStyle(element);
        var color = style.backgroundColor;
        var box = backgroundBox(element);
        // The hole, in the element's background's box
        var x0 = Math.max(0, rect.left - box.left), y0 = Math.max(0, rect.top - box.top);
        var x1 = Math.min(box.width, rect.right - box.left), y1 = Math.min(box.height, rect.bottom - box.top);
        var covers = (x0 <= 0) && (y0 <= 0) && (x1 >= box.width) && (y1 >= box.height);

        if ((isTransparent(color) && (style.backgroundImage === 'none')) || ((style.backgroundImage !== 'none') && !covers)
            || (x1 <= x0) || (y1 <= y0)) {
            return false;
        }

        save(element);
        var images = [], positions = [], sizes = [];
        var piece = function (x, y, w, h) {
            if ((w > 0) && (h > 0) && !isTransparent(color)) {
                images.push('linear-gradient(' + color + ', ' + color + ')');
                positions.push(x + 'px ' + y + 'px');
                sizes.push(w + 'px ' + h + 'px');
            }
        };

        piece(0, 0, box.width, y0);                         // Above
        piece(0, y1, box.width, box.height - y1);           // Below
        piece(0, y0, x0, y1 - y0);                          // Left
        piece(x1, y0, box.width - x1, y1 - y0);             // Right
        element.style.setProperty('background-color', 'transparent', 'important');
        element.style.setProperty('background-image', images.length ? images.join(', ') : 'none', 'important');
        element.style.setProperty('background-position', positions.length ? positions.join(', ') : '0 0', 'important');
        element.style.setProperty('background-size', sizes.length ? sizes.join(', ') : 'auto', 'important');
        element.style.setProperty('background-repeat', 'no-repeat', 'important');
        element.style.setProperty('background-origin', 'border-box', 'important');
        element.style.setProperty('background-clip', 'border-box', 'important');
        return true;
    }

    // Elements drawn under the video, other than its ancestors: what's at points across it, after it (in the order
    // they're drawn, top first), all hit for the while (as an advertising SDK's video, and overlays, ignore the
    // pointer). Hidden if they're within it, else their backgrounds cut. Only added to, while the same video's
    // shown: once hidden they aren't hit
    var lastBeneath = 0;

    // An element, for the log: tag, id, classes, its box and background
    function describe(element) {
        var style = window.getComputedStyle(element);
        var r = element.getBoundingClientRect();
        var name = element.tagName.toLowerCase() + (element.id ? '#' + element.id : '')
            + ((typeof element.className === 'string') && element.className.trim() ? '.' + element.className.trim().split(/\s+/).slice(0, 2).join('.') : '');
        return name + ' [' + Math.round(r.left) + ',' + Math.round(r.top) + ' ' + Math.round(r.width) + 'x' + Math.round(r.height) + ']'
            + (isTransparent(style.backgroundColor) ? '' : ' bg ' + style.backgroundColor)
            + (style.backgroundImage !== 'none' ? ' image' : '') + (style.visibility === 'hidden' ? ' hidden' : '')
            + ((style.opacity !== '1') ? ' opacity ' + style.opacity : '');
    }

    var lastAboveLog = '';

    function punchBeneath(object, rect) {
        lastBeneath = Date.now();
        var hitAll = document.createElement('style');
        hitAll.textContent = '*, *::before, *::after { pointer-events: auto !important; }';
        (document.head || document.documentElement).appendChild(hitAll);
        var beneath = [];
        var above = [];

        for (var fy = 0.1; fy < 1; fy += 0.2) {
            for (var fx = 0.1; fx < 1; fx += 0.2) {
                var list = document.elementsFromPoint(rect.left + fx * rect.width, rect.top + fy * rect.height);
                var at = list.indexOf(object);

                for (var j = 0; j < at; j++) {
                    if ((above.indexOf(list[j]) < 0)) {
                        above.push(list[j]);
                    }
                }

                for (var i = at + 1; (at >= 0) && (i < list.length); i++) {
                    if (!list[i].contains(object) && (beneath.indexOf(list[i]) < 0)) {
                        beneath.push(list[i]);
                    }
                }
            }
        }

        hitAll.remove();
        var changed = [];

        beneath.forEach(function (element) {
            if (isPunched(element)) {
                return;
            }

            var r = element.getBoundingClientRect();
            var within = (r.left >= rect.left - 2) && (r.top >= rect.top - 2) && (r.right <= rect.right + 2) && (r.bottom <= rect.bottom + 2);

            if (within) {
                changed.push('hid ' + describe(element));
                save(element);
                element.style.setProperty('visibility', 'hidden', 'important');
            } else if (cutBackground(element, rect)) {
                changed.push('cut ' + describe(element));
            }
        });

        if (changed.length) {
            consoleLog('[sdrangel] under the video: ' + changed.join('; '));
        }

        // What's over it, that could hide it: logged when that changes
        var over = above.filter(function (element) {
            var style = window.getComputedStyle(element);
            return (style.visibility !== 'hidden') && (!isTransparent(style.backgroundColor) || (style.backgroundImage !== 'none')
                || /^(CANVAS|IMG|VIDEO|IFRAME|OBJECT|EMBED)$/.test(element.tagName));
        }).map(describe).join('; ');

        if (over !== lastAboveLog) {
            lastAboveLog = over;
            consoleLog('[sdrangel] over the video: ' + (over || 'nothing drawn'));
        }

        // What's drawn over the video (as an advert's countdown) kept, though it's in an element hidden as under it
        above.forEach(function (element) {
            var inHidden = punched.some(function (p) {
                return (p.element !== element) && p.element.contains(element) && (p.element.style.getPropertyValue('visibility') === 'hidden');
            });

            if (inHidden && !isPunched(element)) {
                save(element);
                element.style.setProperty('visibility', 'visible', 'important');
            }
        });
    }

    function punch(object, rect) {
        var key = [Math.round(rect.left), Math.round(rect.top), Math.round(rect.width), Math.round(rect.height)].join(',');

        if ((key === punchKey) && (object === punchObject) && punchColors.every(function (c) {
            return window.getComputedStyle(c.element).backgroundColor === c.color;
        })) {
            // As what's under it may have been added: now and then, as styling every element to find it is slow
            if ((object instanceof HTMLMediaElement) && (Date.now() - lastBeneath > 2000)) {
                punchBeneath(object, rect);
            }

            return;
        }

        unpunch();
        punchKey = key;
        punchObject = object;
        punchRect = rect;

        // The element itself: on a TV the video's drawn in it, hiding its background, which here would hide the video
        // under the page (as an advertising SDK's video element, black)
        var ownStyle = window.getComputedStyle(object);

        if (!isTransparent(ownStyle.backgroundColor) || (ownStyle.backgroundImage !== 'none')) {
            save(object);
            object.style.setProperty('background-color', 'transparent', 'important');
            object.style.setProperty('background-image', 'none', 'important');
        }

        for (var element = object.parentElement; element; element = element.parentElement) {
            if (!cutBackground(element, rect)) {
                punchColors.push({ element: element, color: window.getComputedStyle(element).backgroundColor });
            }
        }

        consoleLog('[sdrangel] video ' + describe(object) + ' in ' + window.innerWidth + 'x' + window.innerHeight + ': '
            + (punched.length ? 'backgrounds cleared: ' + punched.map(function (p) { return describe(p.element); }).join('; ') : 'nothing to clear')
            + '; html ' + window.getComputedStyle(document.documentElement).backgroundColor
            + ', body ' + (document.body ? window.getComputedStyle(document.body).backgroundColor : 'none'));
        lastAboveLog = '';

        // Media SDRangel plays for the application (as an advert, over its own player): not a video/broadcast object,
        // which applications put over their pages, to say where the broadcast's shown
        if (object instanceof HTMLMediaElement) {
            punchBeneath(object, rect);
        }
    }

    setInterval(reportVideo, 250);

    // dvb: URLs (TS 102 851) of carousel files, as the server serves them: the page is http://dvb-onid-tsid-sid-ctag.localhost,
    // as its carousel's dvb://onid.tsid.sid.ctag/ would be on a TV. "dvb:/path" is of the page's own carousel
    function mapUrl(url) {
        if ((typeof url !== 'string') || !/^dvb:/i.test(url) || /^dvb:\/\/current\.ait/i.test(url)) {
            return url;
        }

        var match = /^dvb:\/\/([0-9a-f]*)\.([0-9a-f]*)\.([0-9a-f]+)\.([0-9a-f]+)(\/.*)?$/i.exec(url);

        if (match) {
            var port = /\.localhost$/.test(location.hostname) ? location.port : config.serverPort;
            return 'http://dvb-' + (match[1] || 'x') + '-' + (match[2] || 'x') + '-' + match[3] + '-' + match[4] + '.localhost:'
                + port + (match[5] || '/');
        }

        var path = url.replace(/^dvb:/i, '');
        return new URL(path, location.href).href;
    }

    // A carousel's page's request to another site, by SDRangel's proxy where it has one (with Qt 5), which makes it with
    // the page's TV origin: of the page's own origin, so not cross-origin
    function proxyUrl(url) {
        if (!config.proxy || !/\.localhost$/.test(location.hostname)) {
            return url;
        }

        var target;

        try {
            target = new URL(url, location.href);
        } catch (e) {
            return url;
        }

        if (!/^https?:$/.test(target.protocol) || (target.origin === location.origin) || /\.localhost$/.test(target.hostname)) {
            return url;
        }

        return location.origin + config.proxy + '?url=' + encodeURIComponent(target.href);
    }

    var xhrOpen = XMLHttpRequest.prototype.open;
    XMLHttpRequest.prototype.open = function (method, url) {
        var args = Array.prototype.slice.call(arguments);
        args[1] = proxyUrl(mapUrl(String(url)));
        return xhrOpen.apply(this, args);
    };

    if (window.fetch) {
        var fetchOriginal = window.fetch;
        window.fetch = function (input, init) {
            return fetchOriginal.call(window, typeof input === 'string' ? proxyUrl(mapUrl(input)) : input, init);
        };
    }

    // Media an application plays from the internet, as iPlayer's: the browser can't decode it (H.264 and AAC aren't
    // built into Qt WebEngine), so SDRangel's player plays it, under the page, in the element's place. The element
    // stands in for it: its source isn't given to the browser, and its state (paused, currentTime, duration...) and
    // events (loadedmetadata, playing, timeupdate, ended...) are SDRangel's player's
    var srcDescriptor = Object.getOwnPropertyDescriptor(HTMLMediaElement.prototype, 'src');
    var mediaLoad = HTMLMediaElement.prototype.load;
    var mediaPlay = HTMLMediaElement.prototype.play;
    var setAttributeOriginal = Element.prototype.setAttribute;
    var removeAttributeOriginal = Element.prototype.removeAttribute;
    var mediaProxies = {};          // By id
    var nextMediaId = 1;
    var mediaState = new WeakMap(); // Element to its state

    function isStreamUrl(url) {
        return (typeof url === 'string') && /^https?:/i.test(url);
    }

    function timeRanges(ranges) {
        return {
            length: ranges.length,
            start: function (i) { return ranges[i][0]; },
            end: function (i) { return ranges[i][1]; }
        };
    }

    function fireMedia(element, name) {
        element.dispatchEvent(new Event(name));
    }

    // The element's properties, as its stand-in's
    function makeProxy(element) {
        var m = {
            id: nextMediaId++, url: '', paused: true, currentTime: 0, duration: NaN, readyState: 0, networkState: 0,
            ended: false, seeking: false, error: null, width: 0, height: 0, volume: 1, muted: false, playbackRate: 1
        };
        mediaState.set(element, m);
        mediaProxies[m.id] = element;

        var define = function (name, get, set) {
            Object.defineProperty(element, name, { configurable: true, get: get, set: set });
        };

        define('src', function () { return m.url; }, function (value) { setSource(element, value); });
        define('currentSrc', function () { return m.url; });
        define('currentTime', function () { return m.currentTime; }, function (value) {
            value = Number(value);

            if (isNaN(value)) {
                return;
            }

            m.currentTime = value;
            m.seeking = true;
            m.ended = false;
            fireMedia(element, 'seeking');
            send({ t: 'media', cmd: 'seek', id: m.id, position: value });

            // Playing, it's done when SDRangel's playing from there. Paused (as an application's seek bar pauses while
            // it's moved), there's nothing to wait for: SDRangel goes on from there when it's played. An application
            // waiting for it to be done (as TVP VOD's player, which won't seek again until it is) would wait for ever
            if (m.paused) {
                setTimeout(function () {
                    if (m.seeking && m.paused) {
                        m.seeking = false;
                        fireMedia(element, 'timeupdate');
                        fireMedia(element, 'seeked');
                    }
                }, 0);
            }
        });
        define('duration', function () { return m.duration; });
        define('paused', function () { return m.paused; });
        define('ended', function () { return m.ended; });
        define('seeking', function () { return m.seeking; });
        define('readyState', function () { return m.readyState; });
        define('networkState', function () { return m.networkState; });
        define('error', function () { return m.error; });
        define('videoWidth', function () { return m.width; });
        define('videoHeight', function () { return m.height; });
        define('volume', function () { return m.volume; }, function (value) { m.volume = value; fireMedia(element, 'volumechange'); });
        define('muted', function () { return m.muted; }, function (value) { m.muted = !!value; fireMedia(element, 'volumechange'); });
        define('playbackRate', function () { return m.playbackRate; }, function (value) { m.playbackRate = value; });
        define('seekable', function () {
            if (isFinite(m.duration) && (m.duration > 0)) {
                return timeRanges([[0, m.duration]]);
            }
            return timeRanges(m.readyState > 0 ? [[0, m.currentTime]] : []);
        });
        define('buffered', function () { return timeRanges(m.readyState > 0 ? [[m.currentTime, m.currentTime + 5]] : []); });
        define('played', function () { return timeRanges([]); });

        element.play = function () {
            if (!m.url) {
                return Promise.resolve();
            }

            if (m.paused || m.ended)
            {
                if (m.ended) {
                    m.currentTime = 0;
                    m.ended = false;
                    send({ t: 'media', cmd: 'seek', id: m.id, position: 0 });
                }

                m.paused = false;
                fireMedia(element, 'play');
                send({ t: 'media', cmd: 'play', id: m.id });
            }

            return Promise.resolve();
        };
        element.pause = function () {
            if (m.url && !m.paused) {
                m.paused = true;
                send({ t: 'media', cmd: 'pause', id: m.id });
                fireMedia(element, 'pause');
            }
        };
        element.load = function () {
            var source = m.url || sourceChild(element);

            if (source) {
                setSource(element, source);
            } else {
                stopProxy(element);
            }
        };

        return m;
    }

    function sourceChild(element) {
        var sources = element.getElementsByTagName ? element.getElementsByTagName('source') : [];

        for (var i = 0; i < sources.length; i++) {
            var src = sources[i].getAttribute('src');

            if (src) {
                return mapUrl(new URL(src, location.href).href);
            }
        }

        return '';
    }

    function stopProxy(element) {
        var m = mediaState.get(element);

        if (m && m.url) {
            send({ t: 'media', cmd: 'stop', id: m.id });
            m.url = '';
            m.paused = true;
            m.readyState = 0;
            m.networkState = 0;
            m.duration = NaN;
            fireMedia(element, 'emptied');
        }
    }

    function setSource(element, value) {
        var url = value ? mapUrl(new URL(String(value), location.href).href) : '';

        if (!isStreamUrl(url)) {
            stopProxy(element);

            if (value && srcDescriptor && srcDescriptor.set) {
                srcDescriptor.set.call(element, value);
            }

            return;
        }

        var m = mediaState.get(element) || makeProxy(element);
        consoleLog('[sdrangel] media ' + m.id + ': ' + url);
        m.url = url;
        m.paused = true;
        m.ended = false;
        m.currentTime = 0;
        m.duration = NaN;
        m.readyState = 0;
        m.networkState = 2;     // NETWORK_LOADING
        m.error = null;
        fireMedia(element, 'loadstart');
        send({ t: 'media', cmd: 'load', id: m.id, url: url });

        if (element.autoplay || element.hasAttribute('autoplay')) {
            element.play();
        }
    }

    if (srcDescriptor && srcDescriptor.set) {
        Object.defineProperty(HTMLMediaElement.prototype, 'src', {
            configurable: true,
            get: srcDescriptor.get,
            set: function (value) { setSource(this, value); }
        });
    }

    Element.prototype.setAttribute = function (name, value) {
        if ((this instanceof HTMLMediaElement) && (String(name).toLowerCase() === 'src')) {
            setSource(this, value);
            return;
        }

        return setAttributeOriginal.apply(this, arguments);
    };

    Element.prototype.removeAttribute = function (name) {
        if ((this instanceof HTMLMediaElement) && (String(name).toLowerCase() === 'src')) {
            stopProxy(this);
        }

        return removeAttributeOriginal.apply(this, arguments);
    };

    // An element given <source> children, then loaded or played
    HTMLMediaElement.prototype.load = function () {
        var source = sourceChild(this);

        if (isStreamUrl(source)) {
            setSource(this, source);
            return;
        }

        return mediaLoad.apply(this, arguments);
    };

    HTMLMediaElement.prototype.play = function () {
        var source = sourceChild(this);

        if (isStreamUrl(source)) {
            setSource(this, source);
            return this.play();
        }

        return mediaPlay.apply(this, arguments);
    };

    // What SDRangel's player can play
    var canPlayType = HTMLMediaElement.prototype.canPlayType;
    HTMLMediaElement.prototype.canPlayType = function (type) {
        var t = String(type).toLowerCase();
        var result = /application\/dash\+xml|mpegurl|video\/mp4|audio\/mp4|video\/mp2t|audio\/mpeg|audio\/aac/.test(t) ? 'probably'
            : canPlayType.call(this, type);
        consoleLog('[sdrangel] canPlayType ' + type + ' "' + result + '"');
        return result;
    };

    // From SDRangel's player
    function mediaEvent(id, type, data) {
        var element = mediaProxies[id];
        var m = element && mediaState.get(element);

        if (!m || !m.url) {
            return;
        }

        data = data || {};

        if (type === 'metadata') {
            m.duration = (data.duration === null) || (data.duration === undefined) ? Infinity : data.duration;
            m.width = data.width || 0;
            m.height = data.height || 0;
            m.readyState = 4;   // HAVE_ENOUGH_DATA
            m.networkState = 1;
            fireMedia(element, 'durationchange');
            fireMedia(element, 'loadedmetadata');
            fireMedia(element, 'loadeddata');
            fireMedia(element, 'canplay');
            fireMedia(element, 'canplaythrough');
        } else if (type === 'playing') {
            if (m.seeking) {
                m.seeking = false;
                fireMedia(element, 'seeked');
            }
            fireMedia(element, 'playing');
        } else if (type === 'time') {
            m.currentTime = data.position;
            fireMedia(element, 'timeupdate');
        } else if (type === 'waiting') {
            fireMedia(element, 'waiting');
        } else if (type === 'ended') {
            m.ended = true;
            m.paused = true;
            fireMedia(element, 'pause');
            fireMedia(element, 'ended');
        } else if (type === 'error') {
            m.error = { code: data.code || 4, message: data.message || '', MEDIA_ERR_ABORTED: 1, MEDIA_ERR_NETWORK: 2,
                MEDIA_ERR_DECODE: 3, MEDIA_ERR_SRC_NOT_SUPPORTED: 4 };
            fireMedia(element, 'error');
        }
    }

    // Playing elements, for where the video goes
    function playingMedia() {
        var list = [];

        Object.keys(mediaProxies).forEach(function (id) {
            var element = mediaProxies[id];
            var m = mediaState.get(element);

            if (m && m.url && element.isConnected) {
                list.push(element);
            }
        });

        return list;
    }

    // Media Source Extensions, for the log: applications that use them can't be played
    if (window.MediaSource) {
        var addSourceBuffer = MediaSource.prototype.addSourceBuffer;
        MediaSource.prototype.addSourceBuffer = function (type) {
            consoleLog('[sdrangel] MSE addSourceBuffer ' + type);
            return addSourceBuffer.apply(this, arguments);
        };
    }

    // Dialogs would block the application
    window.alert = function (text) { consoleLog('alert: ' + text); };
    window.confirm = function (text) { consoleLog('confirm: ' + text); return false; };
    window.prompt = function (text) { consoleLog('prompt: ' + text); return null; };

    // SDRangel's status (as the red button loading), over everything the application shows
    var statusElement = null;

    function showStatus(text) {
        if (!text) {
            if (statusElement && statusElement.parentNode) {
                statusElement.parentNode.removeChild(statusElement);
            }
            statusElement = null;
            return;
        }

        if (!statusElement || !statusElement.isConnected) {
            statusElement = document.createElement('div');
            statusElement.setAttribute('style', 'position: fixed; right: 64px; bottom: 48px; z-index: 2147483647; ' +
                'padding: 8px 16px; border-radius: 8px; background: rgba(16, 16, 16, 0.8); color: #f1efe8; ' +
                'font: 24px sans-serif; pointer-events: none');
            (document.body || document.documentElement).appendChild(statusElement);
        }

        statusElement.textContent = text;
    }

    window.__sdrangelHbbTV = {
        status: showStatus,
        // A key in the application's keyset, down or up
        key: function (code, down) {
            if (down) {
                keyEvent('keydown', code);
                keyEvent('keypress', code);
            } else {
                keyEvent('keyup', code);
            }
        },
        update: function (newConfig) {
            config = newConfig || config;
        },
        // An event of SDRangel's player, playing an element's media
        media: function (id, type, data) {
            mediaEvent(id, type, data);
        },
        // After a setChannel(): the service's playing, or it couldn't be (error: OIPF's error code)
        channelChanged: function (ok, error) {
            videoObjects.slice().concat([]).forEach(function (object) {
                if (videoState(object).playState !== PLAYSTATE_CONNECTING) {
                    return;
                }

                var channel = makeChannel(config.channel);
                var requested = videoState(object).pendingChannel;
                videoState(object).pendingChannel = null;

                if (ok) {
                    setPlayState(object, PLAYSTATE_PRESENTING);
                    fire(object, 'ChannelChangeSucceeded', { channel: channel }, [channel]);
                } else {
                    // The channel asked for (OIPF DAE 7.13.1.3), not the one still playing
                    var failed = requested || channel;
                    setPlayState(object, PLAYSTATE_UNREALIZED, error);
                    fire(object, 'ChannelChangeError', { channel: failed, errorState: error }, [failed, error]);
                }
            });
        }
    };

    document.addEventListener('DOMContentLoaded', function () { send({ t: 'loaded', url: location.href }); });
    window.addEventListener('error', function (e) {
        send({ t: 'error', message: String(e.message), source: String(e.filename || ''), line: e.lineno || 0 });
    });
})();
