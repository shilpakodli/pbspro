/*
 * Copyright (C) 1994-2017 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *  
 * This file is part of the PBS Professional ("PBS Pro") software.
 * 
 * Open Source License Information:
 *  
 * PBS Pro is free software. You can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free 
 * Software Foundation, either version 3 of the License, or (at your option) any 
 * later version.
 *  
 * PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.
 *  
 * You should have received a copy of the GNU Affero General Public License along 
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 * Commercial License Information: 
 * 
 * The PBS Pro software is licensed under the terms of the GNU Affero General 
 * Public License agreement ("AGPL"), except where a separate commercial license 
 * agreement for PBS Pro version 14 or later has been executed in writing with Altair.
 *  
 * Altair’s dual-license business model allows companies, individuals, and 
 * organizations to create proprietary derivative works of PBS Pro and distribute 
 * them - whether embedded or bundled with other software - under a commercial 
 * license agreement.
 * 
 * Use of Altair’s trademarks, including but not limited to "PBS™", 
 * "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's 
 * trademark licensing policies.
 *
 */


/**
 * @file    db_postgres_impl.c
 *
 * @brief
 * This file contains Postgres specific implementation of functions
 * to access the PBS postgres database. This is postgres specific data store
 * implementation, and should not be used directly by the rest of the PBS code.
 *
 */

#include <pbs_config.h>   /* the master config generated by configure */
#include "pbs_db.h"
#include "db_postgres.h"

#define PBS_DB_CONN_RETRY_TIME 30 /* after waiting for this amount of seconds, connection is retried */


/**
 * An array of structures(of function pointers) for each of the database object
 */
pg_db_fn_t db_fn_arr[PBS_DB_NUM_TYPES] =
	{
	{ /* PBS_DB_JOB */
		pg_db_insert_job,
		pg_db_update_job,
		pg_db_delete_job,
		pg_db_load_job,
		pg_db_find_job,
		pg_db_next_job
	},
	{ /* PBS_DB_RESV */
		pg_db_insert_resv,
		pg_db_update_resv,
		pg_db_delete_resv,
		pg_db_load_resv,
		pg_db_find_resv,
		pg_db_next_resv
	},
	{ /* PBS_DB_SVR */
		pg_db_insert_svr,
		pg_db_update_svr,
		NULL,
		pg_db_load_svr,
		NULL,
		NULL
	},
	{ /* PBS_DB_NODE */
		pg_db_insert_node,
		pg_db_update_node,
		pg_db_delete_node,
		pg_db_load_node,
		pg_db_find_node,
		pg_db_next_node
	},
	{ /* PBS_DB_QUE */
		pg_db_insert_que,
		pg_db_update_que,
		pg_db_delete_que,
		pg_db_load_que,
		pg_db_find_que,
		pg_db_next_que
	},
	{ /* PBS_DB_ATTR */
		pg_db_insert_attr,
		pg_db_update_attr,
		pg_db_delete_attr,
		pg_db_load_attr,
		pg_db_find_attr,
		pg_db_next_attr
	},
	{ /* PBS_DB_JOBSCR */
		pg_db_insert_jobscr,
		NULL,
		NULL,
		pg_db_load_jobscr,
		NULL,
		NULL
	},
	{ /* PBS_DB_SCHED */
		pg_db_insert_sched,
		pg_db_update_sched,
		NULL,
		pg_db_load_sched,
		NULL,
		NULL
	},
	{ /* PBS_DB_SUBJOB */
		pg_db_insert_subjob,
		pg_db_update_subjob,
		NULL,
		NULL,
		pg_db_find_subjob,
		pg_db_next_subjob
	},
	{ /* PBS_DB_MOMINFO_TIME */
		pg_db_insert_mominfo_tm,
		pg_db_update_mominfo_tm,
		NULL,
		pg_db_load_mominfo_tm,
		NULL,
		NULL
	}
};

/**
 * @brief
 *	Initialize a query state variable, before being used in a cursor
 *
 * @param[in]	conn - Database connection handle
 *
 * @return	Pointer to opaque cursor state handle
 * @retval	NULL - Failure to allocate memory
 * @retval	!NULL - Success - returns the new state variable
 *
 */
