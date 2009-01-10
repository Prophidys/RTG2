/****************************************************************************
   Program:     $Id: rtgutil.c,v 1.35 2008/01/19 03:01:32 btoneill Exp $
   Author:      $Author: btoneill $
   Date:        $Date: 2008/01/19 03:01:32 $
   Description: RTG Routines
****************************************************************************/

#include "common.h"
#include "rtg.h"

extern FILE *dfp;

/* read configuration file to establish local environment */
int read_rtg_config(char *file, config_t * set)
{
    FILE *fp;
    char buff[BUFSIZE];
    char p1[BUFSIZE];
    char p2[BUFSIZE];

    if ((fp = fopen(file, "r")) == NULL) {
        return (-1);
    } else {
		if (set->verbose >= LOW) {
			if (set->daemon)
				sysloginfo("Using RTG config file [%s].\n", file);
			else
				fprintf(dfp, "Using RTG config file [%s].\n", file);
		}
        while(!feof(fp)) {
           fgets(buff, BUFSIZE, fp);
           if (!feof(fp) && *buff != '#' && *buff != ' ' && *buff != '\n') {
              sscanf(buff, "%20s %20s", p1, p2);
              if (!strcasecmp(p1, "Interval")) set->interval = atoi(p2);
              else if (!strcasecmp(p1, "HighSkewSlop")) set->highskewslop = atof(p2);
              else if (!strcasecmp(p1, "LowSkewSlop")) set->lowskewslop = atof(p2);
              else if (!strcasecmp(p1, "SNMP_Port")) set->snmp_port = atoi(p2);
              else if (!strcasecmp(p1, "Threads")) set->threads = atoi(p2);
              else if (!strcasecmp(p1, "DB_Driver")) strncpy(set->dbdriver, p2, sizeof(set->dbdriver));
              else if (!strcasecmp(p1, "DB_Host")) strncpy(set->dbhost, p2, sizeof(set->dbhost));
              else if (!strcasecmp(p1, "DB_Database")) strncpy(set->dbdb, p2, sizeof(set->dbdb));
              else if (!strcasecmp(p1, "DB_User")) strncpy(set->dbuser, p2, sizeof(set->dbuser));
              else if (!strcasecmp(p1, "DB_Pass")) strncpy(set->dbpass, p2, sizeof(set->dbpass));
              else { 
                 fatalfile(dfp, "*** Unrecongized directive: %s=%s in %s\n", 
                    p1, p2, file);
              }
           }
        }
        if (set->threads < 1 || set->threads > MAX_THREADS) 
          fatalfile(dfp, "*** Invalid Number of Threads: %d (max=%d).\n", 
             set->threads, MAX_THREADS);
        return (0);
    }
}


int write_rtg_config(char *file, config_t * set)
{
    FILE *fp;

	if (set->verbose >= LOW && !(set->daemon))
		fprintf(dfp, "Writing default config file [%s].", file);
    if ((fp = fopen(file, "w")) == NULL) {
        fprintf(dfp, "\nCould not open '%s' for writing\n", file);
        return (-1);
    } else {
        fprintf(fp, "#\n# RTG v%s Master Config\n#\n", VERSION);
        fprintf(fp, "Interval\t%d\n", set->interval);
        fprintf(fp, "HighSkewSlop\t%f\n", set->highskewslop);
        fprintf(fp, "LowSkewSlop\t%f\n", set->lowskewslop);
        fprintf(fp, "SNMP_Port\t%d\n", set->snmp_port);
        fprintf(fp, "DB_Driver\t%s\n", set->dbdriver);
        fprintf(fp, "DB_Host\t%s\n", set->dbhost);
        fprintf(fp, "DB_Database\t%s\n", set->dbdb);
        fprintf(fp, "DB_User\t%s\n", set->dbuser);
        fprintf(fp, "DB_Pass\t%s\n", set->dbpass);
        fprintf(fp, "Threads\t%d\n", set->threads);
        fclose(fp);
        return (0);
    }
}


/* Populate Master Configuration Defaults */
void config_defaults(config_t * set)
{
   set->interval = DEFAULT_INTERVAL;
   set->highskewslop = DEFAULT_HIGHSKEWSLOP;
   set->lowskewslop = DEFAULT_LOWSKEWSLOP;
   set->snmp_port = DEFAULT_SNMP_PORT;
   set->threads = DEFAULT_THREADS;
   strncpy(set->dbdriver, DEFAULT_DB_DRIVER, sizeof(set->dbdriver));
   strncpy(set->dbhost, DEFAULT_DB_HOST, sizeof(set->dbhost));
   strncpy(set->dbdb, DEFAULT_DB_DB, sizeof(set->dbdb));
   strncpy(set->dbuser, DEFAULT_DB_USER, sizeof(set->dbuser));
   strncpy(set->dbpass, DEFAULT_DB_PASS, sizeof(set->dbpass));
   set->dboff = FALSE;
   set->withzeros = FALSE;
   set->verbose = OFF; 
   set->daemon = TRUE;
   strncpy(config_paths[0], CONFIG_PATH_1, sizeof(config_paths[0]));
   strncpy(config_paths[1], CONFIG_PATH_2, sizeof(config_paths[1]));
   snprintf(config_paths[2], sizeof(config_paths[1]), "%s/etc/", RTG_HOME);
   return;
}


