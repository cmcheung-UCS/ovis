/* -*- c-basic-offset: 8 -*-
 * Copyright (c) 2020-2021 National Technology & Engineering Solutions
 * of Sandia, LLC (NTESS). Under the terms of Contract DE-NA0003525 with
 * NTESS, the U.S. Government retains certain rights in this software.
 * Copyright (c) 2020-2021 Open Grid Computing, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the BSD-type
 * license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *      Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *      Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *      Neither the name of Sandia nor the names of any contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 *      Neither the name of Open Grid Computing nor the names of any
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *      Modified source versions must be plainly marked as such, and
 *      must not be misrepresented as being the original software.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file linux_proc_sampler.c
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dirent.h>
#include <ctype.h>
#include <assert.h>

#include <coll/rbt.h>

#include "ldmsd.h"
#include "../sampler_base.h"
#include "ldmsd_stream.h"
#include "mmalloc.h"
#define DSTRING_USE_SHORT
#include "ovis_util/dstring.h"

#define SAMP "linux_proc_sampler"

#define DEFAULT_STREAM "slurm"

#define INST_LOG(inst, lvl, fmt, ...) \
		 inst->log((lvl), "%s: " fmt, SAMP, ##__VA_ARGS__)

#ifndef ARRAY_LEN
#define ARRAY_LEN(a) (sizeof((a))/sizeof((a)[0]))
#endif

typedef enum linux_proc_sampler_metric linux_proc_sampler_metric_e;
enum linux_proc_sampler_metric {
	APP_ALL = 0,

	/* /proc/[pid]/cmdline */
	APP_CMDLINE_LEN, _APP_CMDLINE_FIRST = APP_CMDLINE_LEN,
			 _APP_FIRST = _APP_CMDLINE_FIRST,
	APP_CMDLINE, _APP_CMDLINE_LAST = APP_CMDLINE,

	/* number of open files */
	APP_N_OPEN_FILES,

	/* io */
	APP_IO_READ_B, _APP_IO_FIRST = APP_IO_READ_B,
	APP_IO_WRITE_B,
	APP_IO_N_READ,
	APP_IO_N_WRITE,
	APP_IO_READ_DEV_B,
	APP_IO_WRITE_DEV_B,
	APP_IO_WRITE_CANCELLED_B, _APP_IO_LAST = APP_IO_WRITE_CANCELLED_B,

	APP_OOM_SCORE,

	APP_OOM_SCORE_ADJ,

	APP_ROOT,

	/* /proc/[pid]/stat */
	APP_STAT_PID, _APP_STAT_FIRST = APP_STAT_PID,
	APP_STAT_COMM,
	APP_STAT_STATE,
	APP_STAT_PPID,
	APP_STAT_PGRP,
	APP_STAT_SESSION,
	APP_STAT_TTY_NR,
	APP_STAT_TPGID,
	APP_STAT_FLAGS,
	APP_STAT_MINFLT,
	APP_STAT_CMINFLT,
	APP_STAT_MAJFLT,
	APP_STAT_CMAJFLT,
	APP_STAT_UTIME,
	APP_STAT_STIME,
	APP_STAT_CUTIME,
	APP_STAT_CSTIME,
	APP_STAT_PRIORITY,
	APP_STAT_NICE,
	APP_STAT_NUM_THREADS,
	APP_STAT_ITREALVALUE,
	APP_STAT_STARTTIME,
	APP_STAT_VSIZE,
	APP_STAT_RSS,
	APP_STAT_RSSLIM,
	APP_STAT_STARTCODE,
	APP_STAT_ENDCODE,
	APP_STAT_STARTSTACK,
	APP_STAT_KSTKESP,
	APP_STAT_KSTKEIP,
	APP_STAT_SIGNAL,
	APP_STAT_BLOCKED,
	APP_STAT_SIGIGNORE,
	APP_STAT_SIGCATCH,
	APP_STAT_WCHAN,
	APP_STAT_NSWAP,
	APP_STAT_CNSWAP,
	APP_STAT_EXIT_SIGNAL,
	APP_STAT_PROCESSOR,
	APP_STAT_RT_PRIORITY,
	APP_STAT_POLICY,
	APP_STAT_DELAYACCT_BLKIO_TICKS,
	APP_STAT_GUEST_TIME,
	APP_STAT_CGUEST_TIME,
	APP_STAT_START_DATA,
	APP_STAT_END_DATA,
	APP_STAT_START_BRK,
	APP_STAT_ARG_START,
	APP_STAT_ARG_END,
	APP_STAT_ENV_START,
	APP_STAT_ENV_END,
	APP_STAT_EXIT_CODE, _APP_STAT_LAST = APP_STAT_EXIT_CODE,

	APP_STATUS_NAME, _APP_STATUS_FIRST = APP_STATUS_NAME,
	APP_STATUS_UMASK,
	APP_STATUS_STATE,
	APP_STATUS_TGID,
	APP_STATUS_NGID,
	APP_STATUS_PID,
	APP_STATUS_PPID,
	APP_STATUS_TRACERPID,
	APP_STATUS_UID,
	APP_STATUS_GID,
	APP_STATUS_FDSIZE,
	APP_STATUS_GROUPS,
	APP_STATUS_NSTGID,
	APP_STATUS_NSPID,
	APP_STATUS_NSPGID,
	APP_STATUS_NSSID,
	APP_STATUS_VMPEAK,
	APP_STATUS_VMSIZE,
	APP_STATUS_VMLCK,
	APP_STATUS_VMPIN,
	APP_STATUS_VMHWM,
	APP_STATUS_VMRSS,
	APP_STATUS_RSSANON,
	APP_STATUS_RSSFILE,
	APP_STATUS_RSSSHMEM,
	APP_STATUS_VMDATA,
	APP_STATUS_VMSTK,
	APP_STATUS_VMEXE,
	APP_STATUS_VMLIB,
	APP_STATUS_VMPTE,
	APP_STATUS_VMPMD,
	APP_STATUS_VMSWAP,
	APP_STATUS_HUGETLBPAGES,
	APP_STATUS_COREDUMPING,
	APP_STATUS_THREADS,
	APP_STATUS_SIG_QUEUED,
	APP_STATUS_SIG_LIMIT,
	APP_STATUS_SIGPND,
	APP_STATUS_SHDPND,
	APP_STATUS_SIGBLK,
	APP_STATUS_SIGIGN,
	APP_STATUS_SIGCGT,
	APP_STATUS_CAPINH,
	APP_STATUS_CAPPRM,
	APP_STATUS_CAPEFF,
	APP_STATUS_CAPBND,
	APP_STATUS_CAPAMB,
	APP_STATUS_NONEWPRIVS,
	APP_STATUS_SECCOMP,
	APP_STATUS_SPECULATION_STORE_BYPASS,
	APP_STATUS_CPUS_ALLOWED,
	APP_STATUS_CPUS_ALLOWED_LIST,
	APP_STATUS_MEMS_ALLOWED,
	APP_STATUS_MEMS_ALLOWED_LIST,
	APP_STATUS_VOLUNTARY_CTXT_SWITCHES,
	APP_STATUS_NONVOLUNTARY_CTXT_SWITCHES,
	_APP_STATUS_LAST = APP_STATUS_NONVOLUNTARY_CTXT_SWITCHES,

	APP_SYSCALL,

	APP_TIMERSLACK_NS,

	APP_WCHAN, /* string */

	APP_TIMING,

	_APP_LAST = APP_TIMING,
};

typedef struct linux_proc_sampler_metric_info_s *linux_proc_sampler_metric_info_t;
struct linux_proc_sampler_metric_info_s {
	linux_proc_sampler_metric_e code;
	const char *name;
	const char *unit;
	enum ldms_value_type mtype;
	int array_len; /* in case mtype is _ARRAY */
	int is_meta;
};

/* CMDLINE_SZ is used for loading /proc/pid/[status,cmdline,syscall,stat] */
#define CMDLINE_SZ 4096
/* space for a /proc/$pid/<leaf> name */
#define PROCPID_SZ 64
/* #define MOUNTINFO_SZ 8192 */
#define ROOT_SZ 4096
#define STAT_COMM_SZ 4096
#define WCHAN_SZ 128 /* NOTE: KSYM_NAME_LEN = 128 */
#define SPECULATION_SZ 64
#define GROUPS_SZ 16
#define NS_SZ 16
#define CPUS_ALLOWED_SZ 4 /* 4 x 64 = 256 bits max */
#define MEMS_ALLOWED_SZ 128 /* 128 x 64 = 8192 bits max */
#define CPUS_ALLOWED_LIST_SZ 128 /* length of list string */
#define MEMS_ALLOWED_LIST_SZ 128 /* length of list string */

struct linux_proc_sampler_metric_info_s metric_info[] = {
	[APP_ALL] = { APP_ALL, "ALL", "", 0, 0, 0 },
	/* /proc/[pid]/cmdline */
	[APP_CMDLINE_LEN] = { APP_CMDLINE_LEN, "cmdline_len", "" , LDMS_V_U16, 0, 1 },
	[APP_CMDLINE] = { APP_CMDLINE, "cmdline", "", LDMS_V_CHAR_ARRAY, CMDLINE_SZ, 1 },

	/* number of open files */
	[APP_N_OPEN_FILES] = { APP_N_OPEN_FILES, "n_open_files", "", LDMS_V_U64, 0, 0 },

	/* io */
	[APP_IO_READ_B] = { APP_IO_READ_B, "io_read_b", "B", LDMS_V_U64, 0, 0 },
	[APP_IO_WRITE_B] = { APP_IO_WRITE_B, "io_write_b", "B", LDMS_V_U64, 0, 0 },
	[APP_IO_N_READ] = { APP_IO_N_READ, "io_n_read", "", LDMS_V_U64, 0, 0 },
	[APP_IO_N_WRITE] = { APP_IO_N_WRITE, "io_n_write", "", LDMS_V_U64, 0, 0 },
	[APP_IO_READ_DEV_B] = { APP_IO_READ_DEV_B, "io_read_dev_b", "B", LDMS_V_U64, 0, 0 },
	[APP_IO_WRITE_DEV_B] = { APP_IO_WRITE_DEV_B, "io_write_dev_b", "B", LDMS_V_U64, 0, 0 },
	[APP_IO_WRITE_CANCELLED_B] = { APP_IO_WRITE_CANCELLED_B, "io_write_cancelled_b", "B", LDMS_V_U64, 0, 0 },

	[APP_OOM_SCORE] = { APP_OOM_SCORE, "oom_score", "", LDMS_V_U64, 0, 0 },

	[APP_OOM_SCORE_ADJ] = { APP_OOM_SCORE_ADJ, "oom_score_adj", "", LDMS_V_U64, 0, 0 },

	[APP_ROOT] = { APP_ROOT, "root", "", LDMS_V_CHAR_ARRAY, ROOT_SZ, 1 },

