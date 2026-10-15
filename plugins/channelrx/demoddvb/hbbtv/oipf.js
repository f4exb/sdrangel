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
    var keysetValue = 0;
    var ownerApplication = null;
    var videoObjects = [];          // video/broadcast objects bound to the service, or with a channel set
    var state = new WeakMap();      // Each object's state
    var lastVideo = '';

    function send(message) {
        console.log('\u0001sdrangel:' + JSON.stringify(message));
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
            sendDRMMessage: function () { return ''; },
            DRMSystemStatus: function () { return 1; }
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

            if ((style.display === 'none') || (style.visibility === 'hidden') || (rect.width <= 0) || (rect.height <= 0)) {
                message = { t: 'video', full: false, hidden: true };
                continue;
            }

            message = { t: 'video', full: false, x: rect.left / width, y: rect.top / height, w: rect.width / width, h: rect.height / height };
            break;
        }

        var text = JSON.stringify(message);

        if (text !== lastVideo) {
            lastVideo = text;
            send(message);
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
        console.log('[sdrangel] media ' + m.id + ': ' + url);
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
        console.log('[sdrangel] canPlayType ' + type + ' "' + result + '"');
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
            console.log('[sdrangel] MSE addSourceBuffer ' + type);
            return addSourceBuffer.apply(this, arguments);
        };
    }

    // Dialogs would block the application
    window.alert = function (text) { console.log('alert: ' + text); };
    window.confirm = function (text) { console.log('confirm: ' + text); return false; };
    window.prompt = function (text) { console.log('prompt: ' + text); return null; };

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
                videoState(object).pendingChannel = null;

                if (ok) {
                    setPlayState(object, PLAYSTATE_PRESENTING);
                    fire(object, 'ChannelChangeSucceeded', { channel: channel }, [channel]);
                } else {
                    setPlayState(object, PLAYSTATE_UNREALIZED, error);
                    fire(object, 'ChannelChangeError', { channel: channel, errorState: error }, [channel, error]);
                }
            });
        }
    };

    document.addEventListener('DOMContentLoaded', function () { send({ t: 'loaded', url: location.href }); });
    window.addEventListener('error', function (e) {
        send({ t: 'error', message: String(e.message), source: String(e.filename || ''), line: e.lineno || 0 });
    });
})();
