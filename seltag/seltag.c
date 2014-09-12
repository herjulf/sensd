#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION "0.9 121231"

/*
 *  Copyright Robert Olsson <robert@herjulf.se>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
*/

void usage(void)
{
  printf("\nseltag formats data on stdin by tag to column format on stdout\n");
  printf("Version %s\n\n", VERSION);
  printf("Example\n");
  printf("seltag 2 ID=#s T=#s V_MCU=#s < infile > outfile\n");
  printf("2 first fields copied as-is. Labels w. ID= T= V_MCU= are extracted and copied w/o label\n\n");
  exit(-1);
}

struct tag {
  char buf[40];
};
struct tag t[40];
unsigned int year, mon, day, hour, min, sec;


int time_parse(char *buf)
{
    int  res = sscanf(buf, 
		   "%4d-%2d-%2d %2d:%2d:%2d",
		      &year, &mon, &day, &hour, &min, &sec);
    
    return res;
}


int main(int ac, char *av[]) 
{
  char buf[BUFSIZ], buf1[BUFSIZ], *res;
  int i, j, k, cpy;
  char timebuf[40];
  if (ac == 1 || strcmp(av[1], "-h") == 0) 
    usage();

  cpy = atoi(av[1]);

  while ( fgets ( buf, BUFSIZ, stdin ) != NULL ) {
    res = strtok( buf, " " );

    for(k = 2; k < ac; k++) 
      strcpy( t[k].buf, "Miss");

    timebuf[0] = 0;
    for(i = 0;  res != NULL; i++ ) {

      if(i < cpy) {
      	strcat(timebuf, res);
	strcat(timebuf, " ");
       }

      for(k = 2; k < ac; k++) {
	j = sscanf(res, av[k], buf1);   
	if(j) 
	  strcpy( t[k].buf, buf1);
      } /* for */

      res = strtok( NULL, " " );
    }

    time_parse(timebuf);
    printf("%04u-%02u-%02u %02u:%02u:%02u ",
    		      year, mon, day, hour, min, sec);


    for(k = 2; k < ac; k++) 
      printf("%s ", t[k].buf);
    printf("\n");

  }
  exit(0);
}