	/* /proc/[pid]/stat */
	[APP_STAT_PID] = { APP_STAT_PID, "stat_pid", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_COMM] = { APP_STAT_COMM, "stat_comm", "", LDMS_V_CHAR_ARRAY, STAT_COMM_SZ, 1 },
	[APP_STAT_STATE] = { APP_STAT_STATE, "stat_state", "", LDMS_V_CHAR, 0, 0 },
	[APP_STAT_PPID] = { APP_STAT_PPID, "stat_ppid", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_PGRP] = { APP_STAT_PGRP, "stat_pgrp", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_SESSION] = { APP_STAT_SESSION, "stat_session", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_TTY_NR] = { APP_STAT_TTY_NR, "stat_tty_nr", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_TPGID] = { APP_STAT_TPGID, "stat_tpgid", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_FLAGS] = { APP_STAT_FLAGS, "stat_flags", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_MINFLT] = { APP_STAT_MINFLT, "stat_minflt", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_CMINFLT] = { APP_STAT_CMINFLT, "stat_cminflt", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_MAJFLT] = { APP_STAT_MAJFLT, "stat_majflt", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_CMAJFLT] = { APP_STAT_CMAJFLT, "stat_cmajflt", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_UTIME] = { APP_STAT_UTIME, "stat_utime", "ticks", LDMS_V_U64, 0, 0 },
	[APP_STAT_STIME] = { APP_STAT_STIME, "stat_stime", "ticks", LDMS_V_U64, 0, 0 },
	[APP_STAT_CUTIME] = { APP_STAT_CUTIME, "stat_cutime", "ticks", LDMS_V_U64, 0, 0 },
	[APP_STAT_CSTIME] = { APP_STAT_CSTIME, "stat_cstime", "ticks", LDMS_V_U64, 0, 0 },
	[APP_STAT_PRIORITY] = { APP_STAT_PRIORITY, "stat_priority", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_NICE] = { APP_STAT_NICE, "stat_nice", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_NUM_THREADS] =
		{ APP_STAT_NUM_THREADS, "stat_num_threads", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_ITREALVALUE] =
		{ APP_STAT_ITREALVALUE, "stat_itrealvalue", "ticks", LDMS_V_U64, 0, 0 },
	[APP_STAT_STARTTIME] =
		{ APP_STAT_STARTTIME, "stat_starttime", "ticks", LDMS_V_U64, 0, 0 },
	[APP_STAT_VSIZE] = { APP_STAT_VSIZE, "stat_vsize", "B", LDMS_V_U64, 0, 0 },
	[APP_STAT_RSS] = { APP_STAT_RSS, "stat_rss", "PG", LDMS_V_U64, 0, 0 },
	[APP_STAT_RSSLIM] = { APP_STAT_RSSLIM, "stat_rsslim", "B", LDMS_V_U64, 0, 0 },
	[APP_STAT_STARTCODE] = { APP_STAT_STARTCODE, "stat_startcode", "PTR", LDMS_V_U64, 0, 0 },
	[APP_STAT_ENDCODE] = { APP_STAT_ENDCODE, "stat_endcode", "PTR", LDMS_V_U64, 0, 0 },
	[APP_STAT_STARTSTACK] =
		{ APP_STAT_STARTSTACK, "stat_startstack", "PTR", LDMS_V_U64, 0, 0 },
	[APP_STAT_KSTKESP] = { APP_STAT_KSTKESP, "stat_kstkesp", "PTR", LDMS_V_U64, 0, 0 },
	[APP_STAT_KSTKEIP] = { APP_STAT_KSTKEIP, "stat_kstkeip", "PTR", LDMS_V_U64, 0, 0 },
	[APP_STAT_SIGNAL] = { APP_STAT_SIGNAL, "stat_signal", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_BLOCKED] = { APP_STAT_BLOCKED, "stat_blocked", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_SIGIGNORE] = { APP_STAT_SIGIGNORE, "stat_sigignore", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_SIGCATCH] = { APP_STAT_SIGCATCH, "stat_sigcatch", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_WCHAN] = { APP_STAT_WCHAN, "stat_wchan", "PTR", LDMS_V_U64, 0, 0 },
	[APP_STAT_NSWAP] = { APP_STAT_NSWAP, "stat_nswap", "PG", LDMS_V_U64, 0, 0 },
	[APP_STAT_CNSWAP] = { APP_STAT_CNSWAP, "stat_cnswap", "PG", LDMS_V_U64, 0, 0 },
	[APP_STAT_EXIT_SIGNAL] = { APP_STAT_EXIT_SIGNAL, "stat_exit_signal", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_PROCESSOR] = { APP_STAT_PROCESSOR, "stat_processor", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_RT_PRIORITY] = { APP_STAT_RT_PRIORITY, "stat_rt_priority", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_POLICY] = { APP_STAT_POLICY, "stat_policy", "", LDMS_V_U64, 0, 0 },
	[APP_STAT_DELAYACCT_BLKIO_TICKS] = { APP_STAT_DELAYACCT_BLKIO_TICKS, "stat_delayacct_blkio_ticks", "ticks", LDMS_V_U64, 0, 0 },
	[APP_STAT_GUEST_TIME] = { APP_STAT_GUEST_TIME, "stat_guest_time", "ticks", LDMS_V_U64, 0, 0 },
	[APP_STAT_CGUEST_TIME] = { APP_STAT_CGUEST_TIME, "stat_cguest_time", "ticks", LDMS_V_U64, 0, 0 },
	[APP_STAT_START_DATA] = { APP_STAT_START_DATA, "stat_start_data", "PTR", LDMS_V_U64, 0, 0 },
	[APP_STAT_END_DATA] = { APP_STAT_END_DATA, "stat_end_data", "PTR", LDMS_V_U64, 0, 0 },
	[APP_STAT_START_BRK] = { APP_STAT_START_BRK, "stat_start_brk", "PTR", LDMS_V_U64, 0, 0 },
	[APP_STAT_ARG_START] = { APP_STAT_ARG_START, "stat_arg_start", "PTR", LDMS_V_U64, 0, 0 },
	[APP_STAT_ARG_END] = { APP_STAT_ARG_END, "stat_arg_end", "PTR", LDMS_V_U64, 0, 0 },
	[APP_STAT_ENV_START] = { APP_STAT_ENV_START, "stat_env_start", "PTR", LDMS_V_U64, 0, 0 },
	[APP_STAT_ENV_END] = { APP_STAT_ENV_END, "stat_env_end", "PTR", LDMS_V_U64, 0, 0 },
	[APP_STAT_EXIT_CODE] = { APP_STAT_EXIT_CODE, "stat_exit_code", "", LDMS_V_U64, 0, 0 },

	/* /proc/[pid]/status */
	[APP_STATUS_NAME] = { APP_STATUS_NAME, "status_name", "", LDMS_V_CHAR_ARRAY, STAT_COMM_SZ, 1 },
	[APP_STATUS_UMASK] = { APP_STATUS_UMASK, "status_umask", "", LDMS_V_U32, 0, 0 },
	[APP_STATUS_STATE] = { APP_STATUS_STATE, "status_state", "", LDMS_V_CHAR, 0, 0 },
	[APP_STATUS_TGID] = { APP_STATUS_TGID, "status_tgid", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_NGID] = { APP_STATUS_NGID, "status_ngid", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_PID] = { APP_STATUS_PID, "status_pid", "" , LDMS_V_U64, 0, 0},
	[APP_STATUS_PPID] = { APP_STATUS_PPID, "status_ppid", "" , LDMS_V_U64, 0, 0},
	[APP_STATUS_TRACERPID] =
		{ APP_STATUS_TRACERPID, "status_tracerpid", "" , LDMS_V_U64, 0, 0},
	[APP_STATUS_UID] = { APP_STATUS_UID, "status_uid", "", LDMS_V_U64_ARRAY, 4, 0 },
	[APP_STATUS_GID] = { APP_STATUS_GID, "status_gid", "" , LDMS_V_U64_ARRAY, 4, 0},
	[APP_STATUS_FDSIZE] = { APP_STATUS_FDSIZE, "status_fdsize", "" , LDMS_V_U64, 0, 0},
	[APP_STATUS_GROUPS] = { APP_STATUS_GROUPS, "status_groups", "", LDMS_V_U64_ARRAY, GROUPS_SZ, 0 },
	[APP_STATUS_NSTGID] = { APP_STATUS_NSTGID, "status_nstgid", "", LDMS_V_U64_ARRAY, NS_SZ, 0 },
	[APP_STATUS_NSPID] = { APP_STATUS_NSPID, "status_nspid", "", LDMS_V_U64_ARRAY, NS_SZ, 0 },
	[APP_STATUS_NSPGID] = { APP_STATUS_NSPGID, "status_nspgid", "", LDMS_V_U64_ARRAY, NS_SZ, 0 },
	[APP_STATUS_NSSID] = { APP_STATUS_NSSID, "status_nssid", "", LDMS_V_U64_ARRAY, NS_SZ, 0 },
	[APP_STATUS_VMPEAK] = { APP_STATUS_VMPEAK, "status_vmpeak", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_VMSIZE] = { APP_STATUS_VMSIZE, "status_vmsize", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_VMLCK] = { APP_STATUS_VMLCK, "status_vmlck", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_VMPIN] = { APP_STATUS_VMPIN, "status_vmpin", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_VMHWM] = { APP_STATUS_VMHWM, "status_vmhwm", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_VMRSS] = { APP_STATUS_VMRSS, "status_vmrss", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_RSSANON] = { APP_STATUS_RSSANON, "status_rssanon", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_RSSFILE] = { APP_STATUS_RSSFILE, "status_rssfile", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_RSSSHMEM] = { APP_STATUS_RSSSHMEM, "status_rssshmem", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_VMDATA] = { APP_STATUS_VMDATA, "status_vmdata", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_VMSTK] = { APP_STATUS_VMSTK, "status_vmstk", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_VMEXE] = { APP_STATUS_VMEXE, "status_vmexe", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_VMLIB] = { APP_STATUS_VMLIB, "status_vmlib", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_VMPTE] = { APP_STATUS_VMPTE, "status_vmpte", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_VMPMD] = { APP_STATUS_VMPMD, "status_vmpmd", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_VMSWAP] = { APP_STATUS_VMSWAP, "status_vmswap", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_HUGETLBPAGES] = { APP_STATUS_HUGETLBPAGES, "status_hugetlbpages", "kB", LDMS_V_U64, 0, 0 },
	[APP_STATUS_COREDUMPING] = { APP_STATUS_COREDUMPING, "status_coredumping", "bool", LDMS_V_U8, 0, 0 },
	[APP_STATUS_THREADS] = { APP_STATUS_THREADS, "status_threads", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_SIG_QUEUED] = { APP_STATUS_SIG_QUEUED, "status_sig_queued", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_SIG_LIMIT] = { APP_STATUS_SIG_LIMIT, "status_sig_limit", "", LDMS_V_U64, 0, 0},
	[APP_STATUS_SIGPND] = { APP_STATUS_SIGPND, "status_sigpnd", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_SHDPND] = { APP_STATUS_SHDPND, "status_shdpnd", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_SIGBLK] = { APP_STATUS_SIGBLK, "status_sigblk", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_SIGIGN] = { APP_STATUS_SIGIGN, "status_sigign", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_SIGCGT] = { APP_STATUS_SIGCGT, "status_sigcgt", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_CAPINH] = { APP_STATUS_CAPINH, "status_capinh", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_CAPPRM] = { APP_STATUS_CAPPRM, "status_capprm", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_CAPEFF] = { APP_STATUS_CAPEFF, "status_capeff", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_CAPBND] = { APP_STATUS_CAPBND, "status_capbnd", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_CAPAMB] = { APP_STATUS_CAPAMB, "status_capamb", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_NONEWPRIVS] = { APP_STATUS_NONEWPRIVS, "status_nonewprivs", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_SECCOMP] = { APP_STATUS_SECCOMP, "status_seccomp", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_SPECULATION_STORE_BYPASS] = { APP_STATUS_SPECULATION_STORE_BYPASS, "status_speculation_store_bypass", "", LDMS_V_CHAR_ARRAY, SPECULATION_SZ, 0 },
	[APP_STATUS_CPUS_ALLOWED] = { APP_STATUS_CPUS_ALLOWED, "status_cpus_allowed", "", LDMS_V_U32_ARRAY, CPUS_ALLOWED_SZ, 0 },
	[APP_STATUS_CPUS_ALLOWED_LIST] = { APP_STATUS_CPUS_ALLOWED_LIST, "status_cpus_allowed_list", "", LDMS_V_CHAR_ARRAY, CPUS_ALLOWED_LIST_SZ, 0 },
	[APP_STATUS_MEMS_ALLOWED] = { APP_STATUS_MEMS_ALLOWED, "status_mems_allowed", "", LDMS_V_U32_ARRAY, MEMS_ALLOWED_SZ, 0 },
	[APP_STATUS_MEMS_ALLOWED_LIST] = { APP_STATUS_MEMS_ALLOWED_LIST, "status_mems_allowed_list", "", LDMS_V_CHAR_ARRAY, MEMS_ALLOWED_LIST_SZ, 0 },
	[APP_STATUS_VOLUNTARY_CTXT_SWITCHES] = { APP_STATUS_VOLUNTARY_CTXT_SWITCHES, "status_voluntary_ctxt_switches", "", LDMS_V_U64, 0, 0 },
	[APP_STATUS_NONVOLUNTARY_CTXT_SWITCHES] = { APP_STATUS_NONVOLUNTARY_CTXT_SWITCHES, "status_nonvoluntary_ctxt_switches", "", LDMS_V_U64, 0, 0 },

