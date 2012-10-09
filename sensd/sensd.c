/*
  
 tty_talker over RX/TX serial port output on stdout

 Robert Olsson  <robert@herjulf.se>  most code taken from:


 file is part of the minicom communications package,
 *		Copyright 1991-1995 Miquel van Smoorenburg.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.

 and

 * Based on.... serial port tester
 * Doug Hughes - Auburn University College of Engineering
 * 9600 baud by default, settable via -19200 or -9600 flags
 * first non-baud argument is tty (e.g. /dev/term/a)
 * second argument is file name (e.g. /etc/hosts)
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termio.h>
#include <time.h>
#include <sys/fcntl.h>
#include <fcntl.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <signal.h>
#include "devtag-allinone.h"

#define VERSION "2.2 120625"
#define END_OF_FILE 26
#define CTRLD  4
#define P_LOCK "/var/lock"

char lockfile[128]; /* UUCP lock file of terminal */
char dial_tty[128];
char username[16];
int pid;
int retry = 6;


void usage_tty_talk(void)
{
  printf("\ntty_talk sends a string to terminal/USB and reads the response\n");
  printf("Version %s\n\n", VERSION);
  printf("tty_talk [-BAUDRATE] [-loop] [-date] [-utc] DEV string\n");
  printf(" -loop forces repeatable reeds\n");
  printf(" -date print cur. date/time on each line\n");
  printf(" -utime time in unix format\n");
  printf(" -utc time in UTC\n");
  printf(" -b run in background\n");
  printf(" -fFILENAME redirect output to FILENAME\n");
  printf(" Valid baudrates 4800, 9600 (Default), 19200, 38400 bps\n");
  printf(" Example: tty_talk -38400  /dev/ttyUSB0 q1\n\n");
  exit(-1);
}

void usage_sensd(void)
{
  printf("\nVersion %s\n", VERSION);
  printf("\nsensd daemon reads sensors data from serial/USB and writes to file\n");
  printf("Usage: sensd [-utc] [-ffile] [-Rpath] DEV\n");
  printf(" -utc time in UTC\n");
  printf(" -ffile data file. Default is /var/log/sensors.dat\n");
  printf(" -Rpath Path for reports. One dir per sensor. One file per value.\n");
  printf("Example: sensd  /dev/ttyUSB0\n");
  exit(-1);
}

/* Options*/
int loop, date, utime, utc, add_preamble, background;
long baud;

#define INV_TTY_TALK (1<<0)
#define INV_SENSD    (1<<1)

unsigned int invokation; 

void usage(void)
{
  if(invokation & INV_TTY_TALK) 
    usage_tty_talk();

  if(invokation & INV_SENSD) 
    usage_sensd();

  exit(-1);
}

unsigned char preamble[4] = {0xAA, 0x00, 0xFF, 0x01};

/*
 * Find out name to use for lockfile when locking tty.
 */

char *mbasename(char *s, char *res, int reslen)
{
  char *p;
  
  if (strncmp(s, "/dev/", 5) == 0) {
    /* In /dev */
    strncpy(res, s + 5, reslen - 1);
    res[reslen-1] = 0;
    for (p = res; *p; p++)
      if (*p == '/')
        *p = '_';
  } else {
    /* Outside of /dev. Do something sensible. */
    if ((p = strrchr(s, '/')) == NULL)
      p = s;
    else
      p++;
    strncpy(res, p, reslen - 1);
    res[reslen-1] = 0;
  }
  return res;
}

int lockfile_create(void)
{
  int fd, n;
  char buf[81];

  n = umask(022);
  /* Create lockfile compatible with UUCP-1.2  and minicom */
  if ((fd = open(lockfile, O_WRONLY | O_CREAT | O_EXCL, 0666)) < 0) {
    return 0;
  } else {
    snprintf(buf, sizeof(buf),  "%05d tty_talk %.20s\n", (int) getpid(), 
	     username);

    write(fd, buf, strlen(buf));
    close(fd);
  }
  umask(n);
  return 1;
}