static void *
pg_initialize_state(pbs_db_conn_t *conn)
{
	pg_query_state_t *state = malloc(sizeof(pg_query_state_t));
	if (!state)
		return NULL;
	state->count = -1;
	state->res = NULL;
	state->row = -1;
	return state;
}

/**
 * @brief
 *	Destroy a query state variable.
 *	Clears the database resultset and free's the memory allocated to
 *	the state variable
 *
 * @param[in]	st - Pointer to the state variable
 *
 */
static void
pg_destroy_state(void *st)
{
	pg_query_state_t *state = st;
	if (state) {
		if (state->res)
			PQclear(state->res);
		free(state);
	}
}

/**
 * @brief
 *	Initialize a multirow database cursor
 *
 * @param[in]	conn - Connected database handle
 * @param[in]	pbs_db_obj_info_t - The pointer to the wrapper object which
 *		describes the PBS object (job/resv/node etc) that is wrapped
 *		inside it.
 * @param[in]	pbs_db_query_options_t - Pointer to the options object that can
 *		contain the flags or timestamp which will effect the query.
 *
 * @return	void *
 * @retval	Not NULL  - success. Returns the opaque cursor state handle
 * @retval	NULL	   - Failure
 *
 */
void*
pbs_db_cursor_init(pbs_db_conn_t *conn, pbs_db_obj_info_t *obj,
	pbs_db_query_options_t *opts)
{
	void *st;
	int ret;

	st = pg_initialize_state(conn);
	if (!st)
		return st;

	ret = db_fn_arr[obj->pbs_db_obj_type].pg_db_find_obj(
		conn, st, obj, opts);
	if (ret == -1) { /* error in executing the sql */
		pg_destroy_state(st);
		return NULL;
	}
	return st;
}

/**
 * @brief
 *	Get the next row from the cursor. It also is used to get the first row
 *	from the cursor as well.
 *
 * @param[in]	conn - Connected database handle
 * @param[in]	state - The cursor state handle that was obtained using the
 *		pbs_db_cursor_init call.
 * @param[out]	pbs_db_obj_info_t - The pointer to the wrapper object which
 *		describes the PBS object (job/resv/node etc) that is wrapped
 *		inside it. The row data is loaded into this parameter.
 *
 * @return      Error code
 * @retval	-1  - Failure
 * @retval	0  - success
 * @retval	1  - Success but no more rows
 *
 */
int
pbs_db_cursor_next(pbs_db_conn_t *conn, void *st, pbs_db_obj_info_t *obj)
{
	pg_query_state_t *state = (pg_query_state_t *) st;
	int ret;

	if (state->row < state->count) {
		ret = db_fn_arr[obj->pbs_db_obj_type].pg_db_next_obj(conn,
			st, obj);
		state->row++;
		return ret;
	}
	return 1; /* no more rows */
}

/**
 * @brief
 *	Close a cursor that was earlier opened using a pbs_db_cursor_init call.
 *
 * @param[in]	conn - Connected database handle
 * @param[in]	state - The cursor state handle that was obtained using the
 *		pbs_db_cursor_init call.
 *
 */
void
pbs_db_cursor_close(pbs_db_conn_t *conn, void *state)
{
	pg_destroy_state(state);
}

/**
 * @brief
 *	Insert a new object into the database
 *
 * @param[in]	conn - Connected database handle
 * @param[in]	pbs_db_obj_info_t - Wrapper object that describes the object
 *		(and data) to insert
 *
 * @return      Error code
 * @retval	-1  - Failure
 * @retval       0  - success
 * @retval	 1 -  Success but no rows inserted
 *
 */
int
pbs_db_insert_obj(pbs_db_conn_t *conn, pbs_db_obj_info_t *obj)
{
	return (db_fn_arr[obj->pbs_db_obj_type].pg_db_insert_obj(conn, obj));
}

/**
 * @brief
 *	Update an existing object into the database
 *
 * @param[in]	conn - Connected database handle
 * @param[in]	pbs_db_obj_info_t - Wrapper object that describes the object
 *		(and data) to update
 *
 * @return      Error code
 * @retval	-1  - Failure
 * @retval       0  - success
 * @retval	 1 -  Success but no rows updated
 *
 */
