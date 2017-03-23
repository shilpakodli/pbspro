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
 * @file	int_status2.c
 * @brief
 * The function that sends the general batch status request
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <string.h>
#include <stdio.h>
#include "libpbs.h"
#include "dis.h"
#include "net_connect.h"
#include "rpp.h"


/**
 * @brief
 *	send status job batch request
 *
 * @param[in] c - socket descriptor
 * @param[in] function - request type
 * @param[in] id - object id
 * @param[in] attrib - pointer to attribute list
 * @param[in] extend - extention string for req encode
 * @param[in] rpp - indication for rpp protocol
 * @param[in] msgid - message id
 *
 * @return      int
 * @retval      0               Success
 * @retval      pbs_error(!0)   error
 */

int
PBSD_status_put(int c, int function, char *id, struct attrl *attrib, 
		char *extend, int rpp, char **msgid)
{
	int rc = 0;
	int sock;

	if (!rpp) {
		sock = connection[c].ch_socket;
		DIS_tcp_setup(sock);
	} else {
		sock = c;
		if ((rc = is_compose_cmd(sock, IS_CMD, msgid)) != DIS_SUCCESS)
			return rc;
	}
	if ((rc = encode_DIS_ReqHdr(sock, function, pbs_current_user))   ||
		(rc = encode_DIS_Status(sock, id, attrib)) ||
		(rc = encode_DIS_ReqExtend(sock, extend))) {
		if (!rpp) {
			connection[c].ch_errtxt = strdup(dis_emsg[rc]);
			if (connection[c].ch_errtxt == NULL)
				return (pbs_errno = PBSE_SYSTEM);
		}
		return (pbs_errno = PBSE_PROTOCOL);
	}
	if (DIS_wflush(sock, rpp)) {
		return (pbs_errno = PBSE_PROTOCOL);
	}

	return 0;
}
