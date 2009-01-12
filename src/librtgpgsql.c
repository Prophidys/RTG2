/*
 * RTG PostgresSQL database driver
 */
  
#include "rtg.h"
#include "rtgdbi.h"

#include <libpq-fe.h>

#include <sys/param.h>

/* thread-specific global variable */
pthread_key_t key;

config_t *set;

/* variable cleanup function */
void killkey(void *target) {
        free(target);
}
	 
/* this gets called once from dl_init */
void my_makekey() {
	/* this shouldn't fail and we're too early to return an error */
	pthread_key_create(&key, killkey);
}
			  
/* called when library loads */
void __attribute__ ((constructor)) dl_init(void) {
/* only call the thread-specific variable setup once */
	pthread_once_t  once = PTHREAD_ONCE_INIT;

	pthread_once(&once, my_makekey);

}

/* return the thread-specific pgsql variable */
PGconn* getpgsql() {
        PGconn *pgsql;
	pgsql = pthread_getspecific(key);

	return(pgsql);
}

/* utility function to safely escape table names */
char *escape_string(char *output, char *input)
{
	/* length of string */
	size_t input_len = strlen(input);

	/* target for PQescapeString */
	/* worst case is every char escaped plus terminating NUL */
	char *scratch = malloc(input_len*2+1);

	/* TODO check return */
	/* escape the string */
	PQescapeString(scratch, input, input_len);

	/* set output to correct length string */
	asprintf(&output, "%s", scratch);

	free(scratch);

	return output;
}


int __db_test() {
	return 1;
}

int __db_status() {
	PGconn *pgsql = getpgsql();

	if (PQstatus(pgsql) == CONNECTION_OK) {
		return TRUE;
	} else {
		debug(LOW, "Postgres error: %s\n", PQerrorMessage(pgsql));
		return FALSE;
	}
}

int __db_connect(config_t *config) {
	char *connectstring;
	PGconn* pgsql;

	set = config;

	/* reserve space for the pgsql variable */
	pgsql = (PGconn*)malloc(sizeof(PGconn*));
	if (!pgsql) {
		debug(LOW, "malloc: out of memory\n");
		return FALSE;
	}

	/* TODO escape strings */
	asprintf(&connectstring, "host='%s' dbname='%s' user='%s' password='%s'", set->dbhost, set->dbdb, set->dbuser, set->dbpass);

	pgsql = PQconnectdb(connectstring);

	free(connectstring);

	if (PQstatus(pgsql) == CONNECTION_OK) {
		debug(LOW, "Postgres connected, PID: %u\n", PQbackendPID(pgsql));
	} else {
		debug(LOW, "Failed to connect to postgres server: %s", PQerrorMessage(pgsql));
		return FALSE;
	}

	/* put the connection into thread-local storage */
	if (!pthread_setspecific(key, (void*)pgsql) == 0) {
		debug(LOW, "Couldn't set thread specific storage\n");
		return FALSE;
	}

	return TRUE;

}

int __db_disconnect() {
	PGconn *pgsql = getpgsql();

	debug(LOW, "Disconnecting from postgres\n");
	PQfinish(pgsql);

	/*
	 * PQfinish free()s the memory location stored in the key,
	 * so when the thread shuts down we get a double free and
	 * a warning. Setting it back to NULL avoids this.
	 */
	pthread_setspecific(key, NULL);

	return TRUE;
}


int __db_insert(char *table, int iid, unsigned long long insert_val, double insert_rate) {
	PGconn *pgsql = getpgsql();

	char *query;
	char *table_esc;

	PGresult *result;

	/* size_t PQescapeString (char *to, const char *from, size_t length); */
	/* INSERT INTO %s (id,dtime,counter,rate) VALUES (%d, NOW(), %llu, %.6f) */
	/* escape table name */
	table_esc = escape_string(table_esc, table);
				
	asprintf(&query,
		"INSERT INTO %s (id,dtime,counter,rate) VALUES (%d,NOW(),%llu,%.6f)",
		table_esc, iid, insert_val, insert_rate);

	free(table_esc);

	debug(HIGH, "Query = %s\n", query);

	result = PQexec(pgsql, query);

	free(query);

	if (PQresultStatus(result) == PGRES_COMMAND_OK) {
		/* free the result */
		(void)PQclear(result);

		return TRUE;
	} else {
		/* free the result */
		(void)PQclear(result);

		/* Note that by libpq convention, a non-empty PQerrorMessage will include a trailing newline. */
		/* also errors start with 'ERROR:' so we don't need to */
		debug(LOW, "Postgres %s", PQerrorMessage(pgsql));

		return FALSE;
	}
}