int
pbs_db_update_obj(pbs_db_conn_t *conn, pbs_db_obj_info_t *obj)
{
	return (db_fn_arr[obj->pbs_db_obj_type].pg_db_update_obj(conn, obj));
}

/**
 * @brief
 *	Delete an existing object from the database
 *
 * @param[in]	conn - Connected database handle
 * @param[in]	pbs_db_obj_info_t - Wrapper object that describes the object
 *		(and data) to delete
 *
 * @return      int
 * @retval	-1  - Failure
 * @retval       0  - success
 * @retval	 1 -  Success but no rows deleted
 *
 */
int
pbs_db_delete_obj(pbs_db_conn_t *conn, pbs_db_obj_info_t *obj)
{
	return (db_fn_arr[obj->pbs_db_obj_type].pg_db_delete_obj(conn, obj));
}

/**
 * @brief
 *	Load a single existing object from the database
 *
 * @param[in]	conn - Connected database handle
 * @param[in/out]pbs_db_obj_info_t - Wrapper object that describes the object
 *		(and data) to load. This parameter used to return the data about
 *		the object loaded
 *
 * @return      Error code
 * @retval       0  - success
 * @retval	-1  - Failure
 * @retval	 1 -  Success but no rows loaded
 *
 */
int
pbs_db_load_obj(pbs_db_conn_t *conn, pbs_db_obj_info_t *obj)
{
	return (db_fn_arr[obj->pbs_db_obj_type].pg_db_load_obj(conn, obj));
}

/**
 * @brief
 *	Cleans up memory associated with a resultset (tht was returned from a
 *	call to a query)
 *
 * @param[in]	conn - Connected database handle
 *
 */
void
pbs_db_cleanup_resultset(pbs_db_conn_t *conn)
{
	PGresult *res = conn->conn_resultset;
	PQclear(res);
}

/**
 * @brief
 *	Get the number of rows from a cursor
 *
 * @param[in]	state - The opaque cursor state handle
 *
 * @return      Numer of rows in the cursor
 * @retval       0 or positive - Number of rows in cursor
 * @retval	-1  - Failure
 *
 *
 */
int
pbs_db_get_rowcount(void *st)
{
	pg_query_state_t *state = (pg_query_state_t *) st;
	if (state)
		return state->count;
	return -1;
}

/**
 * @brief
 *	Initializes all the sqls before they can be used
 *
 * @param[in]   conn - Connected database handle
 *
 * @return      Error code
 * @retval       0  - success
 * @retval	-1  - Failure
 *
 *
 */
int
pbs_db_prepare_sqls(pbs_db_conn_t *conn)
{
	if (pg_db_prepare_job_sqls(conn) != 0)
		return -1;
	if (pg_db_prepare_svr_sqls(conn) != 0)
		return -1;
	if (pg_db_prepare_que_sqls(conn) != 0)
		return -1;
	if (pg_db_prepare_resv_sqls(conn) != 0)
		return -1;
	if (pg_db_prepare_node_sqls(conn) != 0)
		return -1;
	if (pg_db_prepare_sched_sqls(conn) != 0)
		return -1;
	return 0;
}

/**
 * @brief
 *	Start a database transaction
 *	If a transaction is already on, just increment the transactioin nest
 *	count in the database handle object
 *
 * @param[in]	conn - Connected database handle
 * @param[in]	isolation_level - Isolation level to set for the transaction
 * @param[in]	async - Set synchronous/asynchronous commit behavior
 *
 * @return      Error code
 * @retval       0  - success
 * @retval	-1  - Failure
 *
 */
int
pbs_db_begin_trx(pbs_db_conn_t *conn, int isolation_level, int async)
{
	PGresult *res;

	if (conn->conn_trx_nest == 0) {
		res = PQexec((PGconn *) conn->conn_db_handle, "BEGIN");
		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			pg_set_error(conn, "Transaction", "begin");
			PQclear(res);
			return -1;
		}
		PQclear(res);

		if (async == 1) {
			sprintf(conn->conn_sql,
				"SET LOCAL synchronous_commit TO OFF");

			res = PQexec((PGconn *) conn->conn_db_handle,
				conn->conn_sql);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) {
				pg_set_error(conn, "Transaction", conn->conn_sql);
				PQclear(res);
				return -1;
			}
			PQclear(res);
		}
		conn->conn_trx_rollback = 0; /* reset rollback flag at toplevel */
	}
	conn->conn_trx_nest++;
	return 0;
}


