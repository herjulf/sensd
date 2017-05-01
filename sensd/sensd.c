/*
 Copyright GPL by
 Robert Olsson  <robert@Radio-Sensors.COM>  

 Code used from:

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
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "devtag-allinone.h"

#define VERSION "6.2 2017-05-01"
#define END_OF_FILE 26
#define CTRLD  4
#define P_LOCK "/var/lock"
#define LOGFILE "/var/log/sensors.dat"

char lockfile[128]; /* UUCP lock file of terminal */
char dial_tty[128];
char username[16];
int pid;
int retry = 6;

float *gps_lon, *gps_lat;
char *domain = NULL;

int debug = 0;

#define BUFSIZE 1024
#define SERVER_PORT  1234
#define SEND_PORT  SERVER_PORT  

#define TRUE             1
#define FALSE            0


void usage(void)
{
  printf("\nVersion %s\n", VERSION);
  printf("\nsensd: A WSN gateway, proxy and hub\n");
  printf("Usage: sensd [-send addr] [-send_port port] [-p port] [-report] [-domain dmn] [-utc] [-f file] [-R path] [-g gpsdev] [-LATX.xx] [-LONY.yy] [-ALTZ.zz] [ -D dev]\n");

  printf(" -D dev       Serial or USB dev. Typically /dev/ttyUSB0\n");
  printf(" -f file      Local logfile. Default is %s\n", LOGFILE);
  printf(" -report      Enable TCP reports\n");
  printf(" -p port      TCP server port. Default %d\n", SERVER_PORT);
  printf(" -send addr   Send data to a proxy\n");
  printf(" -receive     Receive. Be a proxy\n");
  printf(" -receive_time_local   Use local time\n");
  printf(" -send_port port  Set proxyport. Default %d\n", SEND_PORT);
  printf(" -utc         Time in UTC\n");
  printf(" -R path      Path for reports. One dir per sensor. One file per value\n");
  printf(" -g gpsdev    Device for gps\n");
  printf(" -infile      Read data from a file (named pipe)\n");
  printf(" -debug       Debug on stdout\n");
  printf(" -domain dmn  Add domain tag\n");
  printf("\nExample 1: Local file logging\n sensd  -D /dev/ttyUSB0\n");
  printf("Example 2: TCP listers. No file logging\n sensd -report -f /dev/null -D /dev/ttyUSB0\n");
  printf("Example 3: Send to proxy\n sensd -report -send addr -f /dev/null -D /dev/ttyUSB0\n");
  printf("Example 4: Be a proxy. TCP listers. No file logging\n sensd -report -receive -f /dev/null\n");
  printf("Example 5: Use a named pipe\n sensd -report -f /dev/null -infile mkfifo_file\n");
  printf("Example 6: Use LAT/LON\n sensd -report -f /dev/null -LAT -2.10 -LON 12.10 -D /dev/ttyUSB0\n");
  printf("Example 7: Use LAT/LON\n sensd -report -f /dev/null -LAT -2.10 -LON 12.10 -ALT 10.1 -D /dev/ttyUSB0\n");
  printf("Example 8: Use a gps device\n sensd -report -f /dev/null -g /dev/ttyUSB1 -D /dev/ttyUSB0\n");

  exit(-1);
}


/* Function declarations */
int gps_read(int fd, float *lon, float *lat);

/* Options*/
int cmd, date, utime, utc, background;
long baud;

#define GPS_MISS -999
double lon = GPS_MISS, lat = GPS_MISS;
#define ALT_MISS -999
double alt = ALT_MISS;

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
    snprintf(buf, sizeof(buf),  "%05d sensd %.20s\n", (int) getpid(), 
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

void print_report_time(char *ibuf)
{
  time_t raw_time;
  struct tm *tp;
  char buf[256];

  time ( &raw_time );

  if(utc)
    tp = gmtime ( &raw_time );
  else
    tp = localtime ( &raw_time );

  if(date) {
	  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d TZ=%s ",
		  tp->tm_year+1900, tp->tm_mon+1, 
		  tp->tm_mday, tp->tm_hour, 
		  tp->tm_min, tp->tm_sec, tp->tm_zone);
  }
  memcpy(ibuf, buf, strlen(buf));
}

