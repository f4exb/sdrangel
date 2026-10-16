# Native DRM receiver core

`modemdrm` is the dependency-light core for SDRangel's native Digital Radio
Mondiale receiver. The implementation currently provides:

- ETSI ES 201 980 V4.3.1 mode and spectrum-occupancy parameters;
- configurable carrier search ranges and optional forced mode/occupancy
  acquisition constraints;
- cyclic-prefix mode, timing and fractional carrier-offset estimation;
- standard reference/FAC cell maps for robustness modes A-E;
- OFDM extraction, pilot channel estimation and reliability-weighted soft decisions;
- punctured rate-1/6 convolutional coding, interleaving and FAC decoding;
- three-block coherent FAC acquisition, including the supplied legacy DRM+ vectors;
- locked-window FAC tracking that refreshes cyclic-prefix timing and fractional
  carrier offset, uses multi-frame channel estimation, and bridges one erased
  FAC block when the other two validate before falling back to full acquisition;
- 4-QAM SDC channel decoding for A-D and both Mode E code rates, plus
  two-level 16-QAM SDC decoding for A-D;
- CRC-checked, bounded SDC entity parsing for multiplex layouts, labels,
  time/date, audio, language/country and AFS metadata, with database
  accumulation across the AFS-index cycle and list-version replacement;
- exact MSC cell maps for every valid mode/occupancy, ordered extraction,
  transmission-superframe validation, dummy-cell removal, multiplex-frame
  splitting, and short/long cell deinterleaving with reliability propagation;
- Mode E 4-QAM/16-QAM and A-D 16-QAM/64-QAM equal- and unequal-protection MSC,
  with iterative 64-QAM multilevel refinement for marginal signals and
  decoding at every applicable protection level,
  SDC-directed logical-stream extraction, Mode E audio pairing, and bounded
  AAC superframe/access-unit parsing;
- stateful xHE-AAC superframe parsing with header CRC-8, reverse directory,
  delayed-border, cross-superframe access-unit and trailing CRC-16 handling;
- CRC-checked DRM text-message segment assembly with UTF-8 output and
  clear-display command handling;
- a narrow audio-decoder interface with capability reporting, concealment,
  reset, and an optional FDK-AAC DRM AAC/xHE-AAC transport backend;
- a bounded receiver pipeline that turns an acquired FAC epoch into SDC/MSC,
  maps the stable 24-bit service ID through FAC short IDs to the SDC stream,
  holds next-configuration SDC entities separately and promotes them atomically at the FAC boundary,
  delivers optional PCM frames through a callback, and reports decision-directed
  MSC MER over the exact set of extracted constellation cells;
- packet-mode data components: MOT slideshows and Broadcast Websites of the
  selected service, and the multiplex's Journaline news and programme guide
  (SPI). The MOT, Journaline and SPI decoders and the Viterbi decoder are shared
  with DAB, in `modemdr`;
- DRM energy dispersal and G8/G16 CRC primitives;
- the receiver acquisition/lock state machine; and
- bounded readers for stereo PCM16 IQ WAV and interleaved signed S32LE IQ.