/* Print RTG stats */
void print_stats(stats_t stats, config_t *set)
{
  debug(OFF, "[Polls = %lld] [DBInserts = %lld] [Wraps = %d] [OutOfRange = %d]\n",
      stats.polls, stats.db_inserts, stats.wraps, stats.out_of_range);
  debug(OFF, "[No Resp = %d] [SNMP Errs = %d] [Slow = %d] [PollTime = %2.3f%c]\n",
      stats.no_resp, stats.errors, stats.slow, stats.poll_time, 's');
  return;
}


/* A fancy sleep routine */
int sleepy(float sleep_time, config_t *set)
{
    int chunks = 10;
    int i;

	if (set->daemon) {
		usleep((unsigned int) (sleep_time*MEGA));
		return (0);
	}
	if (sleep_time > chunks) {
		debug(LOW, "Next Poll: ");
		for (i = chunks; i > 0; i--) {
			if (set->verbose >= LOW) {
				printf("%d...", i);
				fflush(NULL);
			}
			usleep((unsigned int) (sleep_time*MEGA/ chunks));
		}
		if (set->verbose >= LOW) printf("\n");
	} else {
		sleep_time*=MEGA;
		usleep((unsigned int) sleep_time);
	}
	return (0);
}


/* Timestamp */
void timestamp(char *str) {
   time_t clock;
   struct tm *t;

   clock = time(NULL);
   t = localtime(&clock);
   printf("[%02d/%02d %02d:%02d:%02d %s]\n", t->tm_mon + 1, 
      t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, str);
   return;
}

char *file_timestamp() {
   static char str[BUFSIZE];
   time_t clock;
   struct tm *t;

   clock = time(NULL);
   t = localtime(&clock);
   snprintf(str, sizeof(str), "%02d%02d_%02d:%02d:%02d", t->tm_mon + 1, 
      t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
   return(str);
}

/* Return number double precision seconds difference between timeval structs */
double timediff(struct timeval tv1, struct timeval tv2) {
	double result = 0.0;

	result = ( (double) tv1.tv_usec / MEGA + tv1.tv_sec ) -
		( (double) tv2.tv_usec / MEGA + tv2.tv_sec );

	if (result < 0) {
		result = ( (double) tv2.tv_usec / MEGA + tv2.tv_sec ) -
			( (double) tv1.tv_usec / MEGA + tv1.tv_sec );
	}

	return (result);
}


int checkPID(char *pidfile, config_t *set) {
	FILE *pidptr = NULL;
	pid_t rtgpoll_pid;

	rtgpoll_pid = getpid();
	if ((pidptr = fopen(pidfile, "r")) != NULL) {
		char temp_pid[BUFSIZE];
		int verify = 0;
		/* rtgpoll appears to already be running. */
		while (fgets(temp_pid,BUFSIZE,pidptr)) {
			verify = atoi(temp_pid);
			debug(LOW, "Checking another instance with pid %d.\n", verify);
			if (kill(verify, 0) == 0) {
				fatal("rtgpoll is already running. Exiting.\n");
			} else {
				/* This process isn't really running */
				debug(LOW, "PID %d is no longer running. Starting anyway.\n", verify);
				unlink(pidfile);
			}
		}
	}

	/* This is good, rtgpoll is not running. */
	if ((pidptr = fopen(pidfile, "w")) != NULL) {
		fprintf(pidptr, "%d\n", rtgpoll_pid);
		fclose(pidptr);
		return(0);
	}
	else {
		/* Yuck, we can't even write the PID. Goodbye. */
		return(-1);
	}
}

int alldigits(char *s) {
    int result = TRUE;

	if (*s == '\0') return FALSE;

    while (*s != '\0') {
        if (!(*s >= '0' && *s <= '9')) 
			return FALSE;
        s++;
    }
    return result;
}

/* As per Stevens */
int daemon_init() {
	pid_t pid;
	
	if ( (pid = fork()) < 0)
		return -1;
	else if (pid != 0)
		exit (0);	/* Parent goes buh-bye */
	
	/* child continues */
	setsid();
	/* chdir("/"); */
	umask(0);
	return (0);
}

