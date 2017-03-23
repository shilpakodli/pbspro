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
 * @file    db_postgres_node.c
 *
 * @brief
 *      Implementation of the node data access functions for postgres
 */

#include <pbs_config.h>   /* the master config generated by configure */
#include "pbs_db.h"
#include "db_postgres.h"

/**
 * @brief
 *	Prepare all the node related sqls. Typically called after connect
 *	and before any other sql exeuction
 *
 * @param[in]	conn - Database connection handle
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 *
 */
int
pg_db_prepare_node_sqls(pbs_db_conn_t *conn)
{
	sprintf(conn->conn_sql, "insert into pbs.node("
		"nd_name, "
		"nd_index, "
		"mom_modtime, "
		"nd_hostname, "
		"nd_state, "
		"nd_ntype, "
		"nd_pque, "
		"nd_savetm, "
		"nd_creattm "
		") "
		"values "
		"($1, $2, $3, $4, $5, $6, $7, localtimestamp, localtimestamp)");

	if (pg_prepare_stmt(conn, STMT_INSERT_NODE, conn->conn_sql, 7) != 0)
		return -1;

	sprintf(conn->conn_sql, "update pbs.node set "
		"nd_index = $2, "
		"mom_modtime = $3, "
		"nd_hostname = $4, "
		"nd_state = $5, "
		"nd_ntype = $6, "
		"nd_pque = $7, "
		"nd_savetm = localtimestamp "
		" where nd_name = $1");
	if (pg_prepare_stmt(conn, STMT_UPDATE_NODE, conn->conn_sql, 7) != 0)
		return -1;

	sprintf(conn->conn_sql, "select "
		"nd_name, "
		"nd_index, "
		"mom_modtime, "
		"nd_hostname, "
		"nd_state, "
		"nd_ntype, "
		"nd_pque "
		"from pbs.node "
		"where nd_name = $1");
	if (pg_prepare_stmt(conn, STMT_SELECT_NODE, conn->conn_sql, 1) != 0)
		return -1;

	sprintf(conn->conn_sql, "insert into pbs.node_attr "
		"(nd_name, "
		"attr_name, "
		"attr_resource, "
		"attr_value, "
		"attr_flags) "
		"values ($1, $2, $3, $4, $5)");
	if (pg_prepare_stmt(conn, STMT_INSERT_NODEATTR, conn->conn_sql, 5) != 0)
		return -1;

	sprintf(conn->conn_sql, "update pbs.node_attr set "
		"attr_resource = $3, "
		"attr_value = $4, "
		"attr_flags = $5 "
		"where nd_name = $1 "
		"and attr_name = $2");
	if (pg_prepare_stmt(conn, STMT_UPDATE_NODEATTR, conn->conn_sql, 5) != 0)
		return -1;

	sprintf(conn->conn_sql, "update pbs.node_attr set "
		"attr_value = $4, "
		"attr_flags = $5 "
		"where nd_name = $1 "
		"and attr_name = $2 "
		"and attr_resource = $3");
	if (pg_prepare_stmt(conn, STMT_UPDATE_NODEATTR_RESC, conn->conn_sql, 5) != 0)
		return -1;

	sprintf(conn->conn_sql, "delete from pbs.node_attr "
		" where nd_name = $1 "
		"and attr_name = $2");
	if (pg_prepare_stmt(conn, STMT_DELETE_NODEATTR, conn->conn_sql, 2) != 0)
		return -1;

	sprintf(conn->conn_sql, "delete from pbs.node_attr "
		" where nd_name = $1 "
		"and attr_name = $2 "
		"and attr_resource = $3");
	if (pg_prepare_stmt(conn, STMT_DELETE_NODEATTR_RESC, conn->conn_sql, 3) != 0)
		return -1;

	sprintf(conn->conn_sql, "select "
		"nd_name, "
		"attr_name, "
		"attr_resource, "
		"attr_value, "
		"attr_flags from "
		"pbs.node_attr "
		"where nd_name = $1");
	if (pg_prepare_stmt(conn, STMT_SELECT_NODEATTR, conn->conn_sql, 1) != 0)
		return -1;

	sprintf(conn->conn_sql, "select "
		"nd_name, "
		"nd_index, "
		"mom_modtime, "
		"nd_hostname, "
		"nd_state, "
		"nd_ntype, "
		"nd_pque "
		"from pbs.node order by nd_creattm");
	if (pg_prepare_stmt(conn, STMT_FIND_NODES_ORDBY_CREATTM, conn->conn_sql, 0) != 0)
		return -1;

	sprintf(conn->conn_sql, "select "
#ifdef NAS /* localmod 079 */
		"n.nd_name, "
		"n.nd_index, "
		"n.mom_modtime, "
		"n.nd_hostname, "
		"n.nd_state, "
		"n.nd_ntype, "
		"n.nd_pque "
		"from pbs.node n left outer join pbs.nas_node i on "
		"n.nd_name=i.nd_name order by i.nd_nasindex");
#else
		"nd_name, "
		"nd_index, "
		"mom_modtime, "
		"nd_hostname, "
		"nd_state, "
		"nd_ntype, "
		"nd_pque "
		"from pbs.node order by nd_index, nd_creattm");
