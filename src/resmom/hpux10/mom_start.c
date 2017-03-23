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

#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include "libpbs.h"
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "list_link.h"
#include "log.h"
#include "server_limits.h"
#include "attribute.h"
#include "resource.h"
#include "job.h"
#include "mom_func.h"
#include "libutil.h"
#include "work_task.h"

/**
 * @file
 */
/* Global Variables */

extern int	 exiting_tasks;
extern char	 mom_host[];
extern pbs_list_head svr_alljobs;
extern int	 termin_child;
extern  int             svr_delay_entry;
extern  pbs_list_head       task_list_event;

/* Private variables */

/**
 * @brief
 *      Set session id and whatever else is required on this machine
 *      to create a new job.
 *      On a Cray, an ALPS reservation will be created and confirmed.
 *
 * @param[in]   pjob - pointer to job structure
 * @param[in]   sjr  - pointer to startjob_rtn structure
 *
 * @return session/job id
 * @retval -1 error from setsid(), no message in log_buffer
 * @retval -2 temporary error, retry job, message in log_buffer
 *
 */

int
set_job(job *pjob, struct startjob_rtn *sjr)
{
	return (sjr->sj_session = setsid());
}

/**
 * @brief
 *      set_globid - set the global id for a machine type.
 *
 * @param[in] pjob - pointer to job structure
 * @param[in] sjr  - pointer to startjob_rtn structure
 *
 * @return Void
 *
 */

void
set_globid(job *pjob, truct startjob_rtn *jr)
{
	return;
}

/**
 * @brief
 *      set_mach_vars - setup machine dependent environment variables
 *
 * @param[in] pjob - pointer to job structure
 * @param[in] vtab - pointer to var_table structure
 *
 * @return      int
 * @retval      0       Success
 *
 */

int
set_mach_vars(job *pjob, struct var_table *vtab)
{
	return 0;
}

/**
 * @brief
 *      sets the shell to be used
 *
 * @param[in] pjob - pointer to job structure
 * @param[in] pwdp - pointer to passwd structure
 *
 * @return      string
 * @retval      shellname       Success
 *
 */

char *
set_shell(job *pjob, struct passwd *pwdp)
{
	char *cp;
	int   i;
	char *shell;
	struct array_strings *vstrs;
	/*
	 * find which shell to use, one specified or the login shell
	 */

	shell = pwdp->pw_shell;
	if ((pjob->ji_wattr[(int)JOB_ATR_shell].at_flags & ATR_VFLAG_SET) &&
		(vstrs = pjob->ji_wattr[(int)JOB_ATR_shell].at_val.at_arst)) {
		for (i = 0; i < vstrs->as_usedptr; ++i) {
			cp = strchr(vstrs->as_string[i], '@');
			if (cp) {
				if (!strncmp(mom_host, cp+1, strlen(cp+1))) {
					*cp = '\0';	/* host name matches */
					shell = vstrs->as_string[i];
					break;
				}
			} else {
				shell = vstrs->as_string[i];	/* wildcard */
			}
		}
	}
	return (shell);
}

/**
 *
 * @brief
 * 	Checks if a child of the current (mom) process has terminated, and
 *	matches it with the pid of one of the tasks in the task_list_event,
 *	or matches the pid of a process being monitored for a PBS job.
 *	if matching a task in the task_list_event, then that task is
 *	marked as WORK_Deferred_Cmp along with the exit value of the child
 *	process. Otherwise if it's for a job, and that job's
 *	JOB_SVFLAG_TERMJOB is set, then mark the job as exiting.
 *
 * @return	Void
 *
 */