/**
 * @brief
 *	End a database transaction
 *	Decrement the transaction nest count in the connection object. If the
 *	count reaches zero, then end the database transaction.
 *
 * @param[in]	conn - Connected database handle
 * @param[in]	commit - If commit is PBS_DB_COMMIT, then the transaction is
 *			 commited. If commi tis PBS_DB_ROLLBACK, then the
 *			 transaction is rolled back.
 *
 * @return      Error code
 * @retval       0  - success
 * @retval	-1  - Failure
 * @retval	-2  - An inner level transaction had called rollback
 *
 */
int
pbs_db_end_trx(pbs_db_conn_t *conn, int commit)
{
	char str[10] = "END";
	PGresult *res;
	int	rc = 0;

	if (conn->conn_trx_nest == 0)
		return 0;

	if (conn->conn_trx_rollback == 1)
		rc = -2;

	if (conn->conn_trx_nest == 1) { /* last one */
		if (commit == PBS_DB_ROLLBACK || conn->conn_trx_rollback == 1)
			strcpy(str, "ROLLBACK");

		res = PQexec((PGconn *) conn->conn_db_handle, str);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			pg_set_error(conn, "Transaction", str);
			PQclear(res);
			conn->conn_trx_nest--;
			return -1;
		}
		PQclear(res);
		conn->conn_trx_rollback = 0;
	} else {
		if (commit == PBS_DB_ROLLBACK)
			conn->conn_trx_rollback = 1; /* mark transaction to rollback */
	}
	conn->conn_trx_nest--;

	return rc;
}

/**
 * @brief
 *	Execute a direct sql string on the open database connection
 *
 * @param[in]	conn - Connected database handle
 * @param[in]	sql  - A string describing the sql to execute.
 *
 * @return      Error code
 * @retval	-1  - Error
 * @retval       0  - success
 * @retval	 1  - Execution succeeded but statement did not return any rows
 *
 */
int
pbs_db_execute_str(pbs_db_conn_t *conn, char *sql)
{
	PGresult *res;
	char *rows_affected = NULL;
	int status;

	res = PQexec((PGconn*) conn->conn_db_handle, sql);
	status = PQresultStatus(res);
	if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
		pg_set_error(conn, "Execution of string statement\n", sql);
		PQclear(res);
		return -1;
	}
	rows_affected = PQcmdTuples(res);
	if ((rows_affected == NULL || strtol(rows_affected, NULL, 10) <=0)
		&& (PQntuples(res) <= 0)) {
		PQclear(res);
		return 1;
	}

	PQclear(res);
	return 0;
}

/**
 * @brief
 *	Check whether connection to pbs dataservice is fine
 *
 * @param[in]	conn - Connected database handle
 *
 * @return      Connection status
 * @retval      -1 - Connection down
 * @retval	 0 - Connection fine
 *
 */
int
pbs_db_is_conn_ok(pbs_db_conn_t *conn)
{
	if (PQstatus((PGconn*) conn->conn_db_handle) != CONNECTION_OK)
		return -1;
	return 0;
}

/**
 * @brief
 *	Static helper function to retrieve postgres error string,
 *	analyze it and find out what kind of error it is. Based on
 *	that, a PBS DB layer specific error code is generated.
 *
 * @param[in]	conn - Connected database handle
 *
 * @return      Connection status
 * @retval      -1 - Connection down
 * @retval       0 - Connection fine
 *
 */
static int
is_conn_error(pbs_db_conn_t *conn, int *failcode)
{
	/* Check to see that the backend connection was successfully made */
	if (PQstatus(conn->conn_db_handle) == CONNECTION_BAD) {
		pg_set_error(conn, "Connection:", "");
		if (strstr((char *) conn->conn_db_err, "Connection refused") || strstr((char *) conn->conn_db_err, "No such file or directory"))
			*failcode = PBS_DB_CONNREFUSED;
		else if (strstr((char *) conn->conn_db_err, "authentication"))
			*failcode = PBS_DB_AUTH_FAILED;
		else if (strstr((char *) conn->conn_db_err, "database system is starting up"))
			*failcode = PBS_DB_STILL_STARTING;
		else
			*failcode = PBS_DB_CONNFAILED; /* default failure code */

		return 1; /* true - connection error */
	}
	return 0; /* no connection error */
}