#endif /* localmod 079 */
	if (pg_prepare_stmt(conn, STMT_FIND_NODES_ORDBY_INDEX, conn->conn_sql, 0) != 0)
		return -1;

	sprintf(conn->conn_sql, "delete from pbs.node where nd_name = $1");
	if (pg_prepare_stmt(conn, STMT_DELETE_NODE, conn->conn_sql, 1) != 0)
		return -1;

	sprintf(conn->conn_sql, "delete from pbs.node");
	if (pg_prepare_stmt(conn, STMT_DELETE_ALL_NODES, conn->conn_sql, 0) != 0)
		return -1;

	sprintf(conn->conn_sql, "select "
		"mit_time, "
		"mit_gen "
		"from pbs.mominfo_time ");
	if (pg_prepare_stmt(conn, STMT_SELECT_MOMINFO_TIME, conn->conn_sql, 0) != 0)
		return -1;

	sprintf(conn->conn_sql, "insert into pbs.mominfo_time("
		"mit_time, "
		"mit_gen) "
		"values "
		"($1, $2)");
	if (pg_prepare_stmt(conn, STMT_INSERT_MOMINFO_TIME, conn->conn_sql, 2) != 0)
		return -1;

	sprintf(conn->conn_sql, "update pbs.mominfo_time set "
		"mit_time = $1, "
		"mit_gen = $2 ");
	if (pg_prepare_stmt(conn, STMT_UPDATE_MOMINFO_TIME, conn->conn_sql, 2) != 0)
		return -1;

	return 0;
}

/**
 * @brief
 *	Load node data from the row into the node object
 *
 * @param[in]	res - Resultset from a earlier query
 * @param[in]	pnd  - Node object to load data into
 * @param[in]	row - The current row to load within the resultset
 *
 *
 */
static void
load_node(PGresult *res, pbs_db_node_info_t *pnd, int row)
{
	/* get the other fields */
	strcpy(pnd->nd_name, PQgetvalue(res, row,
		PQfnumber(res, "nd_name")));
	pnd->nd_index = strtol(PQgetvalue(res, row,
		PQfnumber(res, "nd_index")), NULL, 10);
	pnd->mom_modtime = strtoll(PQgetvalue(res, row,
		PQfnumber(res, "mom_modtime")), NULL, 10);
	strcpy(pnd->nd_hostname, PQgetvalue(res, row,
		PQfnumber(res, "nd_hostname")));
	pnd->nd_state = strtol(PQgetvalue(res, row,
		PQfnumber(res, "nd_state")), NULL, 10);
	pnd->nd_ntype = strtol(PQgetvalue(res, row,
		PQfnumber(res, "nd_ntype")), NULL, 10);
	strcpy(pnd->nd_pque, PQgetvalue(res, row,
		PQfnumber(res, "nd_pque")));
}

/**
 * @brief
 *	Insert node data into the database
 *
 * @param[in]	conn - Connection handle
 * @param[in]	obj  - Information of node to be inserted
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 *
 */
int
pg_db_insert_node(pbs_db_conn_t *conn, pbs_db_obj_info_t *obj)
{
	pbs_db_node_info_t *pnd = obj->pbs_db_un.pbs_db_node;

	LOAD_STR(conn, pnd->nd_name, 0);
	LOAD_INTEGER(conn, pnd->nd_index, 1);
	LOAD_BIGINT(conn, pnd->mom_modtime, 2);
	LOAD_STR(conn, pnd->nd_hostname, 3);
	LOAD_INTEGER(conn, pnd->nd_state, 4);
	LOAD_INTEGER(conn, pnd->nd_ntype, 5);
	LOAD_STR(conn, pnd->nd_pque, 6);

	if (pg_db_cmd(conn, STMT_INSERT_NODE, 7) != 0)
		return -1;

	return 0;
}

/**
 * @brief
 *	Update node data into the database
 *
 * @param[in]	conn - Connection handle
 * @param[in]	obj  - Information of node to be updated
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 * @retval	 1 - Success but no rows updated
 *
 */
int
pg_db_update_node(pbs_db_conn_t *conn, pbs_db_obj_info_t *obj)
{
	pbs_db_node_info_t *pnd = obj->pbs_db_un.pbs_db_node;

	LOAD_STR(conn, pnd->nd_name, 0);
	LOAD_INTEGER(conn, pnd->nd_index, 1);
	LOAD_BIGINT(conn, pnd->mom_modtime, 2);
	LOAD_STR(conn, pnd->nd_hostname, 3);
	LOAD_INTEGER(conn, pnd->nd_state, 4);
	LOAD_INTEGER(conn, pnd->nd_ntype, 5);
	LOAD_STR(conn, pnd->nd_pque, 6);

	return (pg_db_cmd(conn, STMT_UPDATE_NODE, 7));
}

/**
 * @brief
 *	Load node data from the database
 *
 * @param[in]	conn - Connection handle
 * @param[in]	obj  - Load node information into this object
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 * @retval	 1 -  Success but no rows loaded
 *
 */
