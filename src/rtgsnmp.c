/****************************************************************************
   Program:     $Id: rtgsnmp.c,v 1.41 2008/01/19 03:01:32 btoneill Exp $
   Author:      $Author: btoneill $
   Date:        $Date: 2008/01/19 03:01:32 $
   Description: RTG SNMP Routines
****************************************************************************/

#include "common.h"
#include "rtg.h"
#include "rtgdbi.h"

#ifdef OLD_UCD_SNMP
 #include "asn1.h"
 #include "snmp_api.h"
 #include "snmp_impl.h"
 #include "snmp_client.h"
 #include "mib.h"
 #include "snmp.h"
#else
 #include "net-snmp-config.h"
 #include "net-snmp-includes.h"
#endif

extern target_t *current;
extern stats_t stats;
extern config_t *set;

#ifndef FEATURES
/* REB - goes away if each thread starts own DB connection */
extern MYSQL mysql;
#endif

#ifdef FEATURES
void cancel_lock(void *arg)
{
    /* to be called from the cleanup handler */
    PT_MUTEX_UNLOCK(arg);
}

void cleanup_db(void *arg)
{
	db_disconnect();
}

#endif

void *poller(void *thread_args)
{
    worker_t *worker = (worker_t *) thread_args;
    crew_t *crew = worker->crew;
    target_t *entry = NULL;
    void *sessp = NULL;
    struct snmp_session session;
    struct snmp_pdu *pdu = NULL;
    struct snmp_pdu *response = NULL;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    struct variable_list *vars = NULL;
    unsigned long long result = 0;
    unsigned long long last_value = 0;
    unsigned long long insert_val = 0;
    int poll_status = 0, db_status = 0, bits = 0, init = 0;
    char query[BUFSIZE];
    char storedoid[BUFSIZE];
    char result_string[BUFSIZE];
    int cur_work = 0;
    int prev_work = 99999999;
    int loop_count = 0;


#ifdef FEATURES
	/* Per thread SQL connection testing */
  #if HAVE_MYSQL
	MYSQL mysql;
  #elif HAVE_PGSQL
	PGconn *pgsql;
  #endif
	double rate = 0;
	struct timezone tzp;
	struct timeval current_time;
	struct timeval last_time;

	/* for thread settings */
	int oldstate, oldtype;
#endif

	debug(HIGH, "Thread [%d] starting.\n", worker->index);

    pthread_cleanup_push(cleanup_db, NULL);



    /* Attempt to connect to the MySQL Database */
#ifdef FEATURES
	/* Per thread MySQL connection testing */
    if (!(set->dboff)) {
	/* load the database driver */
	if (!(db_init(set))) {
		fatal("** Database error - check configuration.\n");
	}
	/* connect to the database */
	if (!(db_connect(set))) {
		fatal("server not responding.\n");
	}
		/* set up cancel function for exit */
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
		pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldtype);
    }
