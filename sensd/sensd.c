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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "devtag-allinone.h"

#define VERSION "4.2 131021"
#define END_OF_FILE 26
#define CTRLD  4
#define P_LOCK "/var/lock"

char lockfile[128]; /* UUCP lock file of terminal */
char dial_tty[128];
char username[16];
int pid;
int retry = 6;

float *gps_lon, *gps_lat;

#define BUFSIZE 1024
#define SERVER_PORT  1234

#define TRUE             1
#define FALSE            0


void usage(void)
{
  printf("\nVersion %s\n", VERSION);
  printf("\nsensd daemon reads sensors data from serial/USB and writes to file\n");
  printf("Usage: sensd [-pport] [-cmd] [-report] [-utc] [-ffile] [-Rpath] [-ggpsdev] DEV\n");
  printf(" -report Enable net reports\n");
  printf(" -cmd  Enable net commands\n");
  printf(" -pport TCP server port. Default %d\n", SERVER_PORT);
  printf(" -utc time in UTC\n");
  printf(" -ffile data file. Default is /var/log/sensors.dat\n");
  printf(" -Rpath Path for reports. One dir per sensor. One file per value.\n");
  printf(" -ggpsdev Device for gps\n");
  printf(" -infile Read data from a file\n");
  printf("Example 1: sensd  /dev/ttyUSB0\n");
  printf("Example 2: sensd -report -f/dev/null -g/dev/ttyUSB1 /dev/ttyUSB0\n");
  exit(-1);
}


/* Function declarations */
int gps_read(int fd, float *lon, float *lat);

/* Options*/
int cmd, date, utime, utc, background;
long baud;

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
	  sprintf(buf, "%04d-%02d-%02d %2d:%02d:%02d TZ=%s ",
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
    sprintf(buf, "GWGPS_LON=%f ", *gps_lon);
    strcat(datebuf, buf);

    sprintf(buf, "GWGPS_LAT=%f ", *gps_lat);
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

int main(int ac, char *av[]) 
{
  struct termios tp, tp_usb_old, tp_gps_old;
	int usb_fd;
	int file_fd;
	int gps_fd;
	char io[BUFSIZE];
	char buf[BUFSIZE];
	char *filename = NULL;
	char *gpsdev = NULL;
	char *reportpath = NULL;
	unsigned short filedev = 0;
	int res;
	int i, len;
	char *prog = basename (av[0]);
	int    rc, on = 1;
	int    listen_sd = -1, new_sd = -1;
	int    compress_array = FALSE;
	int    close_conn;
	char   buffer[BUFSIZE];
	struct sockaddr_in   addr;
	int    timeout;
	struct pollfd fds[200];
	int    nfds = 2, current_size = 0, j;
	int    send_2_listners;
	unsigned short port = SERVER_PORT;
	unsigned short cmd = 0, report = 0;


	if (strcmp(prog, "sensd") == 0) {
	  baud = B38400;
	  background = 1;
	  date = 1;
	  utime = 1;
	  utc = 0;
	  filename = "/var/log/sensors.dat";
	}

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
	      filename = av[i]+2;

	    else if (strncmp(av[i], "-g", 2) == 0) 
	      gpsdev = av[i]+2;

	    else if (strncmp(av[i], "-R", 2) == 0) {
	      reportpath = av[i]+2;
	      if(!*reportpath) reportpath = "/var/lib/sensd";
	    }

	    else if (strncmp(av[i], "-p", 2) == 0) {
	      port = atoi(av[i]+2);
	    }

	    else if (strcmp(av[i], "-report") == 0) 
	      report = 1;

	    else if (strcmp(av[i], "-cmd") == 0) 
	      cmd = 1;

	    else if (strcmp(av[i], "-infile") == 0) 
	      filedev = 1;

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
	  
	  strncpy(dial_tty, devtag_get(av[i]), sizeof(dial_tty));

	  while (! get_lock()) {
	    if(--retry == 0)
	      exit(-1);
	    sleep(1);
	}

	if(filedev) {
		usb_fd = open(av[i],O_RDONLY);
		if(file_fd < 0) {
			fprintf(stderr, "Failed to open filedev '%s'\n", filename);
			exit(2);
		}
	} else {
		if ((usb_fd = open(devtag_get(av[i]), O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0) {
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
		*gps_lon = *gps_lat = -1;
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
	
	listen_sd = socket(AF_INET, SOCK_STREAM, 0);

	if (listen_sd < 0)  { 
	  perror("socket() failed");
	  exit(-1);
	}

	/* Allow socket descriptor to be reuseable */

	rc = setsockopt(listen_sd, SOL_SOCKET,  SO_REUSEADDR,
			(char *)&on, sizeof(on));
	if (rc < 0)   {
	  perror("setsockopt() failed");
	  close(listen_sd);
	  exit(-1);
	}

	/* Set socket to be nonblocking. All of the sockets for    
	   the incoming connections will also be nonblocking since  
	   they will inherit that state from the listening socket.   */

	rc = ioctl(listen_sd, FIONBIO, (char *)&on);
	if (rc < 0)  {
	    perror("ioctl() failed");
	    close(listen_sd);
	    exit(-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port        = htons(port);

	rc = bind(listen_sd, (struct sockaddr *)&addr, sizeof(addr));
 
	if (rc < 0)   {
	  perror("bind() failed");
	  close(listen_sd);
	  exit(-1);
	}

	/* Set the listen back log  */

	rc = listen(listen_sd, 32);

	if (rc < 0)   {
	  perror("listen() failed");
	  close(listen_sd);
	  exit(-1);
	}

	memset(fds, 0 , sizeof(fds));

	/* Add initial listening sockets  */

	fds[0].fd = listen_sd;
	fds[0].events = POLLIN;

	fds[1].fd = usb_fd;
	fds[1].events = POLLIN;

	nfds = 2;
	timeout = (10 * 1000);

	j = 0;

	while (1)  {
	  int i, ii;
	    char outbuf[512];

	    timeout = (10 * 1000);
	    
	    rc = poll(fds, nfds, timeout);
	    send_2_listners = 0;
	    
	    if (rc < 0)  {
	      //perror("  poll() failed");
	      break;
	    }
	    
	    if (rc == 0)  {
	      /* Timeout placeholder */
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
		if(res > BUFSIZE);
		else
		  strcat(buf, "ERR read");
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
		  // printf("  New incoming connection - %d\n", new_sd);
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

		if(rc > 0) {
		  len = rc;
		  buffer[len-1] = 0xd;

		  if(cmd) 
		    rc = write(usb_fd, &buffer, len);
		  else 
		    rc = send(fds[i].fd, buffer, len, 0);
		}

		if (rc < 0)  
		  close_conn = TRUE;
		
		if (close_conn)  {
		  close(fds[i].fd);
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
	      static int cnt;
	      cnt++;
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
  char foo[6], buf[BUFLEN], c[1];
  char valid;
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
		   &hour, &min, &sec, foo, &valid, lat, c,  lon, c, &speed, &course, &day, &mon, &year);

      if(res == 14 && valid == 'A') {
	*lat /= 100;
	*lon /= 100;
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