void lockfile_remove(void)
{
  if (lockfile[0])
    unlink(lockfile);
}

int have_lock_dir(void)
{
 struct stat stt;
  char buf[128];

  if ( stat(P_LOCK, &stt) == 0) {

    snprintf(lockfile, sizeof(lockfile),
                       "%s/LCK..%s",
                       P_LOCK, mbasename(dial_tty, buf, sizeof(buf)));
  }
  else {
	  fprintf(stderr, "Lock directory %s does not exist\n", P_LOCK);
	exit(-1);
  }
  return 1;
}

int get_lock()
{
  char buf[128];
  int fd, n = 0;

  have_lock_dir();

  if((fd = open(lockfile, O_RDONLY)) >= 0) {
    n = read(fd, buf, 127);
    close(fd);
    if (n > 0) {
      pid = -1;
      if (n == 4)
        /* Kermit-style lockfile. */
        pid = *(int *)buf;
      else {
        /* Ascii lockfile. */
        buf[n] = 0;
        sscanf(buf, "%d", &pid);
      }
      if (pid > 0 && kill((pid_t)pid, 0) < 0 &&
          errno == ESRCH) {
	      fprintf(stderr, "Lockfile is stale. Overriding it..\n");
        sleep(1);
        unlink(lockfile);
      } else
        n = 0;
    }
    if (n == 0) {
      if(retry == 1) /* Last retry */
	      fprintf(stderr, "Device %s is locked.\n", dial_tty);
      return 0;
    }
  }
  lockfile_create();
  return 1;
}


void print_date(char *datebuf)
{
  time_t raw_time;
  struct tm *tp;
  char buf[256];

  *datebuf = 0;

  time ( &raw_time );

  if(utc)
    tp = gmtime ( &raw_time );
  else
    tp = localtime ( &raw_time );

  if(date) {
	  sprintf(buf, "%04d-%02d-%02d %2d:%02d:%02d ",
		  tp->tm_year+1900, tp->tm_mon+1, 
		  tp->tm_mday, tp->tm_hour, 
		  tp->tm_min, tp->tm_sec);
	  strcat(datebuf, buf);
  }

  if(utime) {
	  sprintf(buf, "UT=%ld ", raw_time);
	  strcat(datebuf, buf);
  }
}

int report(const char *buf, const char *reportpath)
{
	struct stat statb;
	char *s = strdup(buf);
	char *id = NULL;
	char *val, *n;
	char fn[512];
	char tmpfn[512];

	while(*s) {
		while(*s && *s == ' ') s++;

		n=s; while(*n && *n != ' ') n++;
		if(*n) { *n = 0; n++; }
		
		val = strchr(s, '=');
		if(val) {
			*val++ = 0;
			if(!id) {
				if((strcmp(s, "ID")==0) && *val) {
					id = val;
					snprintf(fn, sizeof(fn), "%s/%s", reportpath, id);
					if(stat(fn, &statb)) {
						mkdir(fn, 0755);
					}
				}
			}
			if(id && *val) {
				int fd;
				snprintf(fn, sizeof(fn), "%s/%s/%s", reportpath, id, s);
				snprintf(tmpfn, sizeof(tmpfn), "%s/%s/%s.tmp", reportpath, id, s);
				fd = open(tmpfn, O_WRONLY|O_TRUNC|O_CREAT, 0644);
				if(fd >= 0) {
					write(fd, val, strlen(val));
					close(fd);
					rename(tmpfn, fn);
				}
			}
		}
		s = n;
	}
	return 0;
}