#endif /* FEATURES */

    while (1) {

/*        if(loop_count >= POLLS_PER_TRANSACTION) {
            debug(HIGH, "Thread [%d] doing commit on %d\n", worker->index, POLLS_PER_TRANSACTION);
            db_status = db_commit(); 
            loop_count = 0;
        }
*/

#ifdef FEATURES
	/* see if we've been cancelled before we start another loop */
	pthread_testcancel();
#endif
	debug(DEVELOP, "Thread [%d] locking (wait on work)\n", worker->index);

	PT_MUTEX_LOCK(&crew->mutex);
#ifdef FEATURES
	/* add an unlock to the cancel stack */
	pthread_cleanup_push(cancel_lock, &crew->mutex);	
#endif

	while (current == NULL) {
		PT_COND_WAIT(&crew->go, &crew->mutex);
	}
	debug(DEVELOP, "Thread [%d] done waiting, received go (work cnt: %d)\n",
		worker->index, crew->work_count);
        cur_work = crew->work_count;
/*         if(cur_work > prev_work) {
            debug(HIGH, "Thread [%d] doing commit at %d\n", worker->index,time(NULL));
            db_status = db_commit(); 
            loop_count = 0;
        }
*/
        prev_work = cur_work;

	if (current != NULL) {
		debug(DEVELOP, "Thread [%d] processing %s %s (%d work units remain in queue)\n",
			worker->index, current->host->host, current->objoid, crew->work_count);
	    snmp_enable_stderrlog(); 
            snmp_sess_init(&session);
		if (current->host->snmp_ver == 2)
			session.version = SNMP_VERSION_2c;
		else
			session.version = SNMP_VERSION_1;
	    session.peername = current->host->host;
	    session.community = current->host->community;
		session.remote_port = set->snmp_port;
	    session.community_len = strlen(session.community);

	    sessp = snmp_sess_open(&session);
	    anOID_len = MAX_OID_LEN;
	    pdu = snmp_pdu_create(SNMP_MSG_GET);
	    read_objid(current->objoid, anOID, &anOID_len);
	    entry = current;
	    last_value = current->last_value;
#ifdef FEATURES
		/* save the time so we can calculate rate */
		last_time = current->last_time;
#endif
	    init = current->init;
	    insert_val = 0;
	    bits = current->bits;
	    strncpy(storedoid, current->objoid, sizeof(storedoid));
		current = getNext();
	}
	debug(DEVELOP, "Thread [%d] unlocking (done grabbing current)\n", worker->index);
	PT_MUTEX_UNLOCK(&crew->mutex);
#ifdef FEATURES
	/* take the unlock off the cancel stack */
	pthread_cleanup_pop(FALSE);
#endif

	snmp_add_null_var(pdu, anOID, anOID_len);
	if (sessp != NULL) 
	   poll_status = snmp_sess_synch_response(sessp, pdu, &response);
	else
	   poll_status = STAT_DESCRIP_ERROR;

	/* Collect response and process stats */
	PT_MUTEX_LOCK(&stats.mutex);
	if (poll_status == STAT_DESCRIP_ERROR) {
	    stats.errors++;
            printf("*** SNMP Error: (%s) Bad descriptor.\n", session.peername);
	} else if (poll_status == STAT_TIMEOUT) {
	    stats.no_resp++;
	    printf("*** SNMP No response: (%s@%s).\n", session.peername,
	       storedoid);
	} else if (poll_status != STAT_SUCCESS) {
	    stats.errors++;
	    printf("*** SNMP Error: (%s@%s) Unsuccessuful (%d).\n", session.peername,
	       storedoid, poll_status);
	} else if (poll_status == STAT_SUCCESS && response->errstat != SNMP_ERR_NOERROR) {
	    stats.errors++;
	    printf("*** SNMP Error: (%s@%s) %s\n", session.peername,
	       storedoid, snmp_errstring(response->errstat));
	} else if (poll_status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR && response->variables->type == SNMP_NOSUCHINSTANCE) {
	    stats.errors++;
	    printf("*** SNMP Error: No Such Instance Exists (%s@%s)\n", session.peername, storedoid);
	} else if (poll_status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
	    stats.polls++;
	} 
	PT_MUTEX_UNLOCK(&stats.mutex);

	/* Liftoff, successful poll, process it */
#ifdef FEATURES
	/* Get the current time */
	gettimeofday(&current_time, &tzp);
#endif

	if (poll_status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR && response->variables->type != SNMP_NOSUCHINSTANCE) {
	    vars = response->variables;
#ifdef OLD_UCD_SNMP
            sprint_value(result_string, anOID, anOID_len, vars);
#else
	    snprint_value(result_string, BUFSIZE, anOID, anOID_len, vars);
#endif
	    switch (vars->type) {
		/*
		 * Switch over vars->type and modify/assign result accordingly.
		 */
		case ASN_COUNTER64:
		    debug(DEBUG, "64-bit result: (%s@%s) %s\n", session.peername, storedoid, result_string);
		    result = vars->val.counter64->high;
		    result = result << 32;
		    result = result + vars->val.counter64->low;
		    break;
		case ASN_COUNTER:
		    debug(DEBUG, "32-bit result: (%s@%s) %s\n", session.peername, storedoid, result_string);
		    result = (unsigned long) *(vars->val.integer);
		    break;
		case ASN_INTEGER:
		    debug(DEBUG, "Integer result: (%s@%s) %s\n", session.peername, storedoid, result_string);
		    result = (unsigned long) *(vars->val.integer);
		    break;
		case ASN_GAUGE:
		    debug(DEBUG, "32-bit gauge: (%s@%s) %s\n", session.peername, storedoid, result_string);
		    result = (unsigned long) *(vars->val.integer);
		    break;
		case ASN_TIMETICKS:
		    debug(DEBUG, "Timeticks result: (%s@%s) %s\n", session.peername, storedoid, result_string);
		    result = (unsigned long) *(vars->val.integer);
		    break;
		case ASN_OPAQUE:
		    debug(DEBUG, "Opaque result: (%s@%s) %s\n", session.peername, storedoid, result_string);
		    result = (unsigned long) *(vars->val.integer);
		    break;
		case ASN_OCTET_STR:
		    debug(DEBUG, "String Result: (%s@%s) %s\n", session.peername, storedoid, result_string);
#ifdef HAVE_STRTOLL
		    result = strtoll(vars->val.string, NULL, 0);
#else
		    result = strtol(vars->val.string, NULL, 0);
#endif
		    break;
		default:
		    debug(LOW, "Unknown result type: (%s@%s) %s\n", session.peername, storedoid, result_string);
	    }

		/* Gauge Type */
		if (bits == 0) {
			if (result != last_value) {
				insert_val = result;
				debug(DEVELOP, "Thread [%d]: Gauge change from %lld to %lld\n", worker->index, last_value, insert_val);
			} else {
				if (set->withzeros) 
					insert_val = result;
				debug(DEVELOP, "Thread [%d]: Gauge steady at %lld\n", worker->index, insert_val);
			}
	    /* Counter Wrap Condition */
	    } else if (result < last_value) {
			PT_MUTEX_LOCK(&stats.mutex);
              stats.wraps++;
			PT_MUTEX_UNLOCK(&stats.mutex);
	      if (bits == 32) insert_val = (THIRTYTWO - last_value) + result;
	      else if (bits == 64) insert_val = (SIXTYFOUR - last_value) + result;

#ifdef FEATURES
			rate = insert_val / timediff(current_time, last_time);
#endif

	      debug(LOW, "*** Counter Wrap (%s@%s) [poll: %llu][last: %llu][insert: %llu]\n",
	         session.peername, storedoid, result, last_value, insert_val);
	    /* Not a counter wrap and this is not the first poll */
	    } else if ((last_value >= 0) && (init != NEW)) {
		insert_val = result - last_value;

#ifdef FEATURES
			rate = insert_val / timediff(current_time, last_time);
#endif
		
	        /* Print out SNMP result if verbose */
	        if (set->verbose == DEBUG)
		  printf("Thread [%d]: (%lld-%lld -- %llu) = %llu\n", worker->index, result, last_value, insert_val,rate);
	        if (set->verbose == HIGH)
		  printf("Thread [%d]: %llu\n", worker->index, insert_val);
            /* last_value < 0, so this must be the first poll */
	    } else {
#ifdef FEATURES
			/* set up this result for the next poll */
			entry->last_value = result;
#endif
			debug(HIGH, "Thread [%d]: First Poll, Normalizing\n", worker->index);
	    }

		/* Check for bogus data, either negative or unrealistic */
	    if (insert_val > entry->maxspeed || result < 0) {
			debug(LOW, "*** Out of Range (%s@%s) [insert_val: %llu] [oor: %lld]\n",
				session.peername, storedoid, insert_val, entry->maxspeed);
			insert_val = 0;
#ifdef FEATURES
			rate = 0;
#endif
			PT_MUTEX_LOCK(&stats.mutex);
			stats.out_of_range++;
			PT_MUTEX_UNLOCK(&stats.mutex);
	    }

		if (!(set->dboff)) {
			if ( (insert_val > 0) || (set->withzeros) ) {
#ifndef FEATURES
				/* MP - since we have our own database connection we don't need to lock it anymore */
				PT_MUTEX_LOCK(&crew->mutex);
#endif

                                debug(DEVELOP, "db_insert sent: %s %d %d %e\n",entry->table,entry->iid,insert_val,rate);
				/* insert into the database */
				db_status = db_insert(entry->table, entry->iid, insert_val, rate);

#ifndef FEATURES
				/* MP - since we have our own database connection we don't need to lock it anymore */
				PT_MUTEX_UNLOCK(&crew->mutex);
#endif

				if (db_status) {
					PT_MUTEX_LOCK(&stats.mutex);
					stats.db_inserts++;
					PT_MUTEX_UNLOCK(&stats.mutex);
				} else {
					fatal("Fatal database error.\n");
				}
			} /* insert_val > 0 or withzeros */	
		} /* !dboff */

	} /* STAT_SUCCESS */

/*	debug(HIGH, "Thread [%d] doing commit\n", worker->index);
        db_status = db_commit(); */

        if (sessp != NULL) {
           snmp_sess_close(sessp);
           if (response != NULL) snmp_free_pdu(response);
        }

	debug(DEVELOP, "Thread [%d] locking (update work_count)\n", worker->index);
	PT_MUTEX_LOCK(&crew->mutex);
	crew->work_count--;
	/* Only if we received a positive result back do we update the
	   last_value object */
	if (poll_status == STAT_SUCCESS) {
		entry->last_value = result;
		if (init == NEW) entry->init = LIVE;
	}

#ifdef FEATURES
	/* always update the time */
	entry->last_time = current_time;	
#endif

	if (crew->work_count <= 0) {
	    debug(HIGH, "Queue processed. Broadcasting thread done condition.\n");
		PT_COND_BROAD(&crew->done);
	}
	debug(DEVELOP, "Thread [%d] unlocking (update work_count)\n", worker->index);

		PT_MUTEX_UNLOCK(&crew->mutex);

        loop_count++;
    }				/* while(1) */
    pthread_cleanup_pop(FALSE);

/* Not reached */
}