int __db_commit() {
    struct timespec ts1;
    struct timespec ts2;
    unsigned int ms_took;
    int com_ret;
    PGconn *pgsql = getpgsql();

    clock_gettime(CLOCK_REALTIME,&ts1);
    
    com_ret = pgsql_db_commit(pgsql);

    clock_gettime(CLOCK_REALTIME,&ts2);
    
    ms_took = (unsigned int)((ts2.tv_sec * 1000000000 + ts2.tv_nsec) - (ts1.tv_sec * 1000000000 + ts1.tv_nsec)) / 1000000;
    debug(HIGH,"Commit took %d milliseconds at [%d][%d][%d][%d]\n",ms_took,ts1.tv_sec,ts1.tv_nsec,ts2.tv_sec,ts2.tv_nsec);

    return com_ret;
}


#ifdef HAVE_STRTOLL
long long __db_intSpeed(char *query) {
    long long ret = 0;
#else
long __db_intSpeed(char *query) {
    long ret = 0;
#endif
    PGconn *pgsql = getpgsql();
    PGresult *result;

    result = PQexec(pgsql, query);

    if (PQresultStatus(result) == PGRES_TUPLES_OK) {
      if (1 == PQntuples(result))
	{
#ifdef HAVE_STRTOLL
	  ret = strtoll(PQgetvalue(result, 0, 0), NULL, 0);
#else
	  ret = strtol(PQgetvalue(result, 0, 0), NULL, 0);
#endif
	}
      else
	{
	  debug(LOW, "Expected 1 row result but instead got %d rows.\n", PQntuples(result));
	}

      /* free the result */
      (void)PQclear(result);
    } else {
      /* Note that by libpq convention, a non-empty PQerrorMessage will include a trailing newline. */
      /* also errors start with 'ERROR:' so we don't need to */
      debug(LOW, "Postgres %s", PQerrorMessage(pgsql));

      /* free the result */
      (void)PQclear(result);
    }

    return ret;
}


int __db_populate(char *query, data_obj_t *DO) {
    PGconn *pgsql = getpgsql();
    PGresult *result;
    data_t *new = NULL;
    data_t *last = NULL;
    data_t **data = &(DO->data);
    int i, max;

    result = PQexec(pgsql, query);

    if (PQresultStatus(result) == PGRES_TUPLES_OK &&
	2 == PQnfields(result)) {
      max = PQntuples(result);
      for (i = 0; i < max; i++)
	{
	  if ((new = (data_t *) malloc(sizeof(data_t))) == NULL)
            debug(LOW, "  Fatal malloc error in __db_populate.\n");
#ifdef HAVE_STRTOLL
	  new->counter = strtoll(PQgetvalue(result, i, 0), NULL, 0);
#else
	  new->counter = strtol(PQgetvalue(result, i, 0), NULL, 0);
#endif
	  new->timestamp = strtoul(PQgetvalue(result, i, 1), NULL, 0);
	  new->next = NULL;
	  (DO->datapoints)++;
	  if (new->counter > DO->counter_max)
            DO->counter_max = new->counter;
	  if (*data != NULL) {
            last->next = new;
            last = new;
	  } else {
            DO->dataBegin = new->timestamp;
            *data = new;
            last = new;
	  }
	}

      /* free the result */
      (void)PQclear(result);
      return TRUE;
    } else {
      /* Note that by libpq convention, a non-empty PQerrorMessage will include a trailing newline. */
      /* also errors start with 'ERROR:' so we don't need to */

      /* free the result */
      (void)PQclear(result);

      debug(LOW, "Postgres %s", PQerrorMessage(pgsql));
      return FALSE;
    }
}