	[APP_SYSCALL] = { APP_SYSCALL, "syscall", "", LDMS_V_U64_ARRAY, 9, 0 },

	[APP_TIMERSLACK_NS] = { APP_TIMERSLACK_NS, "timerslack_ns", "ns", LDMS_V_U64, 0, 0 },

	[APP_WCHAN] = { APP_WCHAN, "wchan", "", LDMS_V_CHAR_ARRAY, WCHAN_SZ, 0 },
	[APP_TIMING] = { APP_TIMING, "sample_us", "", LDMS_V_U64, 0, 0 },

};

/* This array is initialized in __init__() below */
linux_proc_sampler_metric_info_t metric_info_idx_by_name[_APP_LAST + 1];

struct set_key {
	uint64_t start_tick;
	int64_t os_pid;
};

struct linux_proc_sampler_set {
	struct set_key key;
	ldms_set_t set;
	int64_t task_rank;
	struct rbn rbn;
	int dead;
	LIST_ENTRY(linux_proc_sampler_set) del;
};
LIST_HEAD(set_del_list, linux_proc_sampler_set);


#if 0
static void string_to_timeval(struct timeval *t, char *s)
{
	if (!t || !s)
		return;
	char b[strlen(s)+1];
	int sec, usec;
	strcpy(b, s);
	char *dot = strchr(b, '.');
	if (!dot) {
		sscanf(b, "%d", &sec);
		t->tv_sec = sec;
		t->tv_usec = 0;
	} else {
		size_t fraclen = strlen(dot);
		if (fraclen > 7) {
			dot[7] = '\0';
		}
		sscanf(b, "%d", &sec);
		t->tv_sec = sec;
		sscanf(dot+1, "%d", &usec);
		size_t usec_len = strlen(dot+1);
		switch (usec_len) {
		case 1:
			usec *= 100000;
			break;
		case 2:
			usec *= 10000;
			break;
		case 3:
			usec *= 1000;
			break;
		case 4:
			usec *= 100;
			break;
		case 5:
			usec *= 10;
			break;
		case 6:
			break;
		}
		t->tv_usec = usec;
	}
}
#endif

typedef struct linux_proc_sampler_inst_s *linux_proc_sampler_inst_t;
typedef int (*handler_fn_t)(linux_proc_sampler_inst_t inst, pid_t pid, ldms_set_t set);
struct handler_info {
	handler_fn_t fn;
	const char *fn_name;
};

struct linux_proc_sampler_inst_s {
	struct ldmsd_sampler samp;

	ldmsd_msg_log_f log;
	base_data_t base_data;
	char *instance_prefix;
	bool exe_suffix;
	long sc_clk_tck;
	struct timeval sample_start;

	struct rbt set_rbt;
	pthread_mutex_t mutex;

	char *stream_name;
	ldmsd_stream_client_t stream;
	char *argv_sep;

	struct handler_info fn[17];
	int n_fn;

	int task_rank_idx;
	int start_time_idx;
	int start_tick_idx;
	int exe_idx;
	int sc_clk_tck_idx;
	int is_thread_idx;
	int parent_pid_idx;
	int metric_idx[_APP_LAST+1]; /* 0 means disabled */
};

static void data_set_key(linux_proc_sampler_inst_t inst, struct linux_proc_sampler_set *as, uint64_t tick, int64_t os_pid)
{
	if (!as)
		return;
	as->key.os_pid = os_pid;
	as->key.start_tick = tick;
#ifdef LPDEBUG
	INST_LOG(inst, LDMSD_LDEBUG,"Creating key at %p: %" PRIu64 " , %" PRId64 "\n",
		as, tick, os_pid);
#endif

}

static int cmdline_handler(linux_proc_sampler_inst_t, pid_t, ldms_set_t);
static int n_open_files_handler(linux_proc_sampler_inst_t, pid_t, ldms_set_t);
static int io_handler(linux_proc_sampler_inst_t, pid_t, ldms_set_t);
static int oom_score_handler(linux_proc_sampler_inst_t, pid_t, ldms_set_t);
static int oom_score_adj_handler(linux_proc_sampler_inst_t, pid_t, ldms_set_t);
static int root_handler(linux_proc_sampler_inst_t, pid_t, ldms_set_t);
static int stat_handler(linux_proc_sampler_inst_t, pid_t, ldms_set_t);
static int status_handler(linux_proc_sampler_inst_t, pid_t, ldms_set_t);
static int syscall_handler(linux_proc_sampler_inst_t, pid_t, ldms_set_t);
static int timerslack_ns_handler(linux_proc_sampler_inst_t, pid_t, ldms_set_t);
static int wchan_handler(linux_proc_sampler_inst_t, pid_t, ldms_set_t);
static int timing_handler(linux_proc_sampler_inst_t, pid_t, ldms_set_t);

/* mapping metric -> handler */
struct handler_info handler_info_tbl[] = {
	[_APP_CMDLINE_FIRST ... _APP_CMDLINE_LAST] = { .fn = cmdline_handler, .fn_name = "cmdline_handler" },
	[APP_N_OPEN_FILES] = { .fn = n_open_files_handler, .fn_name = "n_open_files_handler" },
	[_APP_IO_FIRST ... _APP_IO_LAST] = { .fn = io_handler, .fn_name= "io_handler" },
	[APP_OOM_SCORE] = { .fn = oom_score_handler, .fn_name= "oom_score_handler" },
	[APP_OOM_SCORE_ADJ] = { .fn = oom_score_adj_handler, .fn_name= "oom_score_adj_handler" },
	[APP_ROOT] = { .fn = root_handler, .fn_name= "root_handler" },
	[_APP_STAT_FIRST ... _APP_STAT_LAST] = { .fn = stat_handler, .fn_name = "stat_handler"},
	[_APP_STATUS_FIRST ... _APP_STATUS_LAST] = { .fn = status_handler, .fn_name = "status_handler" },
	[APP_SYSCALL] = { .fn = syscall_handler, .fn_name = "syscall_handler" },
	[APP_TIMERSLACK_NS] = { .fn = timerslack_ns_handler, .fn_name = "timerslack_ns_handler" },
	[APP_WCHAN] = { .fn = wchan_handler, .fn_name = "wchan_handler" },
	[APP_TIMING] = { .fn = timing_handler, .fn_name = "timing_handler"}
};

static inline linux_proc_sampler_metric_info_t find_metric_info_by_name(const char *name);

/* ============ Handlers ============ */

/*
 * Read content of the file (given `path`) into string metric at `idx` in `set`,
 * with maximum length `max_len`.
 *
 * **REMARK**: The metric is not '\0'-terminated.
 */
static int __read_str(ldms_set_t set, int idx, const char *path, int max_len)
{
	int fd, rlen;
	ldms_mval_t str = ldms_metric_get(set, idx);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return errno;
	rlen = read(fd, str->a_char, max_len);
	close(fd);
	return rlen;
}

/* like `__read_str()`, but also '\0'-terminate the string */
static int __read_str0(ldms_set_t set, int idx, const char *path, int max_len)
{
	ldms_mval_t str = ldms_metric_get(set, idx);
	int rlen = __read_str(set, idx, path, max_len);
	if (rlen == max_len)
		rlen = max_len - 1;
	str->a_char[rlen] = '\0';
	return rlen;
}

/* Convenient functions that set `set[midx] = val` if midx > 0 */
static inline void __may_set_u64(ldms_set_t set, int midx, uint64_t val)
{
	if (midx > 0)
		ldms_metric_set_u64(set, midx, val);
}

static inline void __may_set_char(ldms_set_t set, int midx, char val)
{
	if (midx > 0)
		ldms_metric_set_char(set, midx, val);
}

static inline void __may_set_str(ldms_set_t set, int midx, const char *s)
{
	ldms_mval_t mval;
	int len;
	if (midx <= 0)
		return;
	mval = ldms_metric_get(set, midx);
	len = ldms_metric_array_get_len(set, midx);
	snprintf(mval->a_char, len, "%s", s);
}

/* reformat nul-delimited argv per sep given.
 * return new len, or -1 if sep is invalid.
 * bsiz maximum space available in b.
 * len space used in b and nul terminated.
 * sep: a character or character code
 */
static int quote_argv(linux_proc_sampler_inst_t inst, int len, char *b, int bsiz, const char *sep)
{
	int i = 0;
	if (!sep || !strlen(sep))
		return len; /* unmodified nul sep */
	if ( strlen(sep) == 1) {
		for ( ; i < (len-1); i++)
			if (b[i] == '\0')
				b[i] = sep[0];
		return len;
	}
	char csep = '\0';
	if (strlen(sep) == 2 && sep[0] == '\\') {
		switch (sep[1]) {
		case '0':
			return len;
		case 'b':
			csep  = ' ';
			break;
		case 't':
			csep  = '\t';
			break;
		case 'n':
			csep  = '\n';
			break;
		case 'v':
			csep  = '\v';
			break;
		case 'r':
			csep  = '\r';
			break;
		case 'f':
			csep  = '\f';
			break;
		default:
			return -1;
		}
	}
	for (i = 0; i < (len-1); i++)
		if (b[i] == '\0')
			b[i] = csep;
	return len;
}

static int check_sep(linux_proc_sampler_inst_t inst, const char *sep)
{
	INST_LOG(inst, LDMSD_LDEBUG,"check_sep: %s\n", sep);
	char testb[6] = "ab\0cd";
	if (quote_argv(inst, 6, testb, 6, sep) == -1) {
		return -1;
	}
	return 0;
}

static int cmdline_handler(linux_proc_sampler_inst_t inst, pid_t pid, ldms_set_t set)
{
	/* populate `cmdline` and `cmdline_len` */
	ldms_mval_t cmdline;
	int len;
	char path[CMDLINE_SZ];
	cmdline = ldms_metric_get(set, inst->metric_idx[APP_CMDLINE]);
	if (cmdline->a_char[0])
		return 0; /* already set */
	snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
	len = __read_str(set, inst->metric_idx[APP_CMDLINE], path, CMDLINE_SZ);
	cmdline->a_char[CMDLINE_SZ - 1] = 0; /* in case len == CMDLINE_SZ */
	len = quote_argv(inst, len, cmdline->a_char, CMDLINE_SZ, inst->argv_sep);
	ldms_metric_set_u64(set, inst->metric_idx[APP_CMDLINE_LEN], len);
	return 0;
}

static int n_open_files_handler(linux_proc_sampler_inst_t inst, pid_t pid,
				ldms_set_t set)
{
	/* populate n_open_files */
	char path[PROCPID_SZ];
	DIR *dir;
	struct dirent *dent;
	int n;
	snprintf(path, sizeof(path), "/proc/%d/fd", pid);
	dir = opendir(path);
	if (!dir)
		return errno;
	n = 0;
	while ((dent = readdir(dir))) {
		if (strcmp(dent->d_name, ".") == 0)
			continue; /* skip self */
		if (strcmp(dent->d_name, "..") == 0)
			continue; /* skip parent */
		n += 1;
	}
	ldms_metric_set_u64(set, inst->metric_idx[APP_N_OPEN_FILES], n);
	closedir(dir);
	return 0;
}

static int io_handler(linux_proc_sampler_inst_t inst, pid_t pid, ldms_set_t set)
{
	/* populate io_* */
	char path[PROCPID_SZ];
	uint64_t val[7];
	int n, rc;
	snprintf(path, sizeof(path), "/proc/%d/io", pid);
	FILE *f = fopen(path, "r");
	if (!f)
		return errno;
	n = fscanf(f,   "rchar: %lu\n"
			"wchar: %lu\n"
			"syscr: %lu\n"
			"syscw: %lu\n"
			"read_bytes: %lu\n"
			"write_bytes: %lu\n"
			"cancelled_write_bytes: %lu\n",
			val+0, val+1, val+2, val+3, val+4, val+5, val+6);
	if (n != 7) {
		rc = EINVAL;
		goto out;
	}
	__may_set_u64(set, inst->metric_idx[APP_IO_READ_B]	   , val[0]);
	__may_set_u64(set, inst->metric_idx[APP_IO_WRITE_B]	  , val[1]);
	__may_set_u64(set, inst->metric_idx[APP_IO_N_READ]	   , val[2]);
	__may_set_u64(set, inst->metric_idx[APP_IO_N_WRITE]	  , val[3]);
	__may_set_u64(set, inst->metric_idx[APP_IO_READ_DEV_B]       , val[4]);
	__may_set_u64(set, inst->metric_idx[APP_IO_WRITE_DEV_B]      , val[5]);
	__may_set_u64(set, inst->metric_idx[APP_IO_WRITE_CANCELLED_B], val[6]);
	rc = 0;
 out:
	fclose(f);
	return rc;
}

