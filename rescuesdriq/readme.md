<h1>Repair or reformat record (.sdriq) files</h1>

<h2>Usage</h2>

This utility attempts to repair .sdriq files, that have their header corrupted, or were created using old versions of SDRangel. Since version 4.2.1 a CRC32 checksum is present and the file will not be played if the check of the header content against the CRC32 fails. In version 6.16.2 the timestamp resolution was increased from seconds to milliseconds since 1970-01-01 00:00:00.000.

The header is composed as follows:

  - Sample rate in S/s (4 bytes, 32 bits)
  - Center frequency in Hz (8 bytes, 64 bits)
  - Start time Unix timestamp epoch in milliseconds (8 bytes, 64 bits)
  - Sample size as 16 or 24 bits (4 bytes, 32 bits)
  - filler with all zeroes (4 bytes, 32 bits)
  - CRC32 (IEEE) of the 28 bytes above (4 bytes, 32 bits)

The header size is 32 bytes in total which is a multiple of 8 bytes thus occupies an integer number of samples whether in 16 or 24 bits mode. When migrating from a pre version 4.2.1 header you may crunch a very small amount of samples.

You can replace values in the header with the following options:

  - -sr uint
      Sample rate (S/s)
  - -cf uint
      Center frequency (Hz)
  - -ts string
      start time RFC3339 (ex: 2006-01-02T15:04:05Z)
  - -now
      use now for start time
  - -msec
      assume timestamp read from input file is in milliseconds (by default seconds will be assumed)
  - -sz uint
      Sample size (16 or 24) (default 16)

You need to specify an input file. If no output file is specified the current header values are printed to the console and the program exits:

  - -in string
      input file (default "foo")

To convert to a new file you need to specify the output file:

  - -out string
      output file (default "foo")

You can specify a block size in multiples of 4k for the copy. Large blocks will yield a faster copy but a larger output file. With the default of 1 (4k) the copy does not take much time anyway:

  - -bz uint
      Copy block size in multiple of 4k (default 1)

<h2>Build</h2>

The program is written in go and is provided only in source code form. Compiling it is very easy:

<h3>Install go</h3>

You will usually find a `golang` package in your distribution. For example in Ubuntu or Debian you can install it with `sudo apt-get install golang`. You can find binary distributions for many systems at the [Go download site](https://golang.org/dl/)

<h3>Build the program</h3>

In this directory just do `go build`

<h3>Unit testing</h3>

Unit test (very simple) is located in `rescuesdriq_test.go`. It uses the [Go Convey](https://github.com/smartystreets/goconvey) framework. You should first install it with:
`go get github.com/smartystreets/goconvey`

You can run unit test from command line with: `go test`

Or with the Go Convey server that you start from this directory with: `$GOPATH/bin/goconvey` where `$GOPATH` is the path to your go installation.
