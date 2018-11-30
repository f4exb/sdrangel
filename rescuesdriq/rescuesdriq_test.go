package main

import (
	"testing"

	. "github.com/smartystreets/goconvey/convey"
)

func TestSpec(t *testing.T) {

	// Only pass t into top-level Convey calls
	Convey("Given a header structure", t, func() {
		var header HeaderStd
		header.SampleRate = 75000
		header.CenterFrequency = 435000000
		header.StartTimestamp = 1539083921
		header.SampleSize = 16
		header.Filler = 0

		crc32 := GetCRC(&header)

		Convey("The CRC32 value should be 2294957931", func() {
			So(crc32, ShouldEqual, 2294957931)
		})
	})
}