/**
 * @brief
 *	Create a new connection structure and initialize the fields
 *
 * @param[in]   host - The hostname to connect to
 * @param[in]   timeout - The connection attempt timeout
 * @param[in]   have_db_control - Do we have db instance control
 * @param[out]  failcode - Output failure code if any
 * @param[out]  errmsg	 - Details of error
 * @param[in]   len	 - length of error messge variable
 *
 * @return      Pointer to initialized connection structure
 * @retval      NULL  - Failure
 * @retval      !NULL - Success
 *
 */
pbs_db_conn_t *
pbs_db_init_connection(char * host, int timeout, int have_db_control, int *failcode, char *errmsg, int len)
{
	pbs_db_conn_t *conn;
	/*
	 * calloc ensures that everything is initialized to zeros
	 * so no need to explicitly set fields to 0.
	 */
	conn = calloc(1, sizeof(pbs_db_conn_t));
	if (!conn) {
		*failcode = PBS_DB_NOMEM;
		return NULL;
	}

	conn->conn_data = malloc(sizeof(pg_conn_data_t));
	if (!conn->conn_data) {
		free(conn);
		conn = NULL;
		*failcode = PBS_DB_NOMEM;
		return NULL;
	}

	if (host) {
		conn->conn_host = strdup(host);
		if (!conn->conn_host) {
			free(conn->conn_data);
			free(conn);
			conn = NULL;
			*failcode = PBS_DB_NOMEM;
			return NULL;
		}
	}
	conn->conn_state = PBS_DB_CONNECT_STATE_NOT_CONNECTED;
	conn->conn_timeout = timeout;
	conn->conn_have_db_control = have_db_control;

	if (have_db_control == 0)
		conn->conn_db_state = PBS_DB_STARTED; /* assume database to be up since we dont have control */
	else
		conn->conn_db_state = PBS_DB_DOWN; /* assume database to be down to start with */

	conn->conn_result_format = 0; /* default result format is TEXT */
	conn->conn_info = pbs_get_connect_string(host, conn->conn_timeout, failcode, errmsg, len);
	if (!conn->conn_info) {
		free(conn->conn_data);
		if (conn->conn_host)
			free(conn->conn_host);
		free(conn);
		conn = NULL;
		return NULL;
	}
	return conn;
}

/**
 * @brief
 *	Connect to the database
 *
 * @param[in]   conn - Previously initialized connection structure
 *
 * @return      Error code
 * @retval       PBS_DB_SUCCESS - Successful connect
 * @retval      !PBS_DB_SUCCESS - Failure code
 *
 */
int
pbs_db_connect(pbs_db_conn_t *conn)
{
	int failcode = PBS_DB_SUCCESS;

	/* Make a connection to the database */
	conn->conn_db_handle = (void *) PQconnectdb(conn->conn_info);

	/* Check to see that the backend connection was successfully made */
	if (!(is_conn_error(conn, &failcode)))
		conn->conn_state = PBS_DB_CONNECT_STATE_CONNECTED;

	return failcode;
}

/**
 * @brief
 *	Connect to the database asynchronously. This
 *  function needs to be called repeatedly till a connection
 *  success or failure happens.
 *
 * @param[in]   conn   - Previously initialized connection structure
 *
 * @return      Failure code
 * @retval       PBS_DB_SUCCESS - Success
 * @retval      !PBS_DB_SUCCESS - Database error code
 *
 */
