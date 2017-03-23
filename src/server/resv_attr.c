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

#include <pbs_config.h>   /* the master config generated by configure */
#include <fcntl.h>
#include <sys/types.h>

#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "job.h"
#include "reservation.h"

/**
 * @file    resv_attr.c
 *
 * @brief
 * 		resv_attr.c	-	This table gives the correspondence between job attributes
 * 		and reservation attributes, if such correspondence exists.
 * 		It is useful in the code that adds a resc_resv structure
 * 		to a reservation job
 * @par
 * 		It should be the case that for each particular correspondence
 * 		the attribute type is the same - things wouldn't work if
 * 		this weren't the case
 */
int     index_atrJob_to_atrResv [][2] = {
        {RESV_ATR_resv_name  , JOB_ATR_jobname},
        {RESV_ATR_resv_owner , JOB_ATR_job_owner},
        {RESV_ATR_state      , JOB_ATR_reserve_state},
        {RESV_ATR_reserve_Tag, JOB_ATR_reserve_Tag},
        {RESV_ATR_reserveID  , JOB_ATR_reserve_ID},
        {RESV_ATR_start      , JOB_ATR_reserve_start},
        {RESV_ATR_end        , JOB_ATR_reserve_end},
        {RESV_ATR_duration   , JOB_ATR_reserve_duration},
        {RESV_ATR_queue      , JOB_ATR_in_queue},
        {RESV_ATR_resource   , JOB_ATR_resource},
        {RESV_ATR_resc_used  , JOB_ATR_resc_used},
        {RESV_ATR_userlst    , JOB_ATR_userlst},
        {RESV_ATR_grouplst   , JOB_ATR_grouplst},
        {RESV_ATR_at_server  , JOB_ATR_at_server},
        {RESV_ATR_account    , JOB_ATR_account},
        {RESV_ATR_ctime      , JOB_ATR_ctime},
        {RESV_ATR_mailpnts   , JOB_ATR_mailpnts},
        {RESV_ATR_mailuser   , JOB_ATR_mailuser},
        {RESV_ATR_mtime      , JOB_ATR_mtime},
        {RESV_ATR_hashname   , JOB_ATR_hashname},
        {RESV_ATR_hopcount   , JOB_ATR_hopcount},
        {RESV_ATR_priority   , JOB_ATR_priority},
        {RESV_ATR_UNKN       , JOB_ATR_UNKN},
        {RESV_ATR_LAST       , JOB_ATR_LAST}
};