int
pg_db_load_node(pbs_db_conn_t *conn, pbs_db_obj_info_t *obj)
{
	PGresult *res;
	int rc;
	pbs_db_node_info_t *pnd = obj->pbs_db_un.pbs_db_node;

	LOAD_STR(conn, pnd->nd_name, 0);

	if ((rc = pg_db_query(conn, STMT_SELECT_NODE, 1, &res)) != 0)
		return rc;

	load_node(res, pnd, 0);

	PQclear(res);
	return 0;
}

/**
 * @brief
 *	Find nodes
 *
 * @param[in]	conn - Connection handle
 * @param[in]	st   - The cursor state variable updated by this query
 * @param[in]	obj  - Information of node to be found
 * @param[in]	opts - Any other options (like flags, timestamp)
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 * @retval	 1 - Success, but no rows found
 *
 */
int
pg_db_find_node(pbs_db_conn_t *conn, void *st, pbs_db_obj_info_t *obj,
	pbs_db_query_options_t *opts)
{
	PGresult *res;
	int rc;
	pg_query_state_t *state = (pg_query_state_t *) st;

	if (!state)
		return -1;

	if ((rc = pg_db_query(conn, STMT_FIND_NODES_ORDBY_INDEX, 0, &res)) != 0)
		return rc;

	state->row = 0;
	state->res = res;
	state->count = PQntuples(res);
	return 0;
}

/**
 * @brief
 *	Get the next node from the cursor
 *
 * @param[in]	conn - Connection handle
 * @param[in]	st   - The cursor state
 * @param[in]	obj  - Node information is loaded into this object
 *
 * @return      Error code
 *		(Even though this returns only 0 now, keeping it as int
 *			to support future change to return a failure)
 * @retval	 0 - Success
 *
 */
int
pg_db_next_node(pbs_db_conn_t *conn, void *st, pbs_db_obj_info_t *obj)
{
	PGresult *res = ((pg_query_state_t *) st)->res;
	pg_query_state_t *state = (pg_query_state_t *) st;

	load_node(res, obj->pbs_db_un.pbs_db_node, state->row);
	return 0;
}

/**
 * @brief
 *	Delete the node from the database
 *
 * @param[in]	conn - Connection handle
 * @param[in]	obj  - Node information
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 * @retval	 1 - Success but no rows deleted
 *
 */
int
pg_db_delete_node(pbs_db_conn_t *conn, pbs_db_obj_info_t *obj)
{
	pbs_db_node_info_t *pnd = obj->pbs_db_un.pbs_db_node;
	LOAD_STR(conn, pnd->nd_name, 0);
	return (pg_db_cmd(conn, STMT_DELETE_NODE, 1));
}

/**
 * @brief
 *	Insert mominfo_time into the database
 *
 * @param[in]	conn - Connection handle
 * @param[in]	obj  - Information of node to be inserted
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 *
 */
int
pg_db_insert_mominfo_tm(pbs_db_conn_t *conn, pbs_db_obj_info_t *obj)
{
	pbs_db_mominfo_time_t *pmi = obj->pbs_db_un.pbs_db_mominfo_tm;

	LOAD_BIGINT(conn, pmi->mit_time, 0);
	LOAD_INTEGER(conn, pmi->mit_gen, 1);
	if (pg_db_cmd(conn, STMT_INSERT_MOMINFO_TIME, 2) != 0)
		return -1;

	return 0;
}

/**
 * @brief
 *	Update mominfo_time into the database
 *
 * @param[in]	conn - Connection handle
 * @param[in]	obj  - Information of node to be updated
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 * @retval	 1 - Success but no rows updated
 *
 */
int
pg_db_update_mominfo_tm(pbs_db_conn_t *conn, pbs_db_obj_info_t *obj)
{
	pbs_db_mominfo_time_t *pmi = obj->pbs_db_un.pbs_db_mominfo_tm;

	LOAD_BIGINT(conn, pmi->mit_time, 0);
	LOAD_INTEGER(conn, pmi->mit_gen, 1);

	return (pg_db_cmd(conn, STMT_UPDATE_MOMINFO_TIME, 2));
}

/**
 * @brief
 *	Load node mominfo_time from the database
 *
 * @param[in]	conn - Connection handle
 * @param[in]	obj  - Load node information into this object
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 * @retval	 1 -  Success but no rows loaded
 *
 */
int
pg_db_load_mominfo_tm(pbs_db_conn_t *conn, pbs_db_obj_info_t *obj)
{
	PGresult *res;
	int rc;
	pbs_db_mominfo_time_t *pmi = obj->pbs_db_un.pbs_db_mominfo_tm;

	if ((rc = pg_db_query(conn, STMT_SELECT_MOMINFO_TIME, 0, &res)) != 0)
		return rc;

	pmi->mit_time = strtoll(PQgetvalue(res, 0,
		PQfnumber(res, "mit_time")), NULL, 10);
	pmi->mit_gen = strtol(PQgetvalue(res, 0,
		PQfnumber(res, "mit_gen")), NULL, 10);

	PQclear(res);
	return 0;
}
