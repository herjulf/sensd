#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define VERSION "1.0 150624"

/*
 *  Copyright Robert Olsson <robert@herjulf.se>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
*/

struct tag {
    char buf[40];
};
struct tag t[40];
unsigned int year, mon, day, hour, min, sec;
struct timeval tv;

/* mktime from linux kernel */

/*
 *  linux/kernel/time.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  This file contains the interface functions for the various
 *  time related system calls: time, stime, gettimeofday, settimeofday,
 *			       adjtime
 */

/* Converts Gregorian date to seconds since 1970-01-01 00:00:00.
 * Assumes input in normal date format, i.e. 1980-12-31 23:59:59
 * => year=1980, mon=12, day=31, hour=23, min=59, sec=59.
 *
 * [For the Julian calendar (which was used in Russia before 1917,
 * Britain & colonies before 1752, anywhere else before 1582,
 * and is still in use by some communities) leave out the
 * -year/100+year/400 terms, and add 10.]
 *
 * This algorithm was first published by Gauss (I think).
 *
 * WARNING: this function will overflow on 2106-02-07 06:28:16 on
 * machines were long is 32-bit! (However, as time_t is signed, we
 * will already get problems at other places on 2038-01-19 03:14:08)
 */

unsigned long our_mktime(const unsigned int year0, const unsigned int mon0, const unsigned int day,
    const unsigned int hour, const unsigned int min, const unsigned int sec)
{
    unsigned int mon = mon0, year = year0;

    /* 1..12 -> 11,12,1..10 */
    if (0 >= (int)(mon -= 2)) {
        mon += 12; /* Puts Feb last since it has leap day */
        year -= 1;
    }

    return ((((unsigned long)(year / 4 - year / 100 + year / 400 + 367 * mon / 12 + day) + year * 365 - 719499) * 24
                + hour /* now have hours */
                ) * 60
               + min /* now have minutes */
               ) * 60
        + sec; /* finally seconds */
}

int time_parse(char* buf)
{
    int res = sscanf(buf, "%4d-%2d-%2d %2d:%2d:%2d", &year, &mon, &day, &hour, &min, &sec);

    return res;
}

void time_adjust(int offset)
{
    struct timeval* tvp;
    struct tm tm;
    char tbuf[40];

    tvp = &tv;

    tvp->tv_sec = our_mktime(year, mon, day, hour, min, sec);
    tvp->tv_usec = 0;
    tvp->tv_sec += offset;
    gmtime_r(&tvp->tv_sec, &tm);

    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm);
    printf("%s ", tbuf);
}

void usage(void)
{
    printf("\nseltag formats data on stdin by tag to column format on stdout\n");
    printf("Version %s\n\n", VERSION);
    printf("Example\n");
    printf("seltag [-debug ] [-first_field 2] [-time_adjust 0] -sel  ID=%%s T=%%s V_MCU=%%s < infile > outfile\n");
    printf(" -sel           colums to extract. Format TAG=%%s\n");
    printf(" -debug\n");
    printf(" -time_adjust   add or del number Sec\n");
    // printf(" -first_fields  copied as-is. Time and date.\n");
    printf("\nExample 1: Extract T\n seltag -sel T=%%s < infile\n");
    printf("Example 2: Extract V_IN, T, RH\n seltag -sel V_IN=%%s T=%%s RH=%%s < infile\n");
    printf("Example 3: Extract T, RH and adjust -2Hours\n seltag -time_adjust -7200 -sel T=%%s RH=%%s < infile\n");
    exit(-1);
}

int main(int ac, char* av[])
{
    char buf[BUFSIZ], buf1[BUFSIZ], *res;
    int i, j, k;
    int cpy, debug;
    char timebuf[40];
    int timeadjust;
    int selpos;

    if (ac == 1 || strcmp(av[1], "-h") == 0)
        usage();

    debug = 0;
    cpy = 2;
    timeadjust = 0;
    selpos = 0;

    for (i = 1; (i < ac) && (av[i][0] == '-'); i++) {

        if (strncmp(av[i], "-debug", 6) == 0) {
            debug = 1;
        }
        else if (strncmp(av[i], "-time_adjust", 12) == 0) {
            timeadjust = atoi(av[++i]);
        }
        // else if (strncmp(av[i], "-first_fields", 13) == 0) {
        //  cpy = atoi(av[++i]);
        //}
        else if (strncmp(av[i], "-sel", 4) == 0) {
            selpos = ++i;
            break;
        }
        else
            usage();
    }

    if (debug) {
        printf("first_fields=%d\n", cpy);
        printf("time_adjust=%d\n", timeadjust);
        printf("sel_pos=%d\n", selpos);
    }

    if (selpos == 0) {
        printf("\nError. You must give -sel followed by selection criterias\n\n");
        usage();
    }

    while (fgets(buf, BUFSIZ, stdin) != NULL) {
        res = strtok(buf, " ");

        for (k = selpos; k < ac; k++)
            strcpy(t[k].buf, "Miss");

        timebuf[0] = 0;
        for (i = 0; res != NULL; i++) {

            if (i < cpy) {
                strcat(timebuf, res);
                strcat(timebuf, " ");
            }

            for (k = selpos; k < ac; k++) {
                j = sscanf(res, av[k], buf1);
                if (j)
                    strcpy(t[k].buf, buf1);
            } /* for */

            res = strtok(NULL, " ");
        }

        time_parse(timebuf);

        if (timeadjust)
            time_adjust(timeadjust);
        else
            printf("%04u-%02u-%02u %02u:%02u:%02u ", year, mon, day, hour, min, sec);

        for (k = 2; k < ac; k++)
            printf("%s ", t[k].buf);
        printf("\n");
    }
    exit(0);
}
