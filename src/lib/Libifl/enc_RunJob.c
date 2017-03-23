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
 * @file	enc_RunJob.c
 * @brief
 * encode_DIS_RunJob() - encode a Run Job Batch Request
 *
 * @par Data items are:
 * 			string		job id
 *			string		destination
 *			unsigned int	resource handle (currently 0)
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include "libpbs.h"
#include "pbs_error.h"
#include "dis.h"

/*
 * encode_DIS_Run - used to encode the basic information for the
 *	RunJob request and the ConfirmReservation request
 */

/**
 * @brief
 *	-used to encode the basic information for the
 *      RunJob request and the ConfirmReservation request
 *
 * @param[in] sock - soket descriptor
 * @param[in] id - reservation id
 * @param[in] where - reservation on
 * @param[in] arg - ar
 *
 * @return      int
 * @retval      DIS_SUCCESS(0)  success
 * @retval      error code      error
 *
 */

int
encode_DIS_Run(int sock, char *id, char *where, unsigned long arg)
{
	int   rc;

	if ((rc = diswst(sock, id) != 0) ||
		(rc = diswst(sock, where) != 0) ||
		(rc = diswul(sock, arg)   != 0))
			return rc;

	return 0;
}