static int oom_score_handler(linux_proc_sampler_inst_t inst, pid_t pid, ldms_set_t set)
{
	/* according to `proc_oom_score()` in Linux kernel src tree, oom_score
	 * is `unsigned long` */
	char path[PROCPID_SZ];
	uint64_t x;
	int n;
	snprintf(path, sizeof(path), "/proc/%d/oom_score", pid);
	FILE *f = fopen(path, "r");
	if (!f)
		return errno;
	n = fscanf(f, "%lu", &x);
	fclose(f);
	if (n != 1)
		return EINVAL;
	ldms_metric_set_u64(set, inst->metric_idx[APP_OOM_SCORE], x);
	return 0;
}

static int oom_score_adj_handler(linux_proc_sampler_inst_t inst, pid_t pid, ldms_set_t set)
{
	/* according to `proc_oom_score_adj_read()` in Linux kernel src tree,
	 * oom_score_adj is `short` */
	char path[PROCPID_SZ];
	int x;
	int n;
	snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
	FILE *f = fopen(path, "r");
	if (!f)
		return errno;
	n = fscanf(f, "%d", &x);
	fclose(f);
	if (n != 1)
		return EINVAL;
	ldms_metric_set_s64(set, inst->metric_idx[APP_OOM_SCORE_ADJ], x);
	return 0;
}

static int root_handler(linux_proc_sampler_inst_t inst, pid_t pid, ldms_set_t set)
{
	char path[PROCPID_SZ];
	ssize_t len;
	int midx = inst->metric_idx[APP_ROOT];
	assert(midx > 0);
	ldms_mval_t mval = ldms_metric_get(set, midx);
	int alen = ldms_metric_array_get_len(set, midx);
	/* /proc/<PID>/root is a soft link */
	snprintf(path, sizeof(path), "/proc/%d/root", pid);
	len = readlink(path, mval->a_char, alen - 1);
	if (len < 0) {
		mval->a_char[0] = '\0';
		return errno;
	}
	mval->a_char[len] = '\0';
	return 0;
}

static int stat_handler(linux_proc_sampler_inst_t inst, pid_t pid, ldms_set_t set)
{
	char buff[CMDLINE_SZ];
	char *str;
	char name[128]; /* should be enough to hold program name */
	int off, n;
	uint64_t val;
	char state;
	pid_t _pid;
	linux_proc_sampler_metric_e code;
	FILE *f;
	snprintf(buff, sizeof(buff), "/proc/%d/stat", pid);
	f = fopen(buff, "r");
	if (!f)
		return errno;
	char *s;
	errno = 0;
	s = fgets(buff, sizeof(buff), f);
	fclose(f);
	str = buff;
	if (s != buff) {
		if (errno) {
			INST_LOG(inst, LDMSD_LDEBUG,
				"error reading /proc/%d/stat %s\n", pid, STRERROR(errno));
			return errno;
		}
		return EINVAL;
	}
	n = sscanf(str, "%d (%[^)]) %c%n", &_pid, name, &state, &off);
	if (n != 3)
		return EINVAL;
	str += off;
	if (_pid != pid)
		return EINVAL; /* should not happen */
	__may_set_u64(set, inst->metric_idx[APP_STAT_PID], _pid);
	__may_set_str(set, inst->metric_idx[APP_STAT_COMM], name);
	__may_set_char(set, inst->metric_idx[APP_STAT_STATE], state);
	for (code = APP_STAT_PPID; code <= _APP_STAT_LAST; code++) {
		n = sscanf(str, "%lu%n", &val, &off);
		if (n != 1)
			return EINVAL;
		str += off;
		__may_set_u64(set, inst->metric_idx[code], val);
	}
	return 0;
}

typedef struct status_line_handler_s {
	const char *key;
	linux_proc_sampler_metric_e code;
	int (*fn)(linux_proc_sampler_inst_t inst, ldms_set_t set, const char *linebuf,
		  linux_proc_sampler_metric_e code);
} *status_line_handler_t;

int __line_dec(linux_proc_sampler_inst_t inst, ldms_set_t set, const char *linebuf, linux_proc_sampler_metric_e code);
int __line_dec_array(linux_proc_sampler_inst_t inst, ldms_set_t set, const char *linebuf, linux_proc_sampler_metric_e code);
int __line_hex(linux_proc_sampler_inst_t inst, ldms_set_t set, const char *linebuf, linux_proc_sampler_metric_e code);
int __line_oct(linux_proc_sampler_inst_t inst, ldms_set_t set, const char *linebuf, linux_proc_sampler_metric_e code);
int __line_char(linux_proc_sampler_inst_t inst, ldms_set_t set, const char *linebuf, linux_proc_sampler_metric_e code);
int __line_sigq(linux_proc_sampler_inst_t inst, ldms_set_t set, const char *linebuf, linux_proc_sampler_metric_e code);
int __line_str(linux_proc_sampler_inst_t inst, ldms_set_t set, const char *linebuf, linux_proc_sampler_metric_e code);
int __line_bitmap(linux_proc_sampler_inst_t inst, ldms_set_t set, const char *linebuf, linux_proc_sampler_metric_e code);

static status_line_handler_t find_status_line_handler(const char *key);

/* the following are not optional */
static char *metrics_always[] = {
	"job_id",
	"task_rank",
	"start_time",
	"start_tick",
	"is_thread",
	"parent",
	"exe"
};

struct metric_sub {
	char *alias;
	char *name;
};

static struct metric_sub alii[] = {
	{"parent_pid", "nothing"},
	{"ppid", "nothing"},
	{"os_pid", "stat_pid"},
	{"pid", "stat_pid"}
};

/* This table will later be sorted alphabetically in __init__() */
struct status_line_handler_s status_line_tbl[] = {
/* linux/fs/proc/array.c:task_state() */
	{ "Name", APP_STATUS_NAME, __line_str},
	{ "Umask", APP_STATUS_UMASK, __line_oct},
	{ "State", APP_STATUS_STATE,__line_char },
	{ "Tgid", APP_STATUS_TGID, __line_dec},
	{ "Ngid", APP_STATUS_NGID, __line_dec},
	{ "Pid", APP_STATUS_PID, __line_dec},
	{ "PPid", APP_STATUS_PPID, __line_dec},
	{ "TracerPid", APP_STATUS_TRACERPID, __line_dec},
	{ "Uid", APP_STATUS_UID, __line_dec_array},
	{ "Gid", APP_STATUS_GID, __line_dec_array},
	{ "FDSize", APP_STATUS_FDSIZE, __line_dec},
	{ "Groups", APP_STATUS_GROUPS, __line_dec_array},
	{ "NStgid", APP_STATUS_NSTGID, __line_dec_array},
	{ "NSpid", APP_STATUS_NSPID, __line_dec_array},
	{ "NSpgid", APP_STATUS_NSPGID, __line_dec_array},
	{ "NSsid", APP_STATUS_NSSID, __line_dec_array},

	/* linux/fs/proc/task_mmu.c:task_mem() */
	{ "VmPeak", APP_STATUS_VMPEAK, __line_dec},
	{ "VmSize", APP_STATUS_VMSIZE, __line_dec},
	{ "VmLck", APP_STATUS_VMLCK, __line_dec},
	{ "VmPin", APP_STATUS_VMPIN, __line_dec},
	{ "VmHWM", APP_STATUS_VMHWM, __line_dec},
	{ "VmRSS", APP_STATUS_VMRSS, __line_dec},
	{ "RssAnon", APP_STATUS_RSSANON, __line_dec},
	{ "RssFile", APP_STATUS_RSSFILE, __line_dec},
	{ "RssShmem", APP_STATUS_RSSSHMEM, __line_dec},
	{ "VmData", APP_STATUS_VMDATA, __line_dec},
	{ "VmStk", APP_STATUS_VMSTK, __line_dec},
	{ "VmExe", APP_STATUS_VMEXE, __line_dec},
	{ "VmLib", APP_STATUS_VMLIB, __line_dec},
	{ "VmPTE", APP_STATUS_VMPTE, __line_dec},
	{ "VmPMD", APP_STATUS_VMPMD, __line_dec},
	{ "VmSwap", APP_STATUS_VMSWAP, __line_dec},
	{ "HugetlbPages", APP_STATUS_HUGETLBPAGES, __line_dec},

	/* linux/fs/proc/array.c:task_core_dumping() */
	{ "CoreDumping", APP_STATUS_COREDUMPING, __line_dec},

	/* linux/fs/proc/array.c:task_sig() */
	{ "Threads", APP_STATUS_THREADS, __line_dec},
	{ "SigQ", APP_STATUS_SIG_QUEUED, __line_sigq},
	{ "SigPnd", APP_STATUS_SIGPND, __line_hex},
	{ "ShdPnd", APP_STATUS_SHDPND, __line_hex},
	{ "SigBlk", APP_STATUS_SIGBLK, __line_hex},
	{ "SigIgn", APP_STATUS_SIGIGN, __line_hex},
	{ "SigCgt", APP_STATUS_SIGCGT, __line_hex},

	/* linux/fs/proc/array.c:task_cap() */
	{ "CapInh", APP_STATUS_CAPINH, __line_hex},
	{ "CapPrm", APP_STATUS_CAPPRM, __line_hex},
	{ "CapEff", APP_STATUS_CAPEFF, __line_hex},
	{ "CapBnd", APP_STATUS_CAPBND, __line_hex},
	{ "CapAmb", APP_STATUS_CAPAMB, __line_hex},

	/* linux/fs/proc/array.c:task_seccomp() */
	{ "NoNewPrivs", APP_STATUS_NONEWPRIVS, __line_dec},
	{ "Seccomp", APP_STATUS_SECCOMP, __line_dec},
	{ "Speculation_Store_Bypass", APP_STATUS_SPECULATION_STORE_BYPASS, __line_str},

	/* linux/fs/proc/array.c:task_cpus_allowed() */
	{ "Cpus_allowed", APP_STATUS_CPUS_ALLOWED, __line_bitmap},
	{ "Cpus_allowed_list", APP_STATUS_CPUS_ALLOWED_LIST, __line_str},

	/* linux/kernel/cgroup/cpuset.c:cpuset_task_status_allowed() */
	{ "Mems_allowed", APP_STATUS_MEMS_ALLOWED, __line_bitmap},
	{ "Mems_allowed_list", APP_STATUS_MEMS_ALLOWED_LIST, __line_str},

	/* linux/fs/proc/array.c:task_context_switch_counts() */
	{ "voluntary_ctxt_switches", APP_STATUS_VOLUNTARY_CTXT_SWITCHES, __line_dec},
	{ "nonvoluntary_ctxt_switches", APP_STATUS_NONVOLUNTARY_CTXT_SWITCHES, __line_dec},
};

static int status_handler(linux_proc_sampler_inst_t inst, pid_t pid, ldms_set_t set)
{
	char buff[CMDLINE_SZ];
	FILE *f;
	char *line, *key, *ptr;
	status_line_handler_t sh;

	snprintf(buff, sizeof(buff), "/proc/%d/status", pid);
	f = fopen(buff, "r");
	if (!f)
		return errno;
	while ((line = fgets(buff, sizeof(buff), f))) {
		key = strtok_r(line, ":", &ptr);
		sh = find_status_line_handler(key);
		if (!sh)
			continue;
		while (isspace(*ptr)) {
			ptr++;
		}
		ptr[strlen(ptr)-1] = '\0'; /* eliminate trailing space */
		if (inst->metric_idx[sh->code] > 0
				|| sh->code == APP_STATUS_SIG_QUEUED) {
			sh->fn(inst, set, ptr, sh->code);
		}
	}
	fclose(f);
	return 0;
}

