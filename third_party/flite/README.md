CMU Flite English frontend
=========================

Vendored from https://github.com/festvox/flite at 6c9f20dc915b17f5619340069889db0aa007fcdc.
Original C sources are unmodified. Only the core, US English models,
CMU lexicon and learned letter-to-sound rules are used. Audio devices, network
sockets and acoustic voice recordings are not used. sources.mk selects the
upstream library source lists with the none audio/mmap backends.

Pronunciations, durations and intonation drive Pink Trombone; Flite does not
generate the output waveform. See COPYING for upstream notices.