void
scan_for_terminated()
{
	int		exiteval;
	pid_t		pid;
	job		*pjob;
	task		*ptask;
	int		statloc;
	struct work_task *wtask = NULL;

	/* update the latest intelligence about the running jobs;         */
	/* must be done before we reap the zombies, else we lose the info */

	termin_child = 0;

	if (mom_get_sample() == PBSE_NONE) {
		pjob = (job *)GET_NEXT(svr_alljobs);
		while (pjob) {
			mom_set_use(pjob);
			pjob = (job *)GET_NEXT(pjob->ji_alljobs);
		}
	}

	/* Now figure out which task(s) have terminated (are zombies) */

	while ((pid = waitpid(-1, &statloc, WNOHANG)) > 0) {

		pjob = (job *)GET_NEXT(svr_alljobs);
		while (pjob) {
			/*
			 ** see if process was a child doing a special
			 ** function for MOM
			 */
			if (pid == pjob->ji_momsubt)
				break;
			/*
			 ** look for task
			 */
			ptask = (task *)GET_NEXT(pjob->ji_tasks);
			while (ptask) {
				if (ptask->ti_qs.ti_sid == pid)
					break;
				ptask = (task *)GET_NEXT(ptask->ti_jobtask);
			}
			if (ptask != NULL)
				break;
			pjob = (job *)GET_NEXT(pjob->ji_alljobs);
		}
		if (WIFEXITED(statloc))
			exiteval = WEXITSTATUS(statloc);
		else if (WIFSIGNALED(statloc))
			exiteval = WTERMSIG(statloc) + 10000;
		else
			exiteval = 1;

		/* Check for other task lists */
		wtask = (struct work_task *)GET_NEXT(task_list_event);
		while (wtask) {
			if ((wtask->wt_type == WORK_Deferred_Child) &&
				(wtask->wt_event == pid)) {
				wtask->wt_type = WORK_Deferred_Cmp;
				wtask->wt_aux = (int)exiteval; /* exit status */
				svr_delay_entry++;	/* see next_task() */
			}
			wtask = (struct work_task *)GET_NEXT(wtask->wt_linkall);
		}

		if (pjob == NULL) {
			DBPRT(("%s: pid %d not tracked, exit %d\n",
				__func__, pid, exiteval))
			continue;
		}

		if (pid == pjob->ji_momsubt) {
			pjob->ji_momsubt = 0;
			if (pjob->ji_mompost) {
				pjob->ji_mompost(pjob, exiteval);
			}
			(void)job_save(pjob, SAVEJOB_QUICK);
			continue;
		}
		DBPRT(("%s: task %08.8X pid %d exit value %d\n", __func__,
			ptask->ti_qs.ti_task, pid, exiteval))
		ptask->ti_qs.ti_exitstat = exiteval;
		sprintf(log_buffer, "task %08.8X terminated",
			ptask->ti_qs.ti_task);
		log_event(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB, LOG_DEBUG,
			pjob->ji_qs.ji_jobid, log_buffer);

		/*
		 ** After the top process(shell) of the TASK exits, check if the
		 ** JOB_SVFLG_TERMJOB job flag set. If yes, then check for any
		 ** live process(s) in the session. If found, make the task
		 ** ORPHAN by setting the flag and delay by kill_delay time. This
		 ** will be exited in kill_job or by cput_sum() as can not be
		 ** seen again by scan_for_terminated().
		 */
		if (pjob->ji_qs.ji_svrflags & JOB_SVFLG_TERMJOB) {
			int	n;

			(void)mom_get_sample();
			n = bld_ptree(ptask->ti_qs.ti_sid);
			if (n > 0) {
				ptask->ti_flags |= TI_FLAGS_ORPHAN;
				DBPRT(("%s: task %8.8X still has %d active procs\n", __func__,
					ptask->ti_qs.ti_task, n))
				continue;
			}
		}

		kill_session(ptask->ti_qs.ti_sid, SIGKILL, 0);
		ptask->ti_qs.ti_status = TI_STATE_EXITED;
		ptask->ti_qs.ti_sid = 0;
		(void)task_save(ptask);
		exiting_tasks = 1;
	}
}


/**
 * @brief
 * 	creat the master pty, this particular
 * 	piece of code depends on multiplexor /dev/ptc
 *
 * @param[out] rtn_name - holds info of tty
 *
 * @return      int
 * @retval      fd      Success
 * @retval      -1      Failure
 *
 */

#define PTY_SIZE 12

int
open_master(char **rtn_name)
char **rtn_name;	/* RETURN name of slave pts */
{
	char 	       *pc1;
	char 	       *pc2;
	int		ptc;	/* master file descriptor */
	static char	ptcchar1[] = "pqrs";
	static char	ptcchar2[] = "0123456789abcdef";
	static char	pty_name[PTY_SIZE+1];	/* "/dev/[pt]tyXY" */

	(void)strncpy(pty_name, "/dev/ptyXY", PTY_SIZE);
	for (pc1 = ptcchar1; *pc1 != '\0'; ++pc1) {
		pty_name[8] = *pc1;
		for (pc2 = ptcchar2; *pc2 != '\0'; ++pc2) {
			pty_name[9] = *pc2;
			if ((ptc = open(pty_name, O_RDWR | O_NOCTTY, 0)) >= 0) {
				/* Got a master, fix name to matching slave */
				pty_name[5] = 't';
				*rtn_name = pty_name;
				return (ptc);

			} else if (errno == ENOENT)
				return (-1);	/* tried all entries, give up */
		}
	}
	return (-1);	/* tried all entries, give up */
}


/*
 * struct sig_tbl = map of signal names to numbers,
 * see req_signal() in ../requests.c
 */

struct sig_tbl sig_tbl[] = {
	{ "NULL", 0 },
	{ "HUP", SIGHUP },
	{ "INT", SIGINT },
	{ "QUIT", SIGQUIT },
	{ "ILL", SIGILL },
	{ "TRAP", SIGTRAP },
	{ "IOT", SIGIOT },
	{ "ABRT", SIGABRT },
	{ "EMT", SIGEMT },
	{ "FPE", SIGFPE },
	{ "KILL", SIGKILL },
	{ "BUS", SIGBUS },
	{ "SEGV", SIGSEGV },
	{ "SYS", SIGSYS },
	{ "PIPE", SIGPIPE },
	{ "ALRM", SIGALRM },
	{ "TERM", SIGTERM },
	{ "USR1", SIGUSR1 },
	{ "USR2", SIGUSR2 },
	{ "CHLD", SIGCHLD },
	{ "PWR", SIGPWR },
	{ "WINCH", SIGWINCH },
	{ "URG", SIGURG },
	{ "POLL", SIGPOLL },
	{ "IO", SIGIO },
	{ "STOP", SIGSTOP },
	{ "TSTP", SIGTSTP },
	{ "CONT", SIGCONT },
	{ "TTIN", SIGTTIN },
	{ "TTOU", SIGTTOU },
	{ "VTALRM", SIGVTALRM },
	{ "PROF", SIGPROF },
	{ "XCPU", SIGXCPU },
	{ "XFSZ", SIGXFSZ },
	{(char *)0, -1}
};
