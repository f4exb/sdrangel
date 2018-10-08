package main

import (
	"flag"
	"fmt"
    "bufio"
    "io"
    "os"
    "bytes"
    "encoding/binary"
    "time"
)

	
type HeaderStd struct {
    SampleRate      uint32
    CenterFrequency uint64
    StartTimestamp  int64
    SampleSize      uint32
    _               uint32
    CRC32           uint32
}

func analyze(fileName string) HeaderStd {
	fmt.Println("input file:", fileName)
	
    // open input file
    fi, err := os.Open(fileName)
    if err != nil {
        panic(err)
    }
    // close fi on exit and check for its returned error
    defer func() {
        if err := fi.Close(); err != nil {
            panic(err)
        }
    }()
    // make a read buffer
    r := bufio.NewReader(fi)
    
    headerbuf := make([]byte, 32) // This is a full header with CRC
	n, err := r.Read(headerbuf)
	if err != nil && err != io.EOF {
		panic(err)
	}
	if (n != 32) {
		panic("Header too small")
	}
		
	var header HeaderStd
	headerr := bytes.NewReader(headerbuf)
	err = binary.Read(headerr, binary.LittleEndian, &header)
    if err != nil {
        panic(err)
    }
    
    fmt.Println("Sample rate:", header.SampleRate)
    fmt.Println("Frequency  :", header.CenterFrequency)
    fmt.Println("Sample Size:", header.SampleSize)
    tm := time.Unix(header.StartTimestamp, 0)
    fmt.Println("Start      :", tm)
    
    return header
}

func main() {
	wordPtr := flag.String("in", "foo", "input file")
	flag.Parse()
	flagSeen := make(map[string]bool)
	flag.Visit(func(f *flag.Flag) { flagSeen[f.Name] = true })
	
	if flagSeen["in"] {
		analyze(*wordPtr)
	} else {
		fmt.Println("No input file given")
	}
}