int __line_dec(linux_proc_sampler_inst_t inst, ldms_set_t set,
		const char *linebuf, linux_proc_sampler_metric_e code)
{
	/* scan for a uint64_t */
	int n;
	uint64_t x;
	n = sscanf(linebuf, "%ld", &x);
	if (n != 1)
		return EINVAL;
	ldms_metric_set_u64(set, inst->metric_idx[code], x);
	return 0;
}

int __line_dec_array(linux_proc_sampler_inst_t inst, ldms_set_t set,
		const char *linebuf, linux_proc_sampler_metric_e code)
{
	int i, n, alen, pos;
	int midx = inst->metric_idx[code];
	const char *ptr = linebuf;
	uint64_t val;
	alen = ldms_metric_array_get_len(set, midx);
	for (i = 0; i < alen; i++) {
		n = sscanf(ptr, "%lu%n", &val, &pos);
		if (n != 1)
			break;
		ldms_metric_array_set_u64(set, midx, i, val);
		ptr += pos;
	}
	return 0;
}

int __line_hex(linux_proc_sampler_inst_t inst, ldms_set_t set,
		const char *linebuf, linux_proc_sampler_metric_e code)
{
	int n;
	uint64_t x;
	n = sscanf(linebuf, "%lx", &x);
	if (n != 1)
		return EINVAL;
	ldms_metric_set_u64(set, inst->metric_idx[code], x);
	return 0;
}

int __line_oct(linux_proc_sampler_inst_t inst, ldms_set_t set,
		const char *linebuf, linux_proc_sampler_metric_e code)
{
	int n;
	uint64_t x;
	n = sscanf(linebuf, "%lo", &x);
	if (n != 1)
		return EINVAL;
	ldms_metric_set_u64(set, inst->metric_idx[code], x);
	return 0;
}

int __line_char(linux_proc_sampler_inst_t inst, ldms_set_t set,
		const char *linebuf, linux_proc_sampler_metric_e code)
{
	int n;
	char x;
	n = sscanf(linebuf, "%c", &x);
	if (n != 1)
		return EINVAL;
	ldms_metric_set_char(set, inst->metric_idx[code], x);
	return 0;
}

int __line_sigq(linux_proc_sampler_inst_t inst, ldms_set_t set,
		const char *linebuf, linux_proc_sampler_metric_e code)
{
	uint64_t q, l;
	int n;
	n = sscanf(linebuf, "%lu/%lu", &q, &l);
	if (n != 2)
		return EINVAL;
	__may_set_u64(set, inst->metric_idx[APP_STATUS_SIG_QUEUED], q);
	__may_set_u64(set, inst->metric_idx[APP_STATUS_SIG_LIMIT] , l);
	return 0;
}

int __line_str(linux_proc_sampler_inst_t inst, ldms_set_t set, const char *linebuf,
		linux_proc_sampler_metric_e code)
{
	int midx = inst->metric_idx[code];
	int alen = ldms_metric_array_get_len(set, midx);
	ldms_mval_t mval = ldms_metric_get(set, midx);
	snprintf(mval->a_char, alen, "%s", linebuf);
	ldms_metric_array_set_char(set, midx, alen-1, '\0'); /* to update data generation number */
	return 0;
}

int __line_bitmap(linux_proc_sampler_inst_t inst, ldms_set_t set, const char *linebuf,
		  linux_proc_sampler_metric_e code)
{
	/* line format: <4-byte-hex>,<4-byte-hex>,...<4-byte-hex>
	 * line ordering: high-byte ... low-byte
	 * ldms array ordering: low-byte .. high-byte
	 * high-byte will be truncated if the ldms array is too short.
	 */
	const char *s;
	int n = strlen(linebuf);
	int midx = inst->metric_idx[code];
	int alen = ldms_metric_array_get_len(set, midx);
	int i, val;
	for (i = 0; i < alen && n; i++) {
		/* reverse scan */
		s = memrchr(linebuf, ',', n);
		if (!s) {
			s = linebuf;
			sscanf(s, "%x", &val);
		} else {
			sscanf(s, ",%x", &val);
		}
		ldms_metric_array_set_u32(set, midx, i, val);
		n = s - linebuf;
	}
	return 0;
}

static int syscall_handler(linux_proc_sampler_inst_t inst, pid_t pid, ldms_set_t set)
{
	char buff[CMDLINE_SZ];
	FILE *f;
	char *ret;
	int i, n;
	uint64_t val[9] = {0};
	snprintf(buff, sizeof(buff), "/proc/%d/syscall", pid);
	/*
	 * NOTE: The file contains single line wcich could be:
	 * - "running": the process is running.
	 * - "-1 <STACK_PTR> <PROGRAM_CTR>": the process is blocked, but not in
	 *   a syscall.
	 * - "<SYSCALL_NUM> <ARG0> ... <ARG5> <STACK_PTR> <PROGRAM_CTR>": the
	 *   syscall number, 6 arguments, stack pointer and program counter.
	 */
	f = fopen(buff, "r");
	if (!f)
		return errno;
	ret = fgets(buff, sizeof(buff), f);
	fclose(f);
	if (!ret)
		return errno;
	if (0 == strncmp(buff, "running", 7)) {
		n = 0;
	} else {
		n = sscanf(buff, "%ld %lx %lx %lx %lx %lx %lx %lx %lx",
				 val+0, val+1, val+2, val+3, val+4,
				 val+5, val+6, val+7, val+8);
	}
	for (i = 0; i < n; i++) {
		ldms_metric_array_set_u64(set, inst->metric_idx[APP_SYSCALL],
					  i, val[i]);
	}
	for (i = n; i < 9; i++) {
		/* zero */
		ldms_metric_array_set_u64(set, inst->metric_idx[APP_SYSCALL],
					  i, 0);
	}
	return 0;
}

static int timerslack_ns_handler(linux_proc_sampler_inst_t inst, pid_t pid, ldms_set_t set)
{
	char path[PROCPID_SZ];
	uint64_t x;
	int n;
	snprintf(path, sizeof(path), "/proc/%d/timerslack_ns", pid);
	FILE *f = fopen(path, "r");
	if (!f && errno != ENOENT)
		return errno;
	if (!f) {
		ldms_metric_set_u64(set, inst->metric_idx[APP_TIMERSLACK_NS], 0);
		return 0;
	}
	n = fscanf(f, "%lu", &x);
	fclose(f);
	if (n != 1)
		return EINVAL;
	ldms_metric_set_u64(set, inst->metric_idx[APP_TIMERSLACK_NS], x);
	return 0;
}

static int wchan_handler(linux_proc_sampler_inst_t inst, pid_t pid, ldms_set_t set)
{
	char path[PROCPID_SZ];
	snprintf(path, sizeof(path), "/proc/%d/wchan", pid);
	__read_str0(set, inst->metric_idx[APP_WCHAN], path, WCHAN_SZ);
	return 0;
}


static int timing_handler(linux_proc_sampler_inst_t inst, pid_t pid, ldms_set_t set)
{
	struct timeval t2;
	gettimeofday(&t2, NULL);
	uint64_t x_us = (t2.tv_sec - inst->sample_start.tv_sec)*1000000;
	int64_t d_us = (int64_t)t2.tv_usec - (int64_t)inst->sample_start.tv_usec;
	if (d_us < 0)
		x_us += (uint64_t)(1000000 + d_us);
	else
		x_us += (uint64_t) d_us;

	ldms_metric_set_u64(set, inst->metric_idx[APP_TIMING], x_us);
	inst->sample_start.tv_sec = 0;
	inst->sample_start.tv_usec = 0;
#ifdef LPDEBUG
	INST_LOG(inst, LDMSD_LDEBUG, "In %" PRIu64 " microseconds\n", x_us);
#endif
	return 0;
}


/* ============== Sampler Plugin APIs ================= */

static int
linux_proc_sampler_update_schema(linux_proc_sampler_inst_t inst, ldms_schema_t schema)
{
	int i, idx;
	linux_proc_sampler_metric_info_t mi;

	inst->task_rank_idx = ldms_schema_meta_add(schema, "task_rank",
						   LDMS_V_U64);
	inst->start_time_idx = ldms_schema_meta_array_add(schema, "start_time",
						LDMS_V_CHAR_ARRAY, 20);
	inst->start_tick_idx = ldms_schema_meta_add(schema, "start_tick",
						LDMS_V_U64);
	inst->is_thread_idx = ldms_schema_meta_add(schema, "is_thread",
						LDMS_V_U8);
	inst->parent_pid_idx = ldms_schema_meta_add(schema, "parent",
						LDMS_V_S64);
	inst->exe_idx = ldms_schema_meta_array_add(schema, "exe",
						LDMS_V_CHAR_ARRAY, 512);
	if (inst->sc_clk_tck)
		inst->sc_clk_tck_idx = ldms_schema_meta_add(schema, "sc_clk_tck",
						LDMS_V_S64);

	/* Add app metrics to the schema */
	for (i = 1; i <= _APP_LAST; i++) {
		if (!inst->metric_idx[i])
			continue;
		mi = &metric_info[i];
		INST_LOG(inst, LDMSD_LDEBUG, "Add metric %s\n", mi->name);
		if (ldms_type_is_array(mi->mtype)) {
			if (mi->is_meta) {
				idx = ldms_schema_meta_array_add(schema,
						mi->name, mi->mtype,
						mi->array_len);
			} else {
				idx = ldms_schema_metric_array_add(schema,
						mi->name, mi->mtype,
						mi->array_len);
			}
		} else {
			if (mi->is_meta) {
				idx = ldms_schema_meta_add(schema, mi->name,
						mi->mtype);
			} else {
				idx = ldms_schema_metric_add(schema, mi->name,
						mi->mtype);
			}
		}
		if (idx < 0) {
			/* error */
			INST_LOG(inst, LDMSD_LERROR,
				 "Error %d adding metric %s into schema.\n",
				 -idx, mi->name);
			return -idx;
		}
		inst->metric_idx[i] = idx;
	}
	return 0;
}

void app_set_destroy(linux_proc_sampler_inst_t inst, struct linux_proc_sampler_set *a)
{
#ifdef LPDEBUG
	INST_LOG(inst, LDMSD_LDEBUG, "Removing set %s\n",
		ldms_set_instance_name_get(a->set));
	INST_LOG(inst, LDMSD_LDEBUG,"Uncreating key at %p: %" PRIu64 " , %" PRId64 "\n",
		&a->key, a->key.start_tick, a->key.os_pid);
#else
	(void)inst;
#endif
	ldmsd_set_deregister(ldms_set_instance_name_get(a->set), SAMP);
	ldms_set_unpublish(a->set);
	ldms_set_delete(a->set);
	a->key.start_tick = 0;
	a->key.os_pid = 0;
	a->set = NULL;
	free(a);
}

static int linux_proc_sampler_sample(struct ldmsd_sampler *pi)
{
	linux_proc_sampler_inst_t inst = (void*)pi;
	int i, rc;
	struct rbn *rbn;
#ifdef LPDEBUG
	INST_LOG(inst, LDMSD_LDEBUG, "Sampling\n");
#endif
	struct linux_proc_sampler_set *app_set;
	struct set_del_list del_list;
	LIST_INIT(&del_list);
	pthread_mutex_lock(&inst->mutex);
	RBT_FOREACH(rbn, &inst->set_rbt) {
		app_set = container_of(rbn, struct linux_proc_sampler_set, rbn);
		ldms_transaction_begin(app_set->set);
		gettimeofday(&inst->sample_start, NULL);
		for (i = 0; i < inst->n_fn; i++) {
			rc = inst->fn[i].fn(inst, app_set->key.os_pid, app_set->set);
			if (rc) {
				INST_LOG(inst, LDMSD_LDEBUG, "Removing set %s. Error %d(%s) from %s\n",
					ldms_set_instance_name_get(app_set->set),
					rc, STRERROR(rc), inst->fn[i].fn_name);
				LIST_INSERT_HEAD(&del_list, app_set, del);
				app_set->dead = rc;
				break;
			}
		}
#ifdef LPDEBUG
		INST_LOG(inst, LDMSD_LDEBUG, "Got data for %s\n",
			ldms_set_instance_name_get(app_set->set));
#endif
		ldms_transaction_end(app_set->set);
	}
	while (!LIST_EMPTY(&del_list)) {
                app_set = LIST_FIRST(&del_list);
		rbn = rbt_find(&inst->set_rbt, &app_set->key);
		if (rbn)
			rbt_del(&inst->set_rbt, rbn);
                LIST_REMOVE(app_set, del);
                app_set_destroy(inst, app_set);
        }
	pthread_mutex_unlock(&inst->mutex);
	return 0;
}


