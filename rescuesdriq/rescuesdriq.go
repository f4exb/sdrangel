package main

import (
	"bufio"
	"bytes"
	"encoding/binary"
	"flag"
	"fmt"
	"hash/crc32"
	"io"
	"os"
	"time"
)

type HeaderStd struct {
	SampleRate      uint32
	CenterFrequency uint64
	StartTimestamp  int64
	SampleSize      uint32
	Filler          uint32
	CRC32           uint32
}

func check(e error) {
	if e != nil {
		panic(e)
	}
}

func analyze(r *bufio.Reader) HeaderStd {
	headerbuf := make([]byte, 32) // This is a full header with CRC
	n, err := r.Read(headerbuf)
	if err != nil && err != io.EOF {
		panic(err)
	}
	if n != 32 {
		panic("Header too small")
	}

	var header HeaderStd
	headerr := bytes.NewReader(headerbuf)
	err = binary.Read(headerr, binary.LittleEndian, &header)
	check(err)

	return header
}

func writeHeader(writer *bufio.Writer, header *HeaderStd) {
	var bin_buf bytes.Buffer
	binary.Write(&bin_buf, binary.LittleEndian, header)
	noh, err := writer.Write(bin_buf.Bytes())
	check(err)
	fmt.Printf("Wrote %d bytes header\n", noh)
}

func setCRC(header *HeaderStd) {
	var bin_buf bytes.Buffer
	header.Filler = 0
	binary.Write(&bin_buf, binary.LittleEndian, header)
	header.CRC32 = crc32.ChecksumIEEE(bin_buf.Bytes()[0:28])
}

func GetCRC(header *HeaderStd) uint32 {
	var bin_buf bytes.Buffer
	header.Filler = 0
	binary.Write(&bin_buf, binary.LittleEndian, header)
	return crc32.ChecksumIEEE(bin_buf.Bytes()[0:28])
}

func printHeader(header *HeaderStd) {
	fmt.Println("Sample rate:", header.SampleRate)
	fmt.Println("Frequency  :", header.CenterFrequency)
	fmt.Println("Sample Size:", header.SampleSize)
	tm := time.Unix(header.StartTimestamp / 1000, header.StartTimestamp % 1000)
	fmt.Println("Start      :", tm)
	fmt.Println("CRC32      :", header.CRC32)
	fmt.Println("CRC32 OK   :", GetCRC(header))
}

func copyContent(reader *bufio.Reader, writer *bufio.Writer, blockSize uint) {
	p := make([]byte, blockSize*4096) // buffer in 4k multiples
	var sz int64 = 0

	for {
		n, err := reader.Read(p)

		if err == nil || err == io.EOF {
			writer.Write(p[0:n])
			sz += int64(n)
			if err == io.EOF {
				break
			}
		} else {
			fmt.Println("An error occurred during content copy. Aborting")
			break
		}

		fmt.Printf("Wrote %d bytes\r", sz)
	}

	fmt.Printf("Wrote %d bytes\r", sz)
}

func main() {
	inFileStr := flag.String("in", "foo", "input file")
	outFileStr := flag.String("out", "foo", "output file")
	sampleRate := flag.Uint("sr", 0, "Sample rate (S/s)")
	centerFreq := flag.Uint64("cf", 0, "Center frequency (Hz)")
	sampleSize := flag.Uint("sz", 16, "Sample size (16 or 24)")
	timeStr := flag.String("ts", "", "start time RFC3339 (ex: 2006-01-02T15:04:05Z)")
	timeNow := flag.Bool("now", false, "use now for start time")
	assumeMilliseconds := flag.Bool("msec", false, "assume timestamp read from input file is in milliseconds (by default seconds will be assumed)")
	blockSize := flag.Uint("bz", 1, "Copy block size in multiple of 4k")

	flag.Parse()
	flagSeen := make(map[string]bool)
	flag.Visit(func(f *flag.Flag) { flagSeen[f.Name] = true })

	if flagSeen["in"] {
		fmt.Println("input file :", *inFileStr)

		// open input file
		fi, err := os.Open(*inFileStr)
		check(err)
		// close fi on exit and check for its returned error
		defer func() {
			err := fi.Close()
			check(err)
		}()
		// make a read buffer
		reader := bufio.NewReader(fi)
		var headerOrigin HeaderStd = analyze(reader)

		if !*assumeMilliseconds {
			headerOrigin.StartTimestamp = headerOrigin.StartTimestamp * (int64(time.Second) / int64(time.Millisecond))
		}

		printHeader(&headerOrigin)

		if flagSeen["out"] {
			if flagSeen["sr"] {
				headerOrigin.SampleRate = uint32(*sampleRate)
			}
			if flagSeen["cf"] {
				headerOrigin.CenterFrequency = *centerFreq
			}
			if flagSeen["sz"] {
				if (*sampleSize == 16) || (*sampleSize == 24) {
					headerOrigin.SampleSize = uint32(*sampleSize)
				} else {
					fmt.Println("Incorrect sample size specified. Defaulting to 16")
					headerOrigin.SampleSize = 16
				}
			}
			if flagSeen["ts"] {
				t, err := time.Parse(time.RFC3339, *timeStr)
				if err == nil {
					headerOrigin.StartTimestamp = t.UnixNano() / int64(time.Millisecond)
				} else {
					fmt.Println("Incorrect time specified. Defaulting to now")
					headerOrigin.StartTimestamp = int64(time.Now().UnixNano() / int64(time.Millisecond))
				}
			} else if *timeNow {
				headerOrigin.StartTimestamp = int64(time.Now().UnixNano() / int64(time.Millisecond))
			}

			fmt.Println("\nHeader is now")
			printHeader(&headerOrigin)
			setCRC(&headerOrigin)
			fmt.Println("CRC32      :", headerOrigin.CRC32)
			fmt.Println("Output file:", *outFileStr)

			fo, err := os.Create(*outFileStr)
			check(err)

			defer func() {
				err := fo.Close()
				check(err)
			}()

			writer := bufio.NewWriter(fo)

			writeHeader(writer, &headerOrigin)
			copyContent(reader, writer, *blockSize)

			fmt.Println("\nCopy done")
			writer.Flush()
		}

	} else {
		fmt.Println("No input file given")
	}
}