void print_report_header(char *gpsdev, char *datebuf)
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
	  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d TZ=%s ",
		  tp->tm_year+1900, tp->tm_mon+1, 
		  tp->tm_mday, tp->tm_hour, 
		  tp->tm_min, tp->tm_sec, tp->tm_zone);
	  strcat(datebuf, buf);
  }

  if(utime) {
	  sprintf(buf, "UT=%ld ", raw_time);
	  strcat(datebuf, buf);
  }

  if(gpsdev) {
    sprintf(buf, "GWGPS_LAT=%-3.5f ", *gps_lat);
    strcat(datebuf, buf);

    sprintf(buf, "GWGPS_LON=%-3.5f ", *gps_lon);
    strcat(datebuf, buf);
  }

  if(lat > GPS_MISS) {
    sprintf(buf, "GW_LAT=%-3.5f ", lat);
    strcat(datebuf, buf);
  }

  if(lon > GPS_MISS) {
    sprintf(buf, "GW_LON=%-3.5f ", lon);
    strcat(datebuf, buf);
  }

  if(alt > ALT_MISS) {
    sprintf(buf, "GW_ALT=%-4.2f ", alt);
    strcat(datebuf, buf);
  }

  if(domain) {
    sprintf(buf, "DOMAIN=%s ", domain);
    strcat(datebuf, buf);
  }
}

int do_report(const char *buf, const char *reportpath)
{
	struct stat statb;
	char *s = strdup(buf);
	char *id = NULL;
	char *val, *n;
	char fn[512];
	char tmpfn[512];

	if(debug)
	  printf("do_report \n");

	while(*s) {
		while(*s && *s == ' ') s++;

		n=s; while(*n && *n != ' ') n++;
		if(*n) { *n = 0; n++; }
		
		val = strchr(s, '=');
		if(val) {
			*val++ = 0;
			if(*s == '[') s++;
			if(!*n) {
				if(*val && val[strlen(val)-1] == '\n') val[strlen(val)-1] = 0;
				if(*val && val[strlen(val)-1] == ']') val[strlen(val)-1] = 0;
			}
			/*
			  We have 3 ID's for node. We select in order. 
			  E64 -  EUI64 
			  ID  -  DALLAS
			  TXT -  Fri-text format
			*/

			if(!id) {
				if((strcmp(s, "TXT")==0) && *val) {
					id = val;
					snprintf(fn, sizeof(fn), "%s/%s", reportpath, id);
					if(stat(fn, &statb)) {
						mkdir(fn, 0755);
					}
				}
			}
			if(!id) {
				if((strcmp(s, "E64")==0) && *val) {
					id = val;
					snprintf(fn, sizeof(fn), "%s/%s", reportpath, id);
					if(stat(fn, &statb)) {
						mkdir(fn, 0755);
					}
				}
			}
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

int set_term(int fd, int baud, struct termios tp)
{

	fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, O_RDWR);

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
		return(-1);
	}
	return 1;
}

int connect_remote(char *host, int port)
{
    struct sockaddr_in6 addr;
    struct hostent *he;
    int len, s, x, on = 1;

    he = gethostbyname(host);
    if (!he)
	return (-2);

    len = sizeof(addr);
    s = socket(AF_INET6, SOCK_STREAM, 0);
    if (s < 0)
	return s;

    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, 4);

    len = sizeof(addr);
    memset(&addr, '\0', len);
    addr.sin6_family = AF_INET6;
    memcpy(&addr.sin6_addr, he->h_addr, he->h_length);
    addr.sin6_port = htons(port);
    x = connect(s, (struct sockaddr *) &addr, len);
    if (x < 0) {
	close(s);
	return x;
    }
    //set_nonblock(s);
    return s;
}



