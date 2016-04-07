# Digital Speech Decoder 1.7.0-dev
Copyright (C) 2010 DSD Author
GPG Key ID: 0x3F1D7FD0 (74EF 430D F7F2 0A48 FCE6  F630 FAA2 635D 3F1D 7FD0)

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
    REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
    AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
    INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
    LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
    OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
    PERFORMANCE OF THIS SOFTWARE.

DSD is able to decode several digital voice formats from discriminator
tap audio and synthesize  the decoded speech.  Speech
synthesis requires mbelib, which is a separate package.


### Supported formats

#### P25 Phase 1

Widely deployed radio standard used in public safety and amateur radio.

Support includes decoding and synthesis of speech,
display of all link control info, and the ability to save
and replay .imb data files

#### ProVoice

EDACS Digital voice format used by public safety and amateur radio.

Support includes decoding and synthesis of speech and
the ability to save and replay .imb data files.

Note: not enabled by default, use `-fp` to enable.

#### X2-TDMA

Two slot TDMA system currently being deployed by several
public safety organizations.   Based on the DMR
standard with extensions for P25 style signaling.

Support includes decoding and synthesis of speech,
display of all link control info, and the ability to save
and replay .amb data files

#### DMR/MOTOTRBO

"Digital Mobile Radio" Eurpoean two slot TDMA standard.
MOTOTRBO is a popular implementation of this standard.

Support includes decoding and synthesis of speech and
the ability to save and replay .amb data files.

#### NXDN

Digital radio standard used by NEXEDGE and IDAS brands.
Supports both 9600 baud (12.5 kHz) and
4800 baud (6.25 kHz) digital voice.

Support includes decoding and synthesis of speech and
the ability to save and replay .amb data files.

#### D-STAR

Amateur radio digital voice standard

This is an earlier version of the AMBE codec than the one
used by most of the protocols. Support for this was added by
various developers.

### Unsupported formats in version 1.6 considered for future development:

#### P25 Phase 2

This is not yet a published standard.  Full support is
expected once the standard is published and there are
systems operating to test against.  Phase 2 will use
a vocoder supported by mbelib.

#### OpenSKY

It is possible that the four slot version uses a vocoder
supported by mbelib.  The two slot version does not.

### Supported demodulation optimizations in version 1.6:

#### C4FM

Continuous envelope 2 or 4 level FSK with relatively
sharp transitions between symbols.  Used by most P25
systems.

Optimizations include calibrating decision points only
during sync, 4/10 sample window per symbol, and symbol
edge timing calibration.

#### GFSK

Continuous envelope 2 or 4 level FSK with a narrower
Gaussian/"raised cosine" filter that affects transitions
between symbols.  Used by DMR/MOTOTRBO, NXDN and many
others.  Noisy C4FM signals may be detected as GFSK

but this is ok, the optimization changes will help with
noisy signals.

Optimizations are similar to C4FM except symbol transitions
are only kept out of the middle 4 samples and only the
middle two samples are used.

#### QPSK

Quadrature Phase Shift Keying (and variants) used in
some P25 systems and all known X2-TDMA systems.  May be
advertised under the marketing term "LSM"

Optimizations include continuous decision point
calibration, using middle two samples, and using the
symbol midpoint "spike" for symbol timing.

## Installation

DSD should easily compile on any Linux or *BSD system with gcc.
There are some debugging/development options in `config.h` that
normal users will want to leave disabled as they can severely
impact performance.

### Requirements
* cmake
* mbelib
* sndfile

### Example building instructions on Ubuntu:

````
sudo apt-get update
sudo apt-get install git make cmake # Update packages
git clone <URL of git repository>   # Something like: git@github.com:USERNAME/dsd.git
cd dsd                              # Move into source folder
mkdir build                         # Create build directory
cd build                            # Move to build directory
cmake ..                            # Create Makefile for current system
make                                # Compiles DSD
sudo make install                   # Installs DSD to the system
````

## Operation

