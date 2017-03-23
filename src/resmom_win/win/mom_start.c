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

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "libpbs.h"
#include "list_link.h"
#include "log.h"
#include "server_limits.h"
#include "attribute.h"
#include "resource.h"
#include "job.h"
#include "mom_func.h"
#include "net_connect.h"
#include "work_task.h"

/**
 * @file
 */
/* Global Variables */

extern int		exiting_tasks;
extern char		mom_host[];
extern pbs_list_head	svr_alljobs;
extern int		svr_delay_entry;
extern pbs_list_head	task_list_event;

/* Private variables */

/**
 * @brief
 *	set_globid - set the global id for a machine type.
 */
void
set_globid(job *pjob, struct startjob_rtn *sjr)
{
	return;
}

/**
 * @brief Find which shell to use, one specified or the login shell
 * Note: This function uses static array for shell, you should use it
 * immediately before any subsequent call of this function.
 * @param[in] pjob : Pointer to the job
 * @param[in] pwdp : pointer to passwd entry
 *
 * @return char *
 * @retval pointer to shell
 */
char *
set_shell(job *pjob, struct passwd *pwdp)
{
	char	*cp;
	int	i;
	static char	shell[MAX_PATH + 1] = {'\0'};
	char    *temp_dir = NULL;
	struct array_strings *vstrs;

	/*
	 * Find which shell to use, one specified or the login shell
	 * If we fail to get cmd shell(unlikely), use "cmd.exe" as shell
	 */
	if (0 != get_cmd_shell(shell, sizeof(shell)))
		(void)snprintf(shell, sizeof(shell) - 1, "cmd.exe");
	if ((pjob->ji_wattr[(int)JOB_ATR_shell].at_flags & ATR_VFLAG_SET) &&
		(vstrs = pjob->ji_wattr[(int)JOB_ATR_shell].at_val.at_arst)) {
		for (i = 0; i < vstrs->as_usedptr; ++i) {
			cp = strchr(vstrs->as_string[i], '@');
			if (cp) {
				if (!strncmp(mom_host, cp+1, strlen(cp+1))) {
					*cp = '\0';	/* host name matches */
					strncpy(shell, vstrs->as_string[i], PBS_CMDLINE_LENGTH - 1);
					break;
				}
			} else {
				strncpy(shell, vstrs->as_string[i], PBS_CMDLINE_LENGTH - 1);	/* wildcard */
			}
		}
	}
	for (i=0; shell[i] != '\0'; i++) {
		if (shell[i] == '/')
			shell[i] = '\\';
	}
	return (shell);
}

/**
 * @brief	wait_action
 *		Wait for a task that has terminated or a socket that is ready to read.
 *		Mark any terminated task as Exiting and do network processing on
 *		any ready socket.
 *
 * @return	void
 */
