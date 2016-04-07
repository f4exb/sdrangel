#include <termios.h>
#include "dsd.h"

void
openSerial (dsd_opts * opts, dsd_state * state)
{

  struct termios tty;
  speed_t baud;

  printf ("Opening serial port %s and setting baud to %i\n", opts->serial_dev, opts->serial_baud);
  opts->serial_fd = open (opts->serial_dev, O_WRONLY);
  if (opts->serial_fd == -1)
    {
      printf ("Error, couldn't open %s\n", opts->serial_dev);
      exit (1);
    }

  tty.c_cflag = 0;

  baud = B115200;
  switch (opts->serial_baud)
    {
    case 1200:
      baud = B1200;
    case 2400:
      baud = B2400;
    case 4800:
      baud = B4800;
    case 9600:
      baud = B9600;
      break;
    case 19200:
      baud = B19200;
      break;
    case 38400:
      baud = B38400;
      break;
    case 57600:
      baud = B57600;
      break;
    case 115200:
      baud = B115200;
      break;
    case 230400:
      baud = B230400;
      break;
    }
  if (opts->serial_baud > 0)
    {
      cfsetospeed (&tty, baud);
      cfsetispeed (&tty, baud);
    }

  tty.c_cflag |= (tty.c_cflag & ~CSIZE) | CS8;
  tty.c_iflag = IGNBRK;
  tty.c_lflag = 0;
  tty.c_oflag = 0;
  tty.c_cflag &= ~CRTSCTS;
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);
  tty.c_cflag &= ~(PARENB | PARODD);
  tty.c_cflag &= ~CSTOPB;
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 5;

  tcsetattr (opts->serial_fd, TCSANOW, &tty);

}

void
resumeScan (dsd_opts * opts, dsd_state * state)
{

  char cmd[16];
  ssize_t result;

  if (opts->serial_fd > 0)
    {
      sprintf (cmd, "\rKEY00\r");
      result = write (opts->serial_fd, cmd, 7);
      cmd[0] = 2;
      cmd[1] = 75;
      cmd[2] = 15;
      cmd[3] = 3;
      cmd[4] = 93;
      cmd[5] = 0;
      result = write (opts->serial_fd, cmd, 5);
      state->numtdulc = 0;
    }
}