There are two main operating modes, "Live scanner" and "Play files"

    Usage: dsd [options]            Live scanner mode

Live Scanner mode takes 48KHz/16 bit mono audio samples from a
sound card input and decodes speech in real time.  Options are provided
for controling information display and saving mbe data files.

The synthesized speech can be  output to a soundcard and/or a
.wav file.

    Usage:  dsd [options] -r <files> Read/Play saved mbe data from file(s)


Play files mode reads mbe data from files specified on the command
line (including wildcards) and synthesizes speech from those files.
The synthesized speech can be  output to a soundcard and/or a
.wav file. The `-r` command line options is used to activate Play files
mode.

### Display modes

There are two main display modes in Live scanner mode.  "Errorbars"
and "Datascope".

Errorbars mode output for P25 Phase 1 looks like this:

````
Sync:  -P25p1   mod: C4FM inlvl: 39% nac:  5C2 src:        0 tg: 32464  TDULC
Sync:  -P25p1   mod: C4FM inlvl: 39% nac:  5C2 src:        0 tg: 32464  TDULC
Sync:  -P25p1   mod: C4FM inlvl: 39% nac:  5C2 src:        0 tg: 32464  TDULC
Sync:  -P25p1   mod: C4FM inlvl: 39% nac:  5C2 src:        0 tg: 32464  TDULC
Sync:  -P25p1   mod: C4FM inlvl: 38% nac:  5C2 src:        0 tg: 32464  TDU
Sync:  -P25p1   mod: C4FM inlvl: 38% nac:  5C2 src:        0 tg: 32464  HDU
Sync:  -P25p1   mod: C4FM inlvl: 42% nac:  5C2 src:        0 tg: 32464  LDU1  e:
Sync: (-P25p1)  mod: C4FM inlvl: 39% nac:  5C2 src:    52610 tg: 32464 (LDU2) e:
Sync:  -P25p1   mod: C4FM inlvl: 38% nac:  5C2 src:    52610 tg: 32464  LDU1  e:
Sync:  -P25p1   mod: C4FM inlvl: 39% nac:  5C2 src:    52610 tg: 32464  LDU2  e:
Sync:  -P25p1   mod: C4FM inlvl: 39% nac:  5C2 src:    52610 tg: 32464  LDU1  e:
Sync:  -P25p1   mod: C4FM inlvl: 39% nac:  5C2 src:    52610 tg: 32464  LDU2  e:
Sync:  -P25p1   mod: C4FM inlvl: 39% nac:  5C2 src:    52610 tg: 32464  LDU1  e:
````

* "Sync" indicates the frame type detected and whether the polarity is
positive or negative.  DSD automatically detects and handles either
polarity except for DMR/MOTOTRBO/X2-TDMA which unfortunatley use both
sync polarities.

* Most combinations of transmitter, receiver and soundcard show netagive
(-) polarity for X2-TDMA signals and (+) polarity for DMR/MOTOTRBO so
those are the defaults.

    * You may need to use the `-x` option to select non-inverted polarity if
    you are not getting usable X2-TDMA/MOTOTRBO/DMR speech.  As they use both
    normal and inverted sync it is not possible to detect polariy
    automatically.

* "mod" indicates the current demodulation optimizations.

* "inlvl" indicates the audio input level.  QPSK signals tend to appear
much "wider" than C4FM from a discriminator tap so it is important
to set your input gain using a QPSK signal if you plan to montir them.
It is not necessary nor desirable to get to 100%, in fact your sound
card may max out below 100%.  It is best to use the Datascope mode for
setting input gain (see below).  Typical values with good results are
40% for C4FM and 66% for QPSK.

* "nac" is the P25 Phase 1 Network Access Code.  This is a 12 bit field
in each P25 Phase 1 header.  It should not be confused with the 16
bit System ID used in non-P25 trunking control channels.

* "src" is the radio id of the trasmitting subscriber unit.

* "tg" is the talkgroup derived from link control information.

* "HDU/LDU1/LDU2/TDU/TDULC" are P25 Phase 1 frame types, referred to as
frame subtype within DSD.