int main(int ac, char *av[]) 
{
	struct termios tp, old;
	int fd;
	char io[BUFSIZ];
	char buf[2*BUFSIZ];
	char *filename = NULL;
	char *reportpath = NULL;
	int res;
	int i, done, len, idx;
	char *prog = basename (av[0]);

	if (strcmp(prog, "tty_talk") == 0)  {
	  invokation = INV_TTY_TALK;
	  baud = B9600;
	  background = 0;
	  loop = 0;
	  date = 0;
	  utc = 0;
	  utime = 0;
	}

	else if (strcmp(prog, "sensd") == 0) {
	  invokation = INV_SENSD;
	  baud = B38400;
	  background = 1;
	  loop = 1;
	  date = 1;
	  utime = 1;
	  utc = 0;
	  filename = "/var/log/sensors.dat";
	}

	if(ac == 1) 
	  usage();

	for(i = 1; (i < ac) && (av[i][0] == '-'); i++)
	  {
	    if (strcmp(av[i], "-300") == 0) 
	      baud = B300;

	    else if (strcmp(av[i], "-600") == 0) 
	      baud = B600;

	    else if (strcmp(av[i], "-1200") == 0) 
	      baud = B1200;

	    else if (strcmp(av[i], "-2400") == 0) 
	      baud = B2400;

	    else if (strcmp(av[i], "-4800") == 0) 
	      baud = B4800;

	    else if (strcmp(av[i], "-9600") == 0)
	      baud = B9600;

	    else if (strcmp(av[i], "-19200") == 0)
	      baud = B19200;

	    else if (strcmp(av[i], "-38400") == 0)
	      baud = B38400;

	    else if (strcmp(av[i], "-loop") == 0) 
	      loop = 1;

	    else if (strcmp(av[i], "-date") == 0) 
	      date = 1;

	    else if (strcmp(av[i], "-utime") == 0) 
	      utime = 1;

	    else if (strcmp(av[i], "-utc") == 0) 
	      utc = 1;

	    else if (strcmp(av[i], "-preamble") == 0) 
	      add_preamble = 1;

	    else if (strcmp(av[i], "-b") == 0) 
	      background = 1;

	    else if (strncmp(av[i], "-f", 2) == 0) 
	      filename = av[i]+2;

	    else if (strncmp(av[i], "-R", 2) == 0) {
	      reportpath = av[i]+2;
	      if(!*reportpath) reportpath = "/var/lib/sensd";
	    }

	    else
	      usage();

	  }

	if(reportpath) {
		struct stat statb;
		if(stat(reportpath, &statb)) {
			fprintf(stderr, "Failed to open '%s'\n", reportpath);
			exit(2);
		}
	}

	if(filename) {
		int ofd;
		ofd = open(filename, O_CREAT|O_RDWR|O_APPEND, 0644);
		if(ofd >= 0) {
			dup2(ofd, 1);
			close(ofd);
		} else {
			fprintf(stderr, "Failed to open '%s'\n", filename);
			exit(2);
		}
	}

	strncpy(dial_tty, devtag_get(av[i]), sizeof(dial_tty));

	while (! get_lock()) {
	    if(--retry == 0)
	      exit(-1);
	    sleep(1);
	}

	if ((fd = open(devtag_get(av[i]), O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0) {
	  perror("bad terminal device, try another");
	  exit(-1);
	}
	
	fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, O_RDWR);

	if (tcgetattr(fd, &tp) < 0) {
		perror("Couldn't get term attributes");
		exit(-1);
	}
	old = tp;

/*
SANE is a composite flag that sets the following parameters from termio(M):

CREAD BRKINT IGNPAR ICRNL IXON ISIG ICANON
ECHO ECHOK OPOST ONLCR

SANE also clears the following modes:

CLOCAL
IGNBRK PARMRK INPCK INLCR IUCLC IXOFF
XCASE ECHOE ECHONL NOFLSH
OLCUC OCRNL ONOCR ONLRET OFILL OFDEL NLDLY CRDLY
TABDLY BSDLY VTDLY FFDLY 

*/
	/* 8 bits + baud rate + local control */
	tp.c_cflag = baud | CS8 | CLOCAL | CREAD;
	tp.c_oflag = 0; /* Raw Input */
	tp.c_lflag = 0; /* No conoical */
	tp.c_oflag &= ~(OLCUC|OCRNL|ONOCR|ONLRET|OFILL|OFDEL|NLDLY|CRDLY);


	/* ignore CR, ignore parity */
	tp.c_iflag = ~(IGNBRK|PARMRK|INPCK|INLCR|IUCLC|IXOFF) |
	  BRKINT|IGNPAR|ICRNL|IXON|ISIG|ICANON;

	tp.c_lflag &= ~(ECHO  | ECHONL);

	tcflush(fd, TCIFLUSH);

	/* set output and input baud rates */

	cfsetospeed(&tp, baud);
	cfsetispeed(&tp, baud);

	if (tcsetattr(fd, TCSANOW, &tp) < 0) {
		perror("Couldn't set term attributes");
		goto error;
	}

	/* Term ok deal w. text to send */
	
	idx = 0;

	/* Only tty_talk sends commnad */
	if( invokation & INV_TTY_TALK ) {
	  
	  i++;
	  for( ; i < ac; i++) {
	    len = strlen(av[i]);
	    strncpy(&io[idx], av[i], len);
	    idx += len;
	    io[idx++] = '\r';
	  }
	  
	  res = write(fd, io, idx);
	  if(res < 0 ) {
	    perror("write faild");
	    goto error;
	  }
	  
	  if(add_preamble) {
	    len = sizeof(preamble);
	    strncpy(&io[idx], preamble, len);
	    idx += len;
	  }
	  
	  i++;
	  for( ; i < ac; i++) {
	    len = strlen(av[i]);
	    strncpy(&io[idx], av[i], len);
	    idx += len;
	    io[idx++] = '\r';
	  }
	} 
	
	if(background) {
	  int i, lfp;
	  char str[10];
	  if(getppid() == 1) 
	    return; /* Already a daemon */

	  i = fork();

	  if (i < 0) 
	    exit(1); /* error */

	  if (i > 0) 
	    _exit(0); /* parent exits */

	  /* child */
	  
	  setsid(); /* obtain a new process group */
	  for (i = getdtablesize(); i >= 0; --i) {
		  if(i == fd) continue;
		  if(i == 1) continue;
	    close(i); /* close all descriptors */
	  }

	  i = open("/dev/null",O_RDWR); dup(i); dup(i); /* handle standard I/O */
	  umask(027); /* set newly created file permissions */
	  chdir("/"); /* change running directory */
#if 0
	  lfp = open(LOCK_FILE,O_RDWR|O_CREAT,0640);
	  if (lfp < 0) exit(1); /* can not open */

	  if (lockf(lfp, F_TLOCK,0) <0 ) 
	    exit(0); /* can not lock */
	  /* first instance continues */
	  sprintf(str,"%d\n",getpid());
	  write(lfp,str,strlen(str)); /* record pid to lockfile */
	  signal(SIGCHLD,SIG_IGN); /* ignore child */
	  signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
	  signal(SIGTTOU,SIG_IGN);
	  signal(SIGTTIN,SIG_IGN);
	  signal(SIGHUP,signal_handler); /* catch hangup signal */
	  signal(SIGTERM,signal_handler); /* catch kill signal */
#endif
	}

	done = 0;

	int j = 0;

	if(reportpath) umask(0);
	
	while (!done && (res = read(fd, io, BUFSIZ)) > 0)  {
	    int i;
	    char outbuf[512];
	    
	    for(i=0; !done && i < res; i++, j++)  {

	      if(io[i] == END_OF_FILE) {
		      outbuf[0] = 0;
		      if(date || utime)
			      print_date(outbuf);
		      buf[j] = 0;
		      strcat(outbuf, buf);
		      write(1, outbuf, strlen(outbuf));
		      if(reportpath) report(buf, reportpath);
		      
		      j = -1;
		      
		      if(!loop) 
			      done = 1;
	      }
	      else  
		      buf[j] = io[i];
	    }
	}

	if (tcsetattr(fd, TCSANOW, &old) < 0) {
		perror("Couldn't restore term attributes");
		exit(-1);
	}
	lockfile_remove();
	exit(0);
error:
	if (tcsetattr(fd, TCSANOW, &old) < 0) {
		perror("Couldn't restore term attributes");
	}
	exit(-1);
}
