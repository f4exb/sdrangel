APRS images are from: https://github.com/hessu/aprs-symbols/tree/master/png

To split in to individual files, using ImageMagick:

  convert aprs-symbols-24-0.png -crop 24x24 aprs-symbols-24-0-%02d.png
  convert aprs-symbols-24-1.png -crop 24x24 aprs-symbols-24-1-%02d.png