/* ============== Common Plugin APIs ================= */

static
char *_help = "\
linux_proc_sampler config synopsis: \n\
    config name=linux_proc_sampler [COMMON_OPTIONS] [stream=STREAM]\n\
	    [sc_clk_tck=1] [metrics=METRICS] [cfg_file=FILE] [exe_suffix=1]]\n\
\n\
Option descriptions:\n\
    instance_prefix    The prefix for generated instance names. Typically a cluster name\n\
		when needed to disambiguate producer names that appear in multiple clusters.\n\
	      (default: no prefix).\n\
    exe_suffix  Append executable path to set instance names.\n\
    stream    The name of the `ldmsd_stream` to listen for SLURM job events.\n\
	      (default: slurm).\n\
    sc_clk_tck=1 Include sc_clk_tck, the ticks per second, in the set.\n\
              The default is to exclude sc_clk_tck.\n\
    metrics   The comma-separated list of metrics to monitor.\n\
	      The default is \"\" (empty), which is equivalent to monitor ALL\n\
	      metrics.\n\
    argv_sep  The separator character to replace nul with in the cmdline string.\n\
              Special specifiers \n,\t,\b etc are also supported.\n\
    cfg_file  The alternative config file in JSON format. The file is\n\
	      expected to have an object that contains the following \n\
	      attributes:\n\
	      - \"stream\": \"STREAM_NAME\"\n\
	      - \"metrics\": [ METRICS ]\n\
	      If the `cfg_file` is given, `stream`, `metrics`, 'sc_clk_tck'\n\
	      and 'exe_suffix' options are ignored.\n\
\n\
The sampler creates and destroys sets according to events received from \n\
LDMSD stream. The sets share the same schema which is contructed according \n\
to the given `metrics` option or \"metrics\" in the JSON `cfg_file`.\n\
The sampling frequency is controlled by LDMSD `smplr` object.\n\
\n\
The following is an example of cfg_file:\n\
```\n\
{\n\
  \"stream\": \"slurm\",\n\
  \"instance_prefix\": \"cluster2\",\n\
  \"metrics\": [\n\
    \"stat_comm\",\n\
    \"stat_pid\",\n\
    \"stat_cutime\"\n\
  ]\n\
}\n\
```\n\
\n\
The following metadata metrics are always reported:\n\
name\ttype\tnotes\n\
task_rank u64 The PID or if slurm present the SLURM_TASK_RANK (inherited by child processes). \n\
start_time char[20] The epoch time when the process started, as estimated from the start_tick.\n\
start_tick u64 The node local start time in jiffies from /proc/$pid/stat.\n\
is_thread u8 Boolean value noting if the process is a child thread in e.g. an OMP application.\n\
parent s64 The parent pid of the process.\n\
exe char[] The full path of the executable.\n\
\n\
";

static char *help_all;
static void compute_help() {
	linux_proc_sampler_metric_info_t mi;
	char name_buf[80];
	int i;
	dsinit2(ds, CMDLINE_SZ);
	dscat(ds, _help);
	dscat(ds, "\nThe list of optional metric names and types is:\n");
	for (i = 1; i <= _APP_LAST; i++) {
		mi = &metric_info[i];
		snprintf(name_buf, sizeof name_buf, "\n%10s%s\t%s",
			ldms_metric_type_to_str(mi->mtype),
			(mi->is_meta ? ", meta-data," : ",      data,"),
			mi->name);
		dscat(ds, name_buf);
	}
	dscat(ds, "\nThe following metrics are not optional:\n");
	for (i = 0; i < ARRAY_LEN(metrics_always); i++) {
		dscat(ds, "\t");
		dscat(ds, metrics_always[i]);
		dscat(ds, "\n");
	}
	help_all = dsdone(ds);
	if (!help_all)
		help_all = _help;
}

static const char *linux_proc_sampler_usage(struct ldmsd_plugin *self)
{
	if (!help_all)
		compute_help();
	return help_all;
}

static void missing_metric(linux_proc_sampler_inst_t inst, const char *tkn)
{
	size_t k, ma = ARRAY_LEN(metrics_always);
	for (k = 0; k < ma; k++) {
		if (strcmp(metrics_always[k], tkn) == 0) {
			INST_LOG(inst, LDMSD_LERROR,
			"metric '%s' is not optional. Remove it.\n", tkn);
			return;
		}
	}
	ma = ARRAY_LEN(alii);
	for (k = 0; k < ma; k++) {
		if (strcmp(alii[k].alias, tkn) == 0) {
			INST_LOG(inst, LDMSD_LERROR,
			"metric '%s' is not supported. Replace with %s.\n", tkn, alii[k].name);
			return;
		}
	}
	INST_LOG(inst, LDMSD_LERROR,
		 "optional metric '%s' is unknown.\n", tkn);
}

static
int __handle_cfg_file(linux_proc_sampler_inst_t inst, char *val)
{
	int i, fd, rc = -1, off;
	ssize_t bsz, sz;
	char *buff = NULL;
	json_parser_t jp = NULL;
	json_entity_t jdoc = NULL;
	json_entity_t list, ent;
	linux_proc_sampler_metric_info_t minfo;

	fd = open(val, O_RDONLY);
	if (fd < 0) {
		INST_LOG(inst, LDMSD_LERROR, "Cannot open %s\n", val);
		return errno;
	}
	sz = lseek(fd, 0, SEEK_END);
	if (sz < 0) {
		rc = errno;
		INST_LOG(inst, LDMSD_LERROR,
			 "lseek() failed, errno: %d\n", errno);
		goto out;
	}
	bsz = sz;
	lseek(fd, 0, SEEK_SET);
	buff = malloc(sz+1);
	if (!buff) {
		rc = errno;
		INST_LOG(inst, LDMSD_LERROR, "Out of memory parsing %s\n", val);
		goto out;
	}
	off = 0;
	while (sz) {
		rc = read(fd, buff + off, sz);
		if (rc < 0) {
			rc = errno;
			INST_LOG(inst, LDMSD_LERROR, "read() error: %d in %s\n",
				errno, val);
			goto out;
		}
		off += rc;
		sz -= rc;
	}

	jp = json_parser_new(0);
	if (!jp) {
		rc = errno;
		INST_LOG(inst, LDMSD_LERROR, "json_parser_new() error: %d\n", errno);
		goto out;
	}
	rc = json_parse_buffer(jp, buff, bsz, &jdoc);
	if (rc) {
		INST_LOG(inst, LDMSD_LERROR, "JSON parse failed: %d\n", rc);
		INST_LOG(inst, LDMSD_LINFO, "input from %s was: %s\n", val, buff);
		goto out;
	}

	ent = json_value_find(jdoc, "argv_sep");
	if (ent) {
		if (ent->type != JSON_STRING_VALUE) {
			rc = EINVAL;
			INST_LOG(inst, LDMSD_LERROR, "Error: `argv_sep` must be a string.\n");
			goto out;
		}
		inst->argv_sep = strdup(json_value_cstr(ent));
		if (!inst->argv_sep) {
			rc = ENOMEM;
			INST_LOG(inst, LDMSD_LERROR, "Out of memory while configuring argv_sep\n");
			goto out;
		}
		if (check_sep(inst, inst->argv_sep)) {
			rc = ERANGE;
			INST_LOG(inst, LDMSD_LERROR, "Config argv_sep='%s' is not supported.\n",
				inst->argv_sep);
			goto out;
		}
	}
	ent = json_value_find(jdoc, "instance_prefix");
	if (ent) {
		if (ent->type != JSON_STRING_VALUE) {
			rc = EINVAL;
			INST_LOG(inst, LDMSD_LERROR, "Error: `instance_prefix` must be a string.\n");
			goto out;
		}
		inst->instance_prefix = strdup(json_value_cstr(ent));
		if (!inst->instance_prefix) {
			rc = ENOMEM;
			INST_LOG(inst, LDMSD_LERROR, "Out of memory while configuring.\n");
			goto out;
		}
	}
	ent = json_value_find(jdoc, "exe_suffix");
	if (ent) {
		inst->exe_suffix = 1;
	}
	ent = json_value_find(jdoc, "sc_clk_tck");
	if (ent) {
		inst->sc_clk_tck = sysconf(_SC_CLK_TCK);
		if (!inst->sc_clk_tck) {
			INST_LOG(inst, LDMSD_LERROR, "sysconf(_SC_CLK_TCK) returned 0.\n");
		} else {
			INST_LOG(inst, LDMSD_LINFO, "sysconf(_SC_CLK_TCK) = %ld.\n",
				inst->sc_clk_tck);
		}
	}
	ent = json_value_find(jdoc, "stream");
	if (ent) {
		if (ent->type != JSON_STRING_VALUE) {
			rc = EINVAL;
			INST_LOG(inst, LDMSD_LERROR, "Error: `stream` must be a string.\n");
			goto out;
		}
		inst->stream_name = strdup(json_value_cstr(ent));
		if (!inst->stream_name) {
			rc = ENOMEM;
			INST_LOG(inst, LDMSD_LERROR, "Out of memory while configuring.\n");
			goto out;
		}
	} /* else, caller will later set default stream_name */
	list = json_value_find(jdoc, "metrics");
	if (list) {
		for (ent = json_item_first(list); ent;
					ent = json_item_next(ent)) {
			if (ent->type != JSON_STRING_VALUE) {
				rc = EINVAL;
				INST_LOG(inst, LDMSD_LERROR,
					 "Error: metric must be a string.\n");
				goto out;
			}
			minfo = find_metric_info_by_name(json_value_cstr(ent));
			if (!minfo) {
				rc = ENOENT;
				missing_metric(inst, json_value_cstr(ent));
				goto out;
			}
			inst->metric_idx[minfo->code] = -1;
			/* Non-zero to enable. This will be replaced with the
			 * actual metric index later. */
		}
	} else {
		for (i = _APP_FIRST; i <= _APP_LAST; i++) {
			inst->metric_idx[i] = -1;
		}
	}
	rc = 0;

 out:
	close(fd);
	if (buff)
		free(buff);
	if (jp)
		json_parser_free(jp);
	if (jdoc)
		json_entity_free(jdoc);
	return rc;
}

/* verifies named entity has type et unless et is JSON_NULL_VALUE */
uint64_t get_field_value_u64(linux_proc_sampler_inst_t inst, json_entity_t src, enum json_value_e et, const char *name)
{
	json_entity_t e = json_value_find(src, name);
	if (!e) {
		INST_LOG(inst, LDMSD_LDEBUG, "no json attribute %s found.\n", name);
		errno = ENOKEY;
		return 0;
	}
	if ( e->type != et && et != JSON_NULL_VALUE) {
		INST_LOG(inst, LDMSD_LDEBUG, "wrong type found for %s: %s. Expected %s.\n",
			name, json_type_name(e->type), json_type_name(et));
		errno = EINVAL;
		return 0;
	}
	uint64_t u64;
	switch (et) {
	case JSON_STRING_VALUE:
		if (sscanf(json_value_cstr(e),"%" SCNu64, &u64) == 1) {
			return u64;
		} else {
			INST_LOG(inst, LDMSD_LDEBUG, "unconvertible to uint64_t: %s from %s.\n",
				json_value_cstr(e), name);
			errno = EINVAL;
			return 0;
		}
	case JSON_INT_VALUE:
		if ( json_value_int(e) < 0) {
			INST_LOG(inst, LDMSD_LDEBUG, "unconvertible to uint64_t: %" PRId64 " from %s.\n",
				json_value_int(e), name);
			errno = ERANGE;
			return 0;
		}
		return (uint64_t)json_value_int(e);
	case JSON_FLOAT_VALUE:
		if ( json_value_float(e) < 0 || json_value_float(e) > UINT64_MAX) {
			errno = ERANGE;
			INST_LOG(inst, LDMSD_LDEBUG, "unconvertible to uint64_t: %g from %s.\n",
				json_value_float(e), name);
			return 0;
		}
		return (uint64_t)json_value_float(e);
	default:
		errno = EINVAL;
		return 0;
	}
}