int lissen(struct sockaddr_in6 addr, int port)
{
	int s = socket(AF_INET6, SOCK_STREAM, 0);
	int rc, on = 1;
	const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;

	if (s < 0)  { 
		perror("socket() failed");
		exit(-1);
	}

	/* Allow socket descriptor to be reuseable */
	rc = setsockopt(s, SOL_SOCKET,  SO_REUSEADDR,
			(char *)&on, sizeof(on));
	if (rc < 0)   {
		perror("setsockopt() failed");
		close(s);
		exit(-1);
	}
	  
	/* Set socket to be nonblocking. All of the sockets for    
	   the incoming connections will also be nonblocking since  
	   they will inherit that state from the listening socket.   */
	  
	rc = ioctl(s, FIONBIO, (char *)&on);
	if (rc < 0)  {
		perror("ioctl() failed");
		close(s);
		exit(-1);
	}
	  
	memset(&addr, 0, sizeof(addr));
	addr.sin6_family      = AF_INET6;
	memcpy((void*) &in6addr_any, &addr.sin6_addr.s6_addr, sizeof(in6addr_any));

	addr.sin6_port        = htons(port);
	  
	rc = bind(s, (struct sockaddr *)&addr, sizeof(addr));
	  
	if (rc < 0)   {
		perror("bind() failed");
		close(s);
		exit(-1);
	}

	/* Set the listen back log  */

	rc = listen(s, 32);
	  
	if (rc < 0)   {
		perror("listen() failed");
		close(s);
		exit(-1);
	}
	return s;
}