void
wait_action(void)
{
	static	char	id[] = "wait_action";
	int		rc = 0;
	int		hNum = 0;
	HANDLE		hArray[MAXIMUM_WAIT_OBJECTS+1] = {INVALID_HANDLE_VALUE};
	HANDLE		hProc = INVALID_HANDLE_VALUE;
	extern	HANDLE	hStop;	/* mutex: quit when released */
	int		ecode = -1;
	job		*pjob = NULL;
	task		*ptask = NULL;
	int		waittime = 500;
	extern	int	mom_run_state;
	struct work_task *p_wtask = NULL;
	HANDLE		  pid = INVALID_HANDLE_VALUE;

	/* Check for non job-related tasks like periodic hook tasks */
	while (1) {

		if ((pid = waitpid((HANDLE)-1, &ecode, WNOHANG)) == (HANDLE)-1) {
			if (errno == EINTR) {
				continue;
			} else {
				break;
			}

		} else if (pid == 0) {
			break;
		}

		p_wtask = (struct work_task *)GET_NEXT(task_list_event);
		while (p_wtask) {
			if ((p_wtask->wt_type == WORK_Deferred_Child) &&
				((HANDLE)p_wtask->wt_event == pid)) {
				p_wtask->wt_type = WORK_Deferred_Cmp;
				p_wtask->wt_aux = (int)ecode;	/* exit status */
				svr_delay_entry++;	/* see next_task() */
			}
			p_wtask = (struct work_task *)GET_NEXT(p_wtask->wt_linkall);
		}
	}

	for (;;) {
		hNum = 0;
		if (mom_run_state && hStop != NULL)	/* add mutex to array */
			hArray[hNum++] = hStop;

		pjob = (job *)GET_NEXT(svr_alljobs);
		while (pjob) {
			/*
			 * see if process was a child doing a special
			 * function for MOM
			 */
			if ((pjob->ji_momsubt != NULL) &&
				(pjob->ji_momsubt != INVALID_HANDLE_VALUE) &&
				(pjob->ji_mompost != NULL)) {
				hArray[hNum++] = pjob->ji_momsubt;
			}
				
			/*
			 * process tasks
			 */
			ptask = (task *)GET_NEXT(pjob->ji_tasks);
			while (ptask) {
				if ((ptask->ti_hProc != NULL) &&
					(ptask->ti_hProc != INVALID_HANDLE_VALUE))
					hArray[hNum++] = ptask->ti_hProc;
				if (hNum > MAXIMUM_WAIT_OBJECTS)
					break;
				ptask = (task *)GET_NEXT(ptask->ti_jobtask);
			}
			if (hNum > MAXIMUM_WAIT_OBJECTS) {
				DBPRT(("%s: %d more than MAX\n", id, hNum))
				hNum = MAXIMUM_WAIT_OBJECTS;
				break;
			}
			pjob = (job *)GET_NEXT(pjob->ji_alljobs);
		}

		if (hNum == 0)		/* nothing to wait for */
			break;

		rc = WaitForMultipleObjects(hNum, hArray,
			FALSE, waittime);
		if (rc == WAIT_TIMEOUT)	/* nobody is done */
			break;
		else if (rc == WAIT_FAILED) {
			log_err(-1, id, "WaitForMultipleObjects");
			break;
		}

		waittime = 0;		/* only wait the first time */
		rc -= WAIT_OBJECT_0;	/* which object was it? */
		assert(0 <= rc && rc < hNum);

		if (rc == 0 && mom_run_state && hStop != NULL) {		/* got mutex */
			mom_run_state = 0;				/* shutdown */
			continue;
		}
		/*
		 **	It was a process finishing.  Find which one.
		 */
		hProc = hArray[rc];

		rc = GetExitCodeProcess(hProc, &ecode);
		if (rc == 0) {
			log_err(-1, id, "GetExitCodeProcess");
			ecode = 99;
		} else if (rc == STILL_ACTIVE)	/* shouldn't happen */
			break;
		CloseHandle(hProc);

		/* find which process finished */
		pjob = (job *)GET_NEXT(svr_alljobs);
		while (pjob) {
			if (pjob->ji_momsubt == hProc)
				break;

			ptask = (task *)GET_NEXT(pjob->ji_tasks);
			while (ptask) {
				if (ptask->ti_hProc == hProc)
					break;
				ptask = (task *)GET_NEXT(ptask->ti_jobtask);
			}
			if (ptask)
				break;
			pjob = (job *)GET_NEXT(pjob->ji_alljobs);
		}
		assert(pjob != NULL);

		if (pjob->ji_momsubt == hProc) {
			pjob->ji_momsubt = NULL;
			if (pjob->ji_mompost) {
				pjob->ji_mompost(pjob, ecode);

				/* After epilogue, get rid of any HOSTFILE */
				if (pjob->ji_mompost == send_obit) {
					char	file[MAXPATHLEN+1];

					(void)sprintf(file, "%s/aux/%s",
						pbs_conf.pbs_home_path,
						pjob->ji_qs.ji_jobid);
					(void)unlink(file);
				}
				pjob->ji_mompost = 0;
			}
			(void)job_save(pjob, SAVEJOB_QUICK);
			continue;
		}

		DBPRT(("%s: task %d pid %d exit value %d\n", id,
			ptask->ti_qs.ti_task, ptask->ti_qs.ti_sid,
			ecode))
		ptask->ti_hProc = NULL;
		ptask->ti_qs.ti_exitstat = ecode;
		ptask->ti_qs.ti_status = TI_STATE_EXITED;
		ptask->ti_qs.ti_sid = 0;
		(void)task_save(ptask);
		sprintf(log_buffer, "task %d terminated", ptask->ti_qs.ti_task);
		log_event(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB, LOG_DEBUG,
			pjob->ji_qs.ji_jobid, log_buffer);

		exiting_tasks = 1;
	}

	connection_idlecheck();
}

/*
 * struct sig_tbl = map of signal names to numbers,
 * see req_signal() in ../requests.c
 */

struct sig_tbl sig_tbl[] = {
	{ "NULL", 0 },
	{ "HUP", 0 },
	{ "INT", SIGINT },
	{ "QUIT", 0 },
	{ "ILL", SIGILL },
	{ "TRAP", 0 },
	{ "IOT", 0 },
	{ "ABRT", SIGABRT },
	{ "EMT", 0 },
	{ "FPE", SIGFPE },
	{ "KILL", 0 },
	{ "BUS", 0 },
	{ "SEGV", SIGSEGV },
	{ "SYS", 0 },
	{ "PIPE", 0 },
	{ "ALRM", 0 },
	{ "TERM", SIGTERM },
	{ "USR1", 0 },
	{ "USR2", 0 },
	{ "CHLD", 0 },
	{ "PWR", 0 },
	{ "WINCH", 0 },
	{ "URG", 0 },
	{ "POLL", 0 },
	{ "IO", 0 },
	{ "STOP", 0 },
	{ "TSTP", 0 },
	{ "CONT", 0 },
	{ "TTIN", 0 },
	{ "TTOU", 0 },
	{ "VTALRM", 0 },
	{ "PROF", 0 },
	{ "XCPU", 0 },
	{ "XFSZ", 0 },
	{(char *)0, -1}
};