static json_entity_t get_field(linux_proc_sampler_inst_t inst, json_entity_t src, enum json_value_e et, const char *name)
{
#ifndef LPDEBUG
	(void)inst;
#endif
	json_entity_t e = json_value_find(src, name);
	if (!e) {
#ifdef LPDEBUG
		INST_LOG(inst, LDMSD_LDEBUG, "no json attribute %s found.\n", name);
#endif
		return NULL;
	}
	if ( e->type != et) {
#ifdef LPDEBUG
		INST_LOG(inst, LDMSD_LDEBUG, "wrong type found for %s: %s. Expected %s.\n",
			name, json_type_name(e->type), json_type_name(et));
#endif
		return NULL;
	}
	return e;
}

/* functions lifted from forkstat split out here for copyright notice considerations */
#include "get_stat_field.c"

static uint64_t get_start_tick(linux_proc_sampler_inst_t inst, json_entity_t data, int64_t pid)
{
	if (!inst || !data)
		return 0;
	uint64_t start_tick = get_field_value_u64(inst, data, JSON_STRING_VALUE, "start_tick");
	if (start_tick)
		return start_tick;
	char path[PATH_MAX];
	char buf[STAT_COMM_SZ];
	snprintf(path, sizeof(path), "/proc/%" PRId64 "/stat", pid);
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return 0;
	ssize_t n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return 0;
	buf[n] = '\0';
	const char* ptr = get_proc_self_stat_field(buf, 22);
	uint64_t start = 0;
	int n2 = sscanf(ptr, "%" PRIu64, &start);
        if (n2 != 1)
		return 0;
        return start;
}

int __handle_task_init(linux_proc_sampler_inst_t inst, json_entity_t data)
{
	/* create a set per task; deduplicate if multiple spank and linux proc sources
	 * are both active. */
	struct linux_proc_sampler_set *app_set;
	ldms_set_t set;
	int len;
	jbuf_t bjb = NULL;
	json_entity_t os_pid;
	json_entity_t task_pid;
	json_entity_t task_rank;
	json_entity_t exe;
	json_entity_t is_thread;
	json_entity_t parent_pid;
	json_entity_t start;
	char setname[512];
	exe = get_field(inst, data, JSON_STRING_VALUE, "exe");
	start = get_field(inst, data, JSON_STRING_VALUE, "start");
	errno = 0;
	bool job_id = true;
	uint64_t job_id_val =
		get_field_value_u64(inst, data, JSON_NULL_VALUE, "job_id");
	if (errno)
		job_id = false;
	os_pid = get_field(inst, data, JSON_INT_VALUE, "os_pid");
	task_pid = get_field(inst, data, JSON_INT_VALUE, "task_pid");
	parent_pid = get_field(inst, data, JSON_INT_VALUE, "parent_pid");
	is_thread = get_field(inst, data, JSON_INT_VALUE, "is_thread");
	if (!job_id && (!os_pid && !task_pid)) {
		INST_LOG(inst, LDMSD_LINFO, "need job_id or (os_pid & task_pid)\n");
		goto dump;
	}
	pid_t  pid;
	if (os_pid)
		pid = (pid_t)json_value_int(os_pid);
	else
		pid = json_value_int(task_pid); /* from spank plugin */

	bool is_thread_val = false;
	pid_t parent = 0;
	if (is_thread && parent_pid) {
		is_thread_val = json_value_int(is_thread);
		parent = json_value_int(parent_pid);
	} else {
		parent = get_parent_pid(pid, &is_thread_val);
	}
	uint64_t start_tick = get_start_tick(inst, data, pid);
	if (!start_tick) {
		INST_LOG(inst, LDMSD_LDEBUG, "ignoring start-tickless pid %"
			PRId64 "\n", pid);
		return 0;
	}
	const char *start_string;
	char start_string_buf[32];
	if (!start) {
		/* message is not from netlink-notifier. and we have to compensate */
		struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
		get_timeval_from_tick(start_tick, &tv);
		snprintf(start_string_buf, sizeof(start_string_buf),"%lu.%06lu",
			tv.tv_sec, tv.tv_usec);
		start_string = start_string_buf;
	} else {
		start_string = json_value_cstr(start);
	}
	const char *exe_string;
	char exe_buf[CMDLINE_SZ];
	if (!exe) {
		proc_exe_buf(pid, exe_buf, sizeof(exe_buf));
		exe_string = exe_buf;
	} else {
		exe_string = json_value_cstr(exe);
	}
	int64_t task_rank_val = -1;
	task_rank = get_field(inst, data, JSON_INT_VALUE, "task_global_id");
	if (task_rank) {
		task_rank_val = json_value_int(task_rank);
	}
/* set instance is $iprefix/$producer/$jobid/$start_time/$os_pid
 * unless it came from spank with task_global_id set, in which case it is.
 * set instance is $iprefix/$producer/$jobid/$start_time/rank/$task_rank
 */
	const char *esep = "";
	const char *esuffix = "";
	if (inst->exe_suffix) {
		esep = "/";
		esuffix = exe_string;
	}
	if (task_rank_val < 0) {
		/* we haven't seen the pid as a slurm item yet. */
		/* set instance is $iprefix/$producer/$jobid/$start_time/$os_pid */
		len = snprintf(setname, sizeof(setname), "%s%s%s/%" PRIu64
							"/%s/%" PRId64 "%s%s" ,
				(inst->instance_prefix ? inst->instance_prefix : ""),
				(inst->instance_prefix ? "/" : ""),
				inst->base_data->producer_name,
				job_id_val,
				start_string,
				(int64_t)pid, esep, esuffix);
		if (len >= sizeof(setname)) {
			INST_LOG(inst, LDMSD_LERROR, "set name too big: %s%s%s/%" PRIu64
							"/%s/%" PRId64 "%s%s\n",
				(inst->instance_prefix ? inst->instance_prefix : ""),
				(inst->instance_prefix ? "/" : ""),
				inst->base_data->producer_name,
				job_id_val,
				start_string,
				(int64_t)pid, esep, esuffix);
			return ENAMETOOLONG;
		}
	} else {
		/* set instance is $iprefix/$producer/$jobid/$start_time/rank/$task_rank */
		len = snprintf(setname, sizeof(setname), "%s%s%s/%" PRIu64
							"/%s/rank/%" PRId64 "%s%s",
				(inst->instance_prefix ? inst->instance_prefix : ""),
				(inst->instance_prefix ? "/" : ""),
				inst->base_data->producer_name,
				job_id_val,
				start_string,
				task_rank_val, esep, esuffix);
		if (len >= sizeof(setname)) {
			INST_LOG(inst, LDMSD_LERROR, "set name too big: %s%s%s/%" PRIu64
							"/%s/rank/%" PRId64 "%s%s\n",
				(inst->instance_prefix ? inst->instance_prefix : ""),
				(inst->instance_prefix ? "/" : ""),
				inst->base_data->producer_name,
				job_id_val,
				start_string,
				task_rank_val, esep, esuffix);
			return ENAMETOOLONG;
		}
	}
	app_set = calloc(1, sizeof(*app_set));
	if (!app_set)
		return ENOMEM;
	app_set->task_rank = task_rank_val;
	data_set_key(inst, app_set, start_tick, pid);

	set = ldms_set_new(setname, inst->base_data->schema);
	static int warn_once;
	static int warn_once_dup;
	if (!set) {
		int ec = errno;
		if (errno != EEXIST && !warn_once) {
			warn_once = 1;
			INST_LOG(inst, LDMSD_LERROR, "Out of set memory. Consider bigger -m.\n");
			struct mm_stat ms;
			mm_stats(&ms);
			INST_LOG(inst, LDMSD_LERROR, "mm_stat: size=%zu grain=%zu "
				"chunks_free=%zu grains_free=%zu grains_largest=%zu "
				"grains_smallest=%zu bytes_free=%zu bytes_largest=%zu "
				"bytes_smallest=%zu\n",
				ms.size, ms.grain, ms.chunks, ms.bytes, ms.largest,
				ms.smallest, ms.grain*ms.bytes, ms.grain*ms.largest,
				ms.grain*ms.smallest);
		}
		if (ec == EEXIST) {
			if (!warn_once_dup) {
				warn_once_dup = 1;
				INST_LOG(inst, LDMSD_LERROR, "Duplicate set name %s."
					"Check for redundant notifiers running.\n",
					setname);
			}
			ec = 0; /* expected case for misconfigured notifiers */
		}
		free(app_set);
		return ec;
	}
	ldms_set_producer_name_set(set, inst->base_data->producer_name);
	base_auth_set(&inst->base_data->auth, set);
	ldms_metric_set_u64(set, BASE_JOB_ID, job_id_val);
	ldms_metric_set_u64(set, BASE_COMPONENT_ID, inst->base_data->component_id);
	ldms_metric_set_s64(set, inst->task_rank_idx, task_rank_val);
	ldms_metric_array_set_str(set, inst->start_time_idx, start_string);
	ldms_metric_set_u64(set, inst->start_tick_idx, start_tick);
	ldms_metric_set_s64(set, inst->parent_pid_idx, parent);
	ldms_metric_set_u8(set, inst->is_thread_idx, is_thread_val ? 1 : 0);
	ldms_metric_array_set_str(set, inst->exe_idx, exe_string);
	if (inst->sc_clk_tck)
		ldms_metric_set_s64(set, inst->sc_clk_tck_idx, inst->sc_clk_tck);
	app_set->set = set;
	rbn_init(&app_set->rbn, (void*)&app_set->key);

	pthread_mutex_lock(&inst->mutex);
	struct rbn *already = rbt_find(&inst->set_rbt, &app_set->key);
	if (already) {
		struct linux_proc_sampler_set *old_app_set;
		old_app_set = container_of(already, struct linux_proc_sampler_set, rbn);
		if (app_set->task_rank != -1) { /* new is from spank */
			if (old_app_set->task_rank == app_set->task_rank) {
				/*skip spank duplicate */
#ifdef LPDEBUG
				INST_LOG(inst, LDMSD_LDEBUG, "Keeping slurm set %s, dropping duplicate %s\n",
					ldms_set_instance_name_get(old_app_set->set),
					ldms_set_instance_name_get(app_set->set));
#endif
				ldms_set_delete(app_set->set);
				free(app_set);
			} else {
				/* remove/replace set pointer in app_set with newly named set instance */
#ifdef LPDEBUG
				INST_LOG(inst, LDMSD_LDEBUG, "Converting set %s to %s\n",
					ldms_set_instance_name_get(old_app_set->set),
					ldms_set_instance_name_get(set));
#endif
				ldmsd_set_deregister(ldms_set_instance_name_get(old_app_set->set), SAMP);
				ldms_set_unpublish(old_app_set->set);
				ldms_set_delete(old_app_set->set);
				old_app_set->set = set;
				old_app_set->task_rank = task_rank_val;
				ldms_set_publish(set);
				ldmsd_set_register(set, SAMP);
			}
		} else {
			/* keep existing set whether slurm or not. */
			/* unreachable in principle if set_create maintains uniqueness */
#ifdef LPDEBUG
			INST_LOG(inst, LDMSD_LDEBUG, "Keeping existing set %s, dropping %s\n",
				ldms_set_instance_name_get(old_app_set->set),
				ldms_set_instance_name_get(app_set->set));
#endif
			ldms_set_delete(app_set->set);
			free(app_set);
		}
		pthread_mutex_unlock(&inst->mutex);
		return 0;
	}
	rbt_ins(&inst->set_rbt, &app_set->rbn);
#ifdef LPDEBUG
	INST_LOG(inst, LDMSD_LDEBUG, "Adding set %s\n",
		ldms_set_instance_name_get(app_set->set));
#endif
	pthread_mutex_unlock(&inst->mutex);
	ldms_set_publish(set);
	ldmsd_set_register(set, SAMP);
	return 0;
dump:
	bjb = json_entity_dump(bjb, data);
	INST_LOG(inst, LDMSD_LDEBUG, "data was: %s\n", bjb->buf);
	jbuf_free(bjb);
	return EINVAL;
}