int main(int ac, char *av[]) 
{
  struct termios tp, tp_usb_old, tp_gps_old;
	int usb_fd;
	int file_fd;
	int gps_fd;
	int send_host_sd = -1;
	int receive = 0;
	int receive_time_local = 0;
	char io[BUFSIZE];
	char buf[BUFSIZE];
	char *filename = NULL;
	char *gpsdev = NULL;
	char *reportpath = NULL;
	unsigned short filedev = 0;
	int res;
	int i, len;
	char *serialdev = NULL;
	int    rc;
	int    listen_sd = -1, new_sd = -1;
	int    compress_array = FALSE;
	int    close_conn;
	char   buffer[BUFSIZE];
	char   *send_host = NULL;
	struct sockaddr_in6   addr;
	int    timeout;
	struct pollfd fds[200];
	int    nfds = 2, current_size = 0, j;
	int    send_2_listners;
	unsigned short port = SERVER_PORT;
	int send_port = SEND_PORT;
	unsigned short report = 0;
	struct sockaddr_in6 saddr;
	socklen_t saddr_len = sizeof(saddr);

	baud = B38400;
	background = 1;
	date = 1;
	utime = 1;
	utc = 0;
	filename = LOGFILE;

	if(ac == 1) 
	  usage();

	for(i = 1; (i < ac) && (av[i][0] == '-'); i++)  {
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

	    else if (strcmp(av[i], "-utime") == 0) 
	      utime = 1;

	    else if (strcmp(av[i], "-utc") == 0) 
	      utc = 1;

	    else if (strcmp(av[i], "-b") == 0) 
	      background = 1;

	    else if (strncmp(av[i], "-f", 2) == 0) 
	      filename = av[++i];

	    else if (strncmp(av[i], "-g", 2) == 0) 
	      gpsdev = av[++i];

	    else if (strncmp(av[i], "-R", 2) == 0) {
	      reportpath = av[++i];
	      if(!*reportpath) reportpath = "/var/lib/sensd";
	    }

	    else if (strncmp(av[i], "-debug", 6) == 0) {
	      debug = 1;
	    }

	    else if (strncmp(av[i], "-domain", 7) == 0) {
	      domain = av[++i];
	    }
	    
	    else if (strncmp(av[i], "-send_port", 9) == 0) {
	      send_port = atoi(av[++i]);
	    }

	    else if (strcmp(av[i], "-send") == 0) {
	      send_host = av[++i];
	    }

	    else if (strncmp(av[i], "-p", 2) == 0) {
	      port = atoi(av[++i]);
	    }

	    else if (strncmp(av[i], "-D", 2) == 0) {
	      serialdev = av[++i];
	    }

	    else if (strncmp(av[i], "-receive_time_local", 10) == 0) 
	      receive_time_local = 1;

	    else if (strncmp(av[i], "-receive", 4) == 0) 
	      receive = 1;

	    else if (strcmp(av[i], "-report") == 0) 
	      report = 1;

	    else if (strcmp(av[i], "-infile") == 0) 
	      filedev = 1;

	    else if (strncmp(av[i], "-LON", 4) == 0)
	      lon = strtod(av[++i], NULL);

	    else if (strncmp(av[i], "-LAT", 4) == 0) 
	      lat = strtod(av[++i], NULL);

	    else if (strncmp(av[i], "-ALT", 4) == 0) 
	      alt = strtod(av[++i], NULL);

	    else
	      usage();

	  }

	if(debug) {
	  printf("send_port=%d\n", send_port);
	  printf("send_host=%s\n", send_host);
	  printf("domain=%s\n", domain);
	  printf("receive=%d\n", receive);
	  printf("receive_time_local=%d\n", receive_time_local);
	  printf("reportpath=%s\n", reportpath);
	}

	if(reportpath) {
		struct stat statb;
		if(stat(reportpath, &statb)) {
			fprintf(stderr, "Failed to open '%s'\n", reportpath);
			exit(2);
		}
	}

	if(filename) {
		file_fd = open(filename, O_CREAT|O_RDWR|O_APPEND, 0644);
		if(file_fd < 0) {
			fprintf(stderr, "Failed to open '%s'\n", filename);
			exit(2);
		}
	}

	if(gpsdev) {
	  gps_fd = open(devtag_get(gpsdev), O_RDWR | O_NOCTTY | O_NONBLOCK);
	  if(gps_fd < 0) {
	    fprintf(stderr, "Failed to open '%s'\n", gpsdev);
	    exit(2);
	  }
	  
	  if (tcgetattr(gps_fd, &tp) < 0) {
		perror("Couldn't get term attributes");
		exit(-1);
	  }
	  tp_gps_old = tp;

	  if( set_term(gps_fd, B4800, tp) != 1)
	    exit(-1);
	}
	  
	if(serialdev) {
	  strncpy(dial_tty, devtag_get(serialdev), sizeof(dial_tty));
	  while (! get_lock()) {
	    if(--retry == 0)
	      exit(-1);
	    sleep(1);
	  }

	  if ((usb_fd = open(devtag_get(serialdev), O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0) {
	    perror("bad terminal device, try another");
	    exit(-1);
	  }
	  
	  if (tcgetattr(usb_fd, &tp) < 0) {
	    perror("Couldn't get term attributes");
	    exit(-1);
	  }
	  tp_usb_old = tp;
	  
	  if( set_term(usb_fd, baud, tp) != 1)
	    exit(-1);
	}

	if(filedev) {
		usb_fd = open(av[i],O_RDONLY);
		if(usb_fd < 0) {
			fprintf(stderr, "Failed to open filedev '%s'\n", filename);
			exit(2);
		}
	} 

	if(gpsdev) {
	  gps_lat = mmap(NULL, sizeof(gps_lat), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	  if( gps_lat == (void *) -1) {
	    perror("mmap");
	    exit(-1);
	  }
	  gps_lon = mmap(NULL, sizeof(gps_lon), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	  if( gps_lon == (void *) -1) {
	    perror("mmap");
	    exit(-1);
	  }
	}


	/* Term ok deal w. text to send */
	
	if(background) {
	  int i;
	  if(getppid() == 1) 
	    return 0; /* Already a daemon */

	  i = fork();

	  if (i < 0) 
	    exit(1); /* error */

	  if (i > 0) 
	    _exit(0); /* parent exits */

	  /* gps child */

	  if( gpsdev && fork() == 0)  {
	    int j;
	    setsid(); /* obtain a new process group */
	    for (j = getdtablesize(); j >= 0; --j) {
	      if(j == gps_fd) continue;
	    }
	    j = open("/dev/null",O_RDWR); dup(i); dup(i); /* handle standard I/O */
	    umask(027); /* set newly created file permissions */
	    chdir("/"); /* change running directory */

	    while(1) {
	      j = gps_read(gps_fd, gps_lon, gps_lat);
	      if(j == -1) {
		*gps_lon = *gps_lat = GPS_MISS;
	      }
	      sleep(5);
	    }
	  }

	  /* poll child */
	  
	  setsid(); /* obtain a new process group */
	  for (i = getdtablesize(); i >= 0; --i) {
		  if(i == gps_fd) continue;
		  if(i == usb_fd) continue;
		  if(i == file_fd) continue;
		  if(debug && i == 1) continue;
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

	if(reportpath) umask(0);
	

	listen_sd = lissen(addr, port);

	memset(fds, 0 , sizeof(fds));

	/* Add initial listening sockets  */

	nfds = 0;
	fds[nfds].fd = listen_sd;
	fds[nfds].events = POLLIN;
	nfds++;

	if(serialdev) {
	  fds[nfds].fd = usb_fd;
	  fds[nfds].events = POLLIN;
	  nfds++;
	}

	j = 0;

	while (1)  {
	  int i, ii;
	  static int proxy_start = 0;
	  char outbuf[BUFSIZ];

	  if (proxy_start++ == 0)
	    timeout = (10);
	  else 
	    timeout = 2000;

	  rc = poll(fds, nfds, timeout);
	    send_2_listners = 0;
	    
	    if (rc < 0)  {
	      //perror("  poll() failed");
	      break;
	    }
	    
	    if (rc == 0)  {
	      /* TIMEOUT: Try (re)-connect to proxy */
	      if(send_host && (send_host_sd == -1)) {

		if(debug)
		  printf("Trying %s on port %d. Connect ", send_host, send_port);

		send_host_sd = connect_remote(send_host, send_port);
		if (send_host_sd >= 0) {
		  fds[nfds].fd = send_host_sd;
		  fds[nfds].events = POLLIN;
		  nfds++;
		  if(debug)
		    printf("succeded\n");
		}
		else {
		  send_host_sd = -1;
		  if(debug)
		    perror("connect");
		}
	      }
	      continue;
	    }

	    current_size = nfds;
	    for (i = 0; i < current_size; i++)   {
	      
		
	      if(fds[i].revents == 0)
		continue;
	      

	      send_2_listners = 0;

	      /* Buffers used
		 io      : USB read buffer
		 outbuf  : whar's written to sensors.dat and port listers
		 buf temp: split USB read code
		 buffer  : read buffer from port listerners
	      */

	      if (fds[i].fd == usb_fd && fds[i].revents & POLLIN)   {
		memset(io,0,BUFSIZE);
		res = read(usb_fd, io, BUFSIZE);

		if(res > BUFSIZE){
		}
		else
		  strcat(buf, "ERR read\n");

	        if(filedev){
		  /* We have input in one line */
		  if(io[0] == '&' && io[1] == ':' && (date || utime))
		    print_report_header(gpsdev, outbuf);
		  strcat(outbuf, io);
		  write(file_fd, outbuf, strlen(outbuf));
		  if(reportpath) 
		    do_report(buf, reportpath);
		  
		  if(report)
		    send_2_listners = 1;

		}else{

		  for(ii=0; ii < res; ii++, j++)  {
		    if(io[ii] == END_OF_FILE) {
		      outbuf[0] = 0;
		      if(buf[0] == '&' && buf[1] == ':' && (date || utime))
		        print_report_header(gpsdev, outbuf);
		      else{ 
		        strcat(outbuf, "ERR missing signature");
		      }
		      buf[j] = 0;
		      strcat(outbuf, buf);
		      write(file_fd, outbuf, strlen(outbuf));
		      if(reportpath) 
		        do_report(buf, reportpath);

		      if(report)
		        send_2_listners = 1;

		      j = -1;
		  
		    }
		    else  {
		      buf[j] = io[ii];
		    }
		  }
		}
		fds[i].revents &= ~POLLIN;
	      }

	      if (fds[i].fd == listen_sd && fds[i].revents & POLLIN)   {

		new_sd = accept(listen_sd, NULL, NULL);
		if (new_sd != -1)  {
		  if(debug)
		    printf("  New incoming listner connection - %d\n", new_sd);
		  fds[nfds].fd = new_sd;
		  fds[nfds].events = POLLIN;
		  nfds++;
		}
		fds[i].revents &= ~POLLIN;
	      }

	      if (fds[i].revents & POLLIN)  {
		close_conn = FALSE;

		rc = recv(fds[i].fd, buffer, sizeof(buffer), 0);
		if (rc < 0)  {
		  if (errno != EWOULDBLOCK)  {
		    close_conn = TRUE;
		  }
		  break;
		}
		  
		if (rc == 0) 
		  close_conn = TRUE;

		/* Socket receive */
		if(rc > 0 && receive) {
		  len = rc;
		  buffer[len-1] = ' ';
		  memset(&saddr, 0, sizeof(saddr));
		  memset(&outbuf, 0, sizeof(outbuf));
		  saddr_len = sizeof(saddr);

		  getpeername(fds[i].fd, (struct sockaddr *)&saddr, &saddr_len);

		  {
		    struct sockaddr_in6 *v4 =  (struct sockaddr_in6 *) &saddr;
		    char str[INET6_ADDRSTRLEN];
		    int hdrlen;

		    ///	sprintf(outbuf, "%s SRC=%s\n", buffer, inet_ntoa(v4->sin6_addr));

		    /* If we receive non-timestamped reports - just add it */

		    if(buffer[0] == '&' && buffer[1] == ':' && (date || utime))
		      print_report_header(gpsdev, outbuf);
		    
		    hdrlen = strlen(outbuf);

		    sprintf(&outbuf[hdrlen], "%s SRC=%s\n", buffer, 
			    inet_ntop(AF_INET6, &v4->sin6_addr, str, sizeof(str)));

		    memset(&buffer, 0, sizeof(buffer));

		    /* override incoming time*/
		    if(receive_time_local)
		      print_report_time(&outbuf[0]);

		  }

		  send_2_listners = 1;
#if 0
		  if(cmd) 
		    rc = write(usb_fd, &buffer, len);
		  else 
		    rc = send(fds[i].fd, buffer, len, 0);
#endif
		}

		if (rc < 0)  
		  close_conn = TRUE;
		
		if (close_conn)  {
		  close(fds[i].fd);

		  if(fds[i].fd == send_host_sd) {
		   send_host_sd = -1;

		   if(debug)
		     printf("Closed connection to %s on port %d \n", send_host, send_port);
		  }
		  fds[i].fd = -1;
		  compress_array = TRUE;
		}
	      }  /* End of existing connection */
	      fds[i].revents = 0;
	    } /* Loop pollable descriptors */

	    /* Squeeze the array and decrement the number of file 
	       descriptors. We do not need to move back the events 
	       and  revents fields because the events will always
	       be POLLIN in this case, and revents is output.  */
	    

	    if (compress_array)  {
	      compress_array = FALSE;
	      for (i = 0; i < nfds; i++)  {
		if (fds[i].fd == -1)  {
		  for(j = i; j < nfds; j++)  {
		    fds[j].fd = fds[j+1].fd;
		  }
		  nfds--;
		}
	      }
	    }

	    if(send_2_listners) {
	      current_size = nfds;
	      for (i = 0; i < current_size; i++)   {


		if (fds[i].fd == usb_fd)
		  continue;
		
		if (fds[i].fd == listen_sd)
		  continue;

		len = strlen(outbuf);
		rc = send(fds[i].fd, outbuf, len, 0);
		
		if (rc < 0)  {
		  close_conn = TRUE;
		  break;
		}
	      }
	    }
	}

	if(gpsdev) {
	  munmap(gps_lon, sizeof(gps_lon));
	  munmap(gps_lat, sizeof(gps_lat));
	}
	if(!filedev){
		if (tcsetattr(usb_fd, TCSANOW, &tp_usb_old) < 0) {
		  perror("Couldn't restore term attributes");
		  exit(-1);
		}
	}

	if(gpsdev) {
	  if (tcsetattr(gps_fd, TCSANOW, &tp_gps_old) < 0) {
	    perror("Couldn't restore term attributes");
	    exit(-1);
	  }
	}

	if(serialdev)
	  lockfile_remove();
	exit(0);
}


#define BUFLEN  124
#define KNOT_TO_KMPH 1.852 

int gps_read(int fd, float *lon, float *lat)
{

  int bp;
  int debug = 0;

  float course, speed;
  char foo[6], buf[BUFLEN], valid, ns, ew;
  int try, res, maxtry = 10;
  unsigned int year, mon, day, hour, min, sec;
  int done = 0;

  for(try = 0; !done && try < maxtry; try++) {

    bp = 0;

    while(bp < BUFLEN) {
      int n, ok;
      char b;
      struct pollfd pfd;
      
      pfd.fd = fd;
      pfd.events = POLLIN;
      ok = poll( &pfd, 1, -1 );
      
    if ( ok < 0 ) 
	continue;
      
      n = read(fd, &b, 1);
      
      if( n < 0)
	continue;
     
      buf[bp++] = b;
      if(b == '\n') 
	break;
    }

    if(strncmp("$GPRMC", buf, 6) == 0 ) {
      
      if(debug)
	printf("%s", buf);
      
      res = sscanf(buf, 
		   "$GPRMC,%2d%2d%2d.%3c,%1c,%f,%1c,%f,%1c,%f,%f,%2d%2d%2d",
		   &hour, &min, &sec, foo, &valid, lat, &ns, lon, &ew, &speed, &course, &day, &mon, &year);

      if(res == 14 && valid == 'A') {

	*lat /= 100;
	*lon /= 100;

	if(ns == 'S')
	  *lat = -*lat;
	
	if(ew == 'W')
	  *lon = -*lon;

	done = 1;
      }
    }
  }
  if(!done)
    return(-1);
  else
    return 1;
}

/*
We get: $GPGSA,A,3,21,18,03,22,19,27,07,08,26,16,24,,1.7,0.8,1.5*36

$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39

Where:
     GSA      Satellite status
     A        Auto selection of 2D or 3D fix (M = manual) 
     3        3D fix - values include: 1 = no fix
                                       2 = 2D fix
                                       3 = 3D fix
     04,05... PRNs of satellites used for fix (space for 12) 
     2.5      PDOP (dilution of precision) 
     1.3      Horizontal dilution of precision (HDOP) 
     2.1      Vertical dilution of precision (VDOP)
     *39      the checksum data, always begins with *

*/


/*

We get: $GPRMC,080642.000,A,5951.0512,N,01736.8681,E,0.10,345.68,140307,,*0D      

$GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh



GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A

Where:
     RMC          Recommended Minimum sentence C
     123519       Fix taken at 12:35:19 UTC
     A            Status A=active or V=Void.
     4807.038,N   Latitude 48 deg 07.038' N
     01131.000,E  Longitude 11 deg 31.000' E
     022.4        Speed over the ground in knots
     084.4        Track angle in degrees True
     230394       Date - 23rd of March 1994
     003.1,W      Magnetic Variation
     *6A          The checksum data, always begins with *


RMC  = Recommended Minimum Specific GPS/TRANSIT Data

1    = UTC of position fix
2    = Data status (V=navigation receiver warning)
3    = Latitude of fix
4    = N or S
5    = Longitude of fix
6    = E or W
7    = Speed over ground in knots
8    = Track made good in degrees True
9    = UT date
10   = Magnetic variation degrees (Easterly var. subtracts from true course)
11   = E or W
12   = Checksum

*/