* "e:" is the beginning of the errorbars display.  Each "=" indicates a
detected error within the voice data.  "R" and "M" indicat that a voice
frame was repeated or muted due to excessive errors.

* Values in parentheses () indicate an assumption (soft decision) was
made based on the previous frame.

Errorbars mode output for X2-TDMA looks like this:
````
Sync:  -X2-TDMA  mod: QPSK inlvl: 59% src:    17211 tg:   197 [SLOT0]  slot1   VOICE e:
Sync:  -X2-TDMA  mod: QPSK inlvl: 47% src:    17211 tg:   197 [SLOT0]  slot1   VOICE e:
Sync:  -X2-TDMA  mod: QPSK inlvl: 43% src:    17211 tg:   197 [SLOT0]  slot1   VOICE e:
Sync: (-X2-TDMA) mod: QPSK inlvl: 28% src:    17211 tg:   197 [SLOT0]  slot1   VOICE e:
````

DMR/MOTOTRBO display is similar except it does not yet show source
and talkgroup information.

As of version 1.2 DSD shows which specific TDMA slots are active (with
capital SLOT letters) and which slot is currently being monitored (with
square brackets [].  Noisy/degraded signals will affect the accuracy
of this display.

The frame subtypes (Voice/LC etc) are shown based on the DMR standard
types.

Datascope mode output looks like this:

````
Demod mode:     C4FM                Nac:                      8C3
Frame Type:     P25 Phase 1         Talkgroup:              16528
Frame Subtype:  LDU1                Source:                     0
TDMA activity:   slot0   slot1      Voice errors:
+----------------------------------------------------------------+
|                   #    ^      !|      ^   #                    |
|                    *           |          *                    |
|                    *           |          *                    |
|                    *           |  *       *                    |
|                    *      *    |  *       *                    |
|                    *      *    |  **      *                    |
|                    *      **   |  **      *                    |
|                   **      **   |  **      *                    |
|                   **      **   |  **      *                    |
|                   **      **   |  **      *                    |
+----------------------------------------------------------------+
                          C4FM Example
````
````
Demod mode:     C4FM                Nac:                      126
Frame Type:     P25 Phase 1         Talkgroup:              25283
Frame Subtype:  LDU2                Source:                     0
TDMA activity:   slot0   slot1      Voice errors:
+----------------------------------------------------------------+
|            #      ^            !            ^       #          |
|                          *     |                               |
|                          *     |                               |
|                          **    |                               |
|                          **    |                    *          |
|            *             **    |    *               *          |
|            **            **    |    *               *          |
|            ***           **    |    **              *          |
|            ***           **    |    ***             *          |
|            ***          ****   |   ****           * *          |
+----------------------------------------------------------------+
                          QPSK Example
````

At the top is various information about the signal, similar to the
information provided in Errorbars mode.  The large box is similar to
a spectrum analyzer viewing the channel bandwidth.

The horizontal axis is the input audio level, minimum on the left and
maximum on the right.   The vertical axis is the number of samples
seend at each audio level.

The "*" symbols represent the number of audio
samples that were at each level during the aggregation period.
(default = 36 symbols) The `-S` options controls the aggregation period
as well as the QPSK tracking symbol buffer, so changing that will affect
QPSK performance as well as the Datascope display.

As you can see from the figures above, clean C4FM signals tend to have
four very sharply defined audio levels.  The datascope pattern also
tends to be faily stable with minor shifts left and right as the
receiver tries to frequency track any DC offset.

QPSK signals on the other hand tend to appear much broader (and artifact
of how they are distored by FM PLL discriminators).  They also tend
to vary wildly in width and centering.  This is especially true when
monitoring simulcast systems. Muliple QPSK signals interfere much more
dramatically with an FM discriminator than C4FM signals.

    For this reason it is important to isolate your receiver to one
    transmitter tower, _especially_ for QPSK signals.

    The "#" symbols indicate the detected min/max values that are used
    to calibrate the symbol decision points. These are indicated by
    "!" for the center decision point and "^" for the mid decision points.

### Display Options

There are several options to control the type and quantity of
information displayed in Errorbars mode:

````
-e             Show Frame Info and errorbars (default)
-pe            Show P25 encryption sync bits
-pl            Show P25 link control bits
-ps            Show P25 status bits and low speed data
-pt            Show P25 talkgroup info
-q             Don't show Frame Info/errorbars
-s             Datascope (disables other display options)
-t             Show symbol timing during sync
-v <num>       Frame information Verbosity
-z <num>       Frame rate for datascope
````

Most of these options are self explanitory.  Symbol timing is a noisy
option that allows you to view the quality of the frame sync samples
and accuracy of the symbol timing adjustments.

Symbol Timing display looks like this:
````
Symbol Timing:
----------
----------
----------
----------
----------
-+++++++++ 1
+---------- 0
----------
++++++++++ 0
++++++++++
---------- 0
----------
++++++++++ 0
++++++++++
++++++++++
++++++++++
---------- 0
++++++++++ 0
---------- 0
++++++++++ 0
++++++++++
++++++++++
++++++++++
++++++++++
C4FM example
````
````
Symbol Timing:
+---------
----------
----------
----------
-----X---- 5
--+++O++++- 4
----------
----X----- 4
++++O++--- 4
--++O++++- 4
----X----- 4
----------
++++O+++-- 4
-+++O+++-- 4
--++O+++-- 4
--++O+++-- 4
----------
++++O++++- 4
----------
++++O+++-- 4
-+++O++++- 4
-+++O+++++ 4
-+++O++--- 4
--++O+++-- 4
QPSK example
````

Symbol timing is only displayed for symbols during the frame sync
period.  Each horizontal line represents the 10 audio samples for each
symbol.  "-" indicates an audio sample below the center reference level
and "+" represents a sample above center.  "X" indicates a low spike
below a reference threshold  (reference minimum for C4FM and 80%
of reference minimum for QPSK).  "O" represents a high spike above
the high reference threshold.  The numbers to the right indicate which
sample position the targeted transition occurred (+/- for C4FM or
spike high/low for QPSK).  The number of audio samples for the next
symbol are adjusted to get this value closer to the target (0 for
C4FM and 4 for QPSK).  This shows how DSD maintains accurate symbol
timing.  Symbol timing adjustments are only made during sync, which
is the only time reliable transitions can be observed.

In both examples above the symbol timing was off by one sample at
the beginning of the frame sync period and was adjusted.  Generally
if you see any spike values "X/O" in C4FM mode, or lots of them in
QPSK mode it indicates noise on the input signal.

### Input/Output Options

````
-i <device/file> Audio input device/file (default is /dev/audio)
-o <device>      Audio output device (default is /dev/audio)
-d <dir>         Create mbe data files, use this directory
-r <files>       Read/Play saved mbe data from file(s)
-g <num>         Audio output gain (default = 0 = auto)
-n               Do not send synthesized speech to audio output device
-w <file>        Output synthesized speech to a .wav file
````

The audio in device can be a sound card OR a .wav file if the file
is in the exact format 48k/16bits/mono/pcm.  Audio in should be an
unfilterd discriminator tap signal.

The audio out device should be a sound card (use the `-w` options to
output to a .wav file).

If the audio in device is the same as the audio out device, the
synthesized speech has to be upsampled to the 48k sample rate required
for input.  A fast upsample function is provided but still leaves some
artifacts.

    The best sound and minimum cpu usage is achieved with separate sound
    cards for input and output

If you specify different input/output devices DSD will use 8k as the
output sample rate and the lack of resampling results in much better
audio as well as lowe cpu consumption.

If you are using onboard "AC97" sound device you may find that DSD uses
much more cpu than expected, in some cases more than is available.
This is because many AC97 sound devices are designed to rely on CPU
processing power instead of hardware.  You may also find that 8k sample
rate output is upsampled in the driver using a very basic algorithim
resulting in severe distortion.  The solution is to use a real hardware
sound device (pci card, usb device etc).

As of version 1.2 DSD now automatically levels the output audio. This
greately improves readability and eliminates the painful effects of
noise bursts.  You can specify a fixed audio output gain with the -g
option.

### Scanner control options:
````
-B <num>       Serial port baud rate (default=115200)
-C <device>    Serial port for scanner control (default=/dev/ttyUSB0)
-R <num>      Resume scan after <num> TDULC frames or any PDU or TSDU
````

On some P25 systems Packet Data Units (PDU) are sent on the same
frequencies used for voice traffic.  If done constantly this can
be a severe hinderance to scanning the system in conventional
mode.  The -R option enables sending a "resume scan" command to
a scanner connected to a serial port.  Use `-B` and `-C` to set the baud
rate and serial port device if necessary.

### Decoder options
````
-fa           Auto-detect frame type (default)
-f1           Decode only P25 Phase 1
-fd           Decode only D-STAR*
-fi           Decode only NXDN48* (6.25 kHz) / IDAS*
-fn           Decode only NXDN96 (12.5 kHz)
-fp           Decode only ProVoice*
-fr           Decode only DMR/MOTOTRBO
-fx           Decode only X2-TDMA
-l            Disable Filters (not recommended)
-ma           Auto-select modulation optimizations (default)
-mc           Use only C4FM modulation optimizations
-mg           Use only GFSK modulation optimizations
-mq           Use only QPSK modulation optimizations
-pu           Unmute Encrypted P25
-u <num>      Unvoiced speech quality (default=3)
-xx           Expect non-inverted X2-TDMA signal
-xr           Expect inverted DMR/MOTOTRBO signal
````
\* denotes frame types that cannot be auto-detected.

ProVoice and NXDN48 not auto-detected as use different symbol
rates (9600 and 2400) than most formats (4800).

MBE speech synthesis is broken down into two main types of sounds,
"Voiced" and "Unvoiced".  Voiced speech bands are synthesized with
a single sine wave centered in the frequency band with the appropriate
phase and amplitude.

Unvoiced speech is supposed to be generated with a noise source, 256
point DFT a number of band filters, followed by a 256 point inverse DFT.
For computational simplicity mbelib uses a different method.  For each
unvoiced speech band, a number of sine waves are generated, each with a
different random initial phase.  The number of waves used per band is
controlled by the `-u` option.  A setting of 4 would approximate the
performance of the 256 point DFT method as the maximum number of voice
bands is 56, and very low frequencies are not synthesized.  Values less
than 3 have a noticable lack of unvoiced speech and/or artifacts.  The
defualt of 3 provides good speech quality with reasonable cpu use.
Increasing the quality above the default rapidly consumes more CPU for
increasingly diminishing returns.


#### Advanced decoder options
````
-A <num>      QPSK modulation auto detection threshold (default=26)
-S <num>      Symbol buffer size for QPSK decision point tracking
            (default=36)
-M <num>      Min/Max buffer size for QPSK decision point tracking
            (default=15)
````

### Encryption

Decryption of speech is **NOT** supported, even if you lawfully posess the
encryption keys.  Decryption support will not be added in the future as
the authors wish to steer as far away from the legal issues associated
with encryption as possible.


    We realize that there are many legitemate and lawful uses of decryption
    software including system/interoperability testing and lawful monitoring.
    This software is distributed under a liberal BSD license so there is
    nothing to stop others from supplying patches, forking this project or
    incorporating it into a commercial product and adding decryption support.

    There is support for displaying the encryption sync bits transmitted in
    the clear on P25 Phase 1 systems.  These bits do not allow for the
    decryption of signals without the secret encryption keys.  The
    encryption sync bits are useful for determining whether a signal is
    encrypted vs merely noisy or degraded.  As the encryption sync bits
    typically include long strings of zeros when a transmission is not
    encrypted they can also be used to visually estimate bit error rates.