int __handle_task_exit(linux_proc_sampler_inst_t inst, json_entity_t data)
{
	struct rbn *rbn;
	struct linux_proc_sampler_set *app_set;
	json_entity_t os_pid;
	os_pid = get_field(inst, data, JSON_INT_VALUE, "os_pid");
	pid_t pid;
	if (!os_pid ) {
		/* from spank plugin */
		json_entity_t task_pid;
		task_pid = get_field(inst, data, JSON_INT_VALUE, "task_pid");
		pid = (pid_t)json_value_int(task_pid);
	} else {
		pid = (pid_t)json_value_int(os_pid);
	}
	uint64_t start_tick = get_start_tick(inst, data, pid);
	if (!start_tick)
		return EINVAL;
	struct linux_proc_sampler_set app_set_search;
	data_set_key(inst, &app_set_search, start_tick, pid);
	pthread_mutex_lock(&inst->mutex);
	rbn = rbt_find(&inst->set_rbt, (void*)&app_set_search.key);
	if (!rbn) {
		pthread_mutex_unlock(&inst->mutex);
		return 0; /* exit occuring of process we didn't catch the start of. */
	}
	rbt_del(&inst->set_rbt, rbn);
	pthread_mutex_unlock(&inst->mutex);
	app_set = container_of(rbn, struct linux_proc_sampler_set, rbn);
	app_set_destroy(inst, app_set);
	return 0;
}

static int __stream_cb(ldmsd_stream_client_t c, void *ctxt,
		ldmsd_stream_type_t stream_type,
		const char *msg, size_t msg_len, json_entity_t entity)
{
	int rc;
	linux_proc_sampler_inst_t inst = ctxt;
	json_entity_t event, data;
	const char *event_name;

	if (stream_type != LDMSD_STREAM_JSON) {
		INST_LOG(inst, LDMSD_LDEBUG, "Unexpected stream type data...ignoring\n");
		INST_LOG(inst, LDMSD_LDEBUG, "%s\n", msg);
		rc = EINVAL;
		goto err;
	}

	event = get_field(inst, entity, JSON_STRING_VALUE, "event");
	if (!event) {
		rc = ENOENT;
		goto err;
	}
	event_name = json_value_cstr(event);
	data = json_value_find(entity, "data");
	if (!data) {
		INST_LOG(inst, LDMSD_LERROR,
			 "'%s' event is missing the 'data' attribute\n",
			 event_name);
		rc = ENOENT;
		goto err;
	}
	if (0 == strcmp(event_name, "task_init_priv")) {
		rc = __handle_task_init(inst, data);
		if (rc) {
			INST_LOG(inst, LDMSD_LERROR,
				"failed to process task_init: %d: %s\n",
				rc, STRERROR(rc));
			goto err;
		}
	} else if (0 == strcmp(event_name, "task_exit")) {
		rc = __handle_task_exit(inst, data);
		if (rc) {
			INST_LOG(inst, LDMSD_LERROR,
				"failed to process task_exit: %d: %s\n",
				rc, STRERROR(rc));
			goto err;
		}
	}
	rc = 0;
	goto out;
 err:
#ifdef LPDEBUG
	INST_LOG(inst, LDMSD_LDEBUG, "Doing nothing with msg %s.\n",
		msg);
#endif
 out:
	return rc;
}

static void linux_proc_sampler_term(struct ldmsd_plugin *pi);

static int
linux_proc_sampler_config(struct ldmsd_plugin *pi, struct attr_value_list *kwl,
					    struct attr_value_list *avl)
{
	linux_proc_sampler_inst_t inst = (void*)pi;
	int i, rc;
	linux_proc_sampler_metric_info_t minfo;
	char *val;

	if (inst->base_data) {
		/* already configured */
		INST_LOG(inst, LDMSD_LERROR, "already configured.\n");
		return EALREADY;
	}

	inst->base_data = base_config(avl, SAMP, SAMP, inst->log);
	if (!inst->base_data) {
		/* base_config() already log error message */
		return errno;
	}
	INST_LOG(inst, LDMSD_LDEBUG, "configuring.\n");

	/* Plugin-specific config here */
	val = av_value(avl, "cfg_file");
	if (val) {
		rc = __handle_cfg_file(inst, val);
		if (rc)
			goto err;
	} else {
		val = av_value(avl, "instance_prefix");
		if (val) {
			inst->instance_prefix = strdup(val);
			if (!inst->instance_prefix) {
				INST_LOG(inst, LDMSD_LERROR, "Config out of memory for instance_prefix\n");
				rc = ENOMEM;
				goto err;
			}
		}
		val = av_value(avl, "argv_sep");
		if (val) {
			inst->argv_sep = strdup(val);
			if (!inst->argv_sep) {
				INST_LOG(inst, LDMSD_LERROR, "Config out of memory for arg_sep\n");
				rc = ENOMEM;
				goto err;
			}
			if (check_sep(inst, inst->argv_sep)) {
				rc = ERANGE;
				INST_LOG(inst, LDMSD_LERROR, "Config argv_sep='%s' not supported.\n",
					inst->argv_sep );
				goto err;
			}
		}
		val = av_value(avl, "exe_suffix");
		if (val) {
			inst->exe_suffix = true;
		}
		val = av_value(avl, "sc_clk_tck");
		if (val) {
			inst->sc_clk_tck = sysconf(_SC_CLK_TCK);
		}
		val = av_value(avl, "stream");
		if (val) {
			inst->stream_name = strdup(val);
			if (!inst->stream_name) {
				INST_LOG(inst, LDMSD_LERROR, "Config out of memory for stream\n");
				rc = ENOMEM;
				goto err;
			}
		}
		val = av_value(avl, "metrics");
		if (val) {
			char *tkn, *ptr;
			tkn = strtok_r(val, ",", &ptr);
			while (tkn) {
				minfo = find_metric_info_by_name(tkn);
				if (!minfo) {
					rc = ENOENT;
					missing_metric(inst, tkn);
					goto err;
				}
				inst->metric_idx[minfo->code] = -1;
				tkn = strtok_r(NULL, ",", &ptr);
			}
		} else {
			for (i = _APP_FIRST; i <= _APP_LAST; i++) {
				inst->metric_idx[i] = -1;
			}
		}
	}

	/* default stream */
	if (!inst->stream_name) {
		inst->stream_name = strdup("slurm");
		if (!inst->stream_name) {
			INST_LOG(inst, LDMSD_LERROR, "Config: out of memory for default stream\n");
			rc = ENOMEM;
			goto err;
		}
	}

	inst->n_fn = 0;
	for (i = _APP_FIRST; i <= _APP_LAST; i++) {
		if (inst->metric_idx[i] == 0)
			continue;
		if (inst->n_fn && inst->fn[inst->n_fn-1].fn == handler_info_tbl[i].fn)
			continue; /* already added */
		/* add the handler */
		inst->fn[inst->n_fn] = handler_info_tbl[i];
		inst->n_fn++;
	}

	/* create schema */
	if (!base_schema_new(inst->base_data)) {
		INST_LOG(inst, LDMSD_LERROR, "Out of memory making schema\n");
		rc = errno;
		goto err;
	}
	rc = linux_proc_sampler_update_schema(inst, inst->base_data->schema);
	if (rc)
		goto err;

	/* subscribe to the stream */
	inst->stream = ldmsd_stream_subscribe(inst->stream_name, __stream_cb, inst);
	if (!inst->stream) {
		INST_LOG(inst, LDMSD_LERROR,
			 "Error subcribing to stream `%s`: %d\n",
			 inst->stream_name, errno);
		rc = errno;
		goto err;
	}

	return 0;

 err:
	/* undo the config */
	linux_proc_sampler_term(pi);
	return rc;
}

static
void linux_proc_sampler_term(struct ldmsd_plugin *pi)
{
	linux_proc_sampler_inst_t inst = (void*)pi;
	struct rbn *rbn;
	struct linux_proc_sampler_set *app_set;

	if (inst->stream)
		ldmsd_stream_close(inst->stream);
	pthread_mutex_lock(&inst->mutex);
	while ((rbn = rbt_min(&inst->set_rbt))) {
		rbt_del(&inst->set_rbt, rbn);
		app_set = container_of(rbn, struct linux_proc_sampler_set, rbn);
		app_set_destroy(inst, app_set);
	}
	pthread_mutex_unlock(&inst->mutex);
	free(inst->instance_prefix);
	inst->instance_prefix = NULL;
	free(inst->stream_name);
	inst->stream_name = NULL;
	free(inst->argv_sep);
	inst->argv_sep = NULL;
	if (inst->base_data)
		base_del(inst->base_data);
	bzero(inst->fn, sizeof(inst->fn));
	inst->n_fn = 0;
	bzero(inst->metric_idx, sizeof(inst->metric_idx));
}

static int
idx_cmp_by_name(const void *a, const void *b)
{
	/* a, b are pointer to linux_proc_sampler_metric_info_t */
	const linux_proc_sampler_metric_info_t *_a = a, *_b = b;
	return strcmp((*_a)->name, (*_b)->name);
}

static int
idx_key_cmp_by_name(const void *key, const void *ent)
{
	const linux_proc_sampler_metric_info_t *_ent = ent;
	return strcmp(key, (*_ent)->name);
}

static inline linux_proc_sampler_metric_info_t
find_metric_info_by_name(const char *name)
{
	linux_proc_sampler_metric_info_t *minfo = bsearch(name, metric_info_idx_by_name,
			_APP_LAST + 1, sizeof(linux_proc_sampler_metric_info_t),
			idx_key_cmp_by_name);
	return minfo?(*minfo):(NULL);
}

static int
status_line_cmp(const void *a, const void *b)
{
	const struct status_line_handler_s *_a = a, *_b = (void*)b;
	return strcmp(_a->key, _b->key);
}

static int
status_line_key_cmp(const void *key, const void *ent)
{
	const struct status_line_handler_s *_ent = ent;
	return strcmp(key, _ent->key);
}

static status_line_handler_t
find_status_line_handler(const char *key)
{
	return bsearch(key, status_line_tbl, ARRAY_LEN(status_line_tbl),
			sizeof(status_line_tbl[0]), status_line_key_cmp);
}

static
struct linux_proc_sampler_inst_s __inst = {
	.samp = {
		.base = {
			.name = SAMP,
			.type = LDMSD_PLUGIN_SAMPLER,
			.term = linux_proc_sampler_term,
			.config = linux_proc_sampler_config,
			.usage = linux_proc_sampler_usage,
		},
		.sample = linux_proc_sampler_sample,
	},
	.log = ldmsd_log
};

struct ldmsd_plugin *get_plugin(ldmsd_msg_log_f pf)
{
	__inst.log = pf;
	__inst.argv_sep = NULL;
	return &__inst.samp.base;
}

int set_rbn_cmp(void *tree_key, const void *key)
{
	const struct set_key *tk = tree_key;
	const struct set_key *k = key;
	if (tk->start_tick != k->start_tick)
		return (int64_t)tk->start_tick - (int64_t)k->start_tick;
	return tk->os_pid - k->os_pid;
}

__attribute__((constructor))
static
void __init__()
{
	/* initialize metric_info_idx_by_name */
	int i;
	for (i = 0; i <= _APP_LAST; i++) {
		metric_info_idx_by_name[i] = &metric_info[i];
	}
	qsort(metric_info_idx_by_name, _APP_LAST + 1,
		sizeof(metric_info_idx_by_name[0]), idx_cmp_by_name);
	qsort(status_line_tbl, ARRAY_LEN(status_line_tbl),
			sizeof(status_line_tbl[0]), status_line_cmp);
	rbt_init(&__inst.set_rbt, set_rbn_cmp);
}

__attribute__((destructor))
static
void __destroy__()
{
	if (help_all != _help)
		free(help_all);
	help_all = NULL;
}