int
pbs_db_connect_async(pbs_db_conn_t *conn)
{
	struct timeval tv;
	fd_set set;
	fd_set err_set;
	int sock;
	int failcode = PBS_DB_SUCCESS;
	int rc;

	tv.tv_sec = 0;
	tv.tv_usec = 100; /* 100 usec timeout in select, so as not to burn the CPU */

	switch (conn->conn_state) {
		case PBS_DB_CONNECT_STATE_NOT_CONNECTED: /* start connection */
			/* Initiate a connection to the database */
			conn->conn_db_handle = (void *) PQconnectStart(conn->conn_info);
			if (is_conn_error(conn, &failcode)) {
				conn->conn_state = PBS_DB_CONNECT_STATE_FAILED;
				return failcode;
			}

			conn->conn_connect_time = time(0); /* connection initiated now */
			conn->conn_state = PBS_DB_CONNECT_STATE_CONNECTING;
			conn->conn_internal_state = PGRES_POLLING_WRITING;

			/* fall into the next case - no break */

		case PBS_DB_CONNECT_STATE_CONNECTING:

			/* Check for connection status */
			if (is_conn_error(conn, &failcode)) {
				conn->conn_state = PBS_DB_CONNECT_STATE_FAILED;
				return failcode;
			}

			/* get the database connection socket fd */
			sock = PQsocket(conn->conn_db_handle);
			if (sock == -1) {
				conn->conn_state = PBS_DB_CONNECT_STATE_FAILED;
				return failcode;
			}
			FD_ZERO(&set);
			FD_SET(sock, &set);
			FD_ZERO(&err_set);
			FD_SET(sock, &err_set);

			/* connect start has been done, now poll */
			switch (conn->conn_internal_state) {
				case PGRES_POLLING_ACTIVE:
					break;

				case PGRES_POLLING_WRITING:
					rc = select(sock + 1, NULL, &set, &err_set, &tv);
					if (rc == 1) {
						/* ready for writing */
						conn->conn_internal_state = PQconnectPoll(conn->conn_db_handle);
						break; /* get out of switch here, else fall into timeout check */
					}
					/* socket err, err_set, or timeout */
					if (rc == -1 || time(0) - conn->conn_connect_time > PBS_DB_CONN_RETRY_TIME) {
						conn->conn_state = PBS_DB_CONNECT_STATE_FAILED;
						failcode = 1;
					}
					break;

				case PGRES_POLLING_READING:
					rc = select(sock + 1, &set, NULL, &err_set, &tv);
					if (rc == 1) {
						/* ready for reading */
						conn->conn_internal_state = PQconnectPoll(conn->conn_db_handle);
						break; /* get out of switch here, else fall into timeout check */
					}
					/* socket err, err_set, or timeout */
					if (rc == -1 || time(0) - conn->conn_connect_time > PBS_DB_CONN_RETRY_TIME) {
						conn->conn_state = PBS_DB_CONNECT_STATE_FAILED;
						failcode = 1;
					}
					break;

				case PGRES_POLLING_OK:
					conn->conn_state = PBS_DB_CONNECT_STATE_CONNECTED;
					break;

				case PGRES_POLLING_FAILED:
					is_conn_error(conn, &failcode);
					conn->conn_state = PBS_DB_CONNECT_STATE_FAILED;
			}
			break;
	}

	return failcode;
}

/**
 * @brief
 *	Disconnect from the database and frees all allocated memory.
 *
 * @param[in]   conn - Connected database handle
 *
 * @return      Error code
 * @retval       0  - success
 * @retval      -1  - Failure
 *
 */
void
pbs_db_disconnect(pbs_db_conn_t *conn)
{
	if (!conn)
		return;

	if (conn->conn_db_handle && conn->conn_state != PBS_DB_CONNECT_STATE_NOT_CONNECTED) {
		PQfinish(conn->conn_db_handle);
		conn->conn_db_handle = NULL;
	}
	conn->conn_state = PBS_DB_CONNECT_STATE_NOT_CONNECTED;

	return;
}

/**
 * @brief
 *      Destroys a previously created connection structure
 *      and frees all memory associated with it.
 *
 * @param[in]   conn - Previously initialized connection structure
 *
 */
void
pbs_db_destroy_connection(pbs_db_conn_t *conn)
{
	if (!conn)
		return;

	if (conn->conn_db_err)
		free(conn->conn_db_err);

	if (conn->conn_info)
		free(conn->conn_info);

	if (conn->conn_host)
		free(conn->conn_host);

	if (conn->conn_data)
		free(conn->conn_data);

	pbs_db_disconnect(conn);

	free(conn);

	return;
}

