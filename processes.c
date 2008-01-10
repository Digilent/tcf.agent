/*******************************************************************************
 * Copyright (c) 2007 Wind River Systems, Inc. and others.
 * All rights reserved. This program and the accompanying materials 
 * are made available under the terms of the Eclipse Public License v1.0 
 * which accompanies this distribution, and is available at 
 * http://www.eclipse.org/legal/epl-v10.html 
 *  
 * Contributors:
 *     Wind River Systems - initial API and implementation
 *******************************************************************************/

/*
 * TCF Processes - process control service.
 * Processes service provides access to the target OS's process information,
 * allows to start and terminate a process, and allows to attach and
 * detach a process for debugging. Debug services, like Memory and Run Control,
 * require a process to be attached before they can access it.
 */

#include "config.h"
#if SERVICE_Processes

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include "mdep.h"
#include "myalloc.h"
#include "protocol.h"
#include "context.h"
#include "json.h"
#include "exceptions.h"

static const char * PROCESSES = "Processes";

#if defined(WIN32)
#  include <direct.h>
#elif defined(_WRS_KERNEL)
#  include <symLib.h>
#  include <sysSymTbl.h>
#else
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
#  include <dirent.h>
#endif

static void write_context(OutputStream * out, char * id, char * dir) {
    Context * ctx = NULL;

    out->write(out, '{');

#if defined(WIN32)
#elif defined(_WRS_KERNEL)
#else
    if (chdir(dir) >= 0) {
        int sz;
        char fnm[FILE_PATH_SIZE + 1];

        json_write_string(out, "CanTerminate");
        out->write(out, ':');
        json_write_boolean(out, 1);
        out->write(out, ',');

        if ((sz = readlink("exe", fnm, FILE_PATH_SIZE)) > 0) {
            fnm[sz] = 0;
            json_write_string(out, "Name");
            out->write(out, ':');
            json_write_string(out, fnm);
            out->write(out, ',');
        }
    }
#endif
    
    ctx = id2ctx(id);
    if (ctx != NULL) {
        json_write_string(out, "Attached");
        out->write(out, ':');
        json_write_boolean(out, 1);
        out->write(out, ',');
    }

    json_write_string(out, "ID");
    out->write(out, ':');
    json_write_string(out, id);

    out->write(out, '}');
}

static void command_get_context(char * token, InputStream * inp, OutputStream * out) {
    int err = 0;
    char id[256];
    pid_t pid, parent;
    char dir[FILE_PATH_SIZE];

    json_read_string(inp, id, sizeof(id));
    if (inp->read(inp) != 0) exception(ERR_JSON_SYNTAX);
    if (inp->read(inp) != MARKER_EOM) exception(ERR_JSON_SYNTAX);

    pid = id2pid(id, &parent);
    write_stringz(out, "R");
    write_stringz(out, token);

    pid = id2pid(id, &parent);
    snprintf(dir, sizeof(dir), "/proc/%d", pid);
    if (pid != 0 && parent == 0) {
#if defined(WIN32)
#elif defined(_WRS_KERNEL)
        if (TASK_ID_VERIFY(pid) == ERROR) err = ERR_INV_CONTEXT;
#else
        struct_stat st;
        if (lstat(dir, &st) < 0) err = errno;
        else if (!S_ISDIR(st.st_mode)) err = ERR_INV_CONTEXT;
#endif
    }

    write_errno(out, err);
    
    if (err == 0 && pid != 0 && parent == 0) {
        write_context(out, id, dir);
        out->write(out, 0);
    }
    else {
        write_stringz(out, "null");
    }

    out->write(out, MARKER_EOM);
}

static void command_get_children(char * token, InputStream * inp, OutputStream * out) {
    char id[256];
    pid_t parent = 0;
    int attached_only;

    json_read_string(inp, id, sizeof(id));
    if (inp->read(inp) != 0) exception(ERR_JSON_SYNTAX);
    attached_only = json_read_boolean(inp);
    if (inp->read(inp) != 0) exception(ERR_JSON_SYNTAX);
    if (inp->read(inp) != MARKER_EOM) exception(ERR_JSON_SYNTAX);

    write_stringz(out, "R");
    write_stringz(out, token);

    if (id[0] != 0) {
        write_errno(out, 0);
        write_stringz(out, "null");
    }
    else {
#if defined(WIN32)
#elif defined(_WRS_KERNEL)
        int i = 0;
        int cnt = 0;
        int ids_cnt = 0;
        int ids_max = 500;
        int * ids = (int *)loc_alloc(ids_max * sizeof(int));
        while (1) {
            ids_cnt = taskIdListGet(ids, ids_max);
            if (ids_cnt < ids_max) break;
            loc_free(ids);
            ids_max *= 2;
            ids = (int *)loc_alloc(ids_max * sizeof(int));
        }
        out->write(out, '[');
        for (i = 0; i < ids_cnt; i++) {
            if (!attached_only || context_find_from_pid(ids[i]) != NULL) {
                if (cnt > 0) out->write(out, ',');
                json_write_string(out, pid2id(ids[i], 0));
                cnt++;
            }
        }
        out->write(out, ']');
        out->write(out, 0);
#else
        DIR * proc = opendir("/proc");
        if (proc == NULL) {
            write_errno(out, errno);
            write_stringz(out, "null");
        }
        else {
            int cnt = 0;
            write_errno(out, 0);
            out->write(out, '[');
            for (;;) {
                struct dirent * ent = readdir(proc);
                if (ent == NULL) break;
                if (ent->d_name[0] >= '1' && ent->d_name[0] <= '9') {
                    pid_t pid = atol(ent->d_name);
                    if (!attached_only || context_find_from_pid(pid) != NULL) {
                        if (cnt > 0) out->write(out, ',');
                        json_write_string(out, pid2id(pid, 0));
                        cnt++;
                    }
                }
            }
            out->write(out, ']');
            out->write(out, 0);
            closedir(proc);
        }
#endif
    }

    out->write(out, MARKER_EOM);
}

static void command_attach(char * token, InputStream * inp, OutputStream * out) {
    int err = 0;
    char id[256];
    pid_t pid, parent;

    json_read_string(inp, id, sizeof(id));
    if (inp->read(inp) != 0) exception(ERR_JSON_SYNTAX);
    if (inp->read(inp) != MARKER_EOM) exception(ERR_JSON_SYNTAX);

    pid = id2pid(id, &parent);
    write_stringz(out, "R");
    write_stringz(out, token);

    if (parent != 0) {
        err = ERR_INV_CONTEXT;
    }
    else if (context_find_from_pid(pid) != NULL) {
        err = ERR_ALREADY_ATTACHED;
    }
    else {
        if (context_attach(pid, NULL) < 0) err = errno;
    }

    write_errno(out, err);
    out->write(out, MARKER_EOM);
}

static void command_detach(char * token, InputStream * inp, OutputStream * out) {
    // TODO: implement command_detach()
    exception(ERR_PROTOCOL);
}

static void command_terminate(char * token, InputStream * inp, OutputStream * out) {
    int err = 0;
    char id[256];
    pid_t pid, parent;

    json_read_string(inp, id, sizeof(id));
    if (inp->read(inp) != 0) exception(ERR_JSON_SYNTAX);
    if (inp->read(inp) != MARKER_EOM) exception(ERR_JSON_SYNTAX);

    pid = id2pid(id, &parent);
    write_stringz(out, "R");
    write_stringz(out, token);

    if (parent != 0) {
        err = ERR_INV_CONTEXT;
    }
    else {
#if defined(WIN32)
        err = ENOSYS;
#elif defined(_WRS_KERNEL)
        if (kill(pid, SIGTERM) < 0) err = errno;
#else
        if (kill(pid, SIGTERM) < 0) err = errno;
#endif
    }

    write_errno(out, err);
    out->write(out, MARKER_EOM);
}

static void command_signal(char * token, InputStream * inp, OutputStream * out) {
    int err = 0;
    char id[256];
    int signal = 0;
    pid_t pid, parent;

    json_read_string(inp, id, sizeof(id));
    if (inp->read(inp) != 0) exception(ERR_JSON_SYNTAX);
    signal = (int)json_read_long(inp);
    if (inp->read(inp) != 0) exception(ERR_JSON_SYNTAX);
    if (inp->read(inp) != MARKER_EOM) exception(ERR_JSON_SYNTAX);

    pid = id2pid(id, &parent);
    write_stringz(out, "R");
    write_stringz(out, token);

    if (parent != 0) {
        err = ERR_INV_CONTEXT;
    }
    else {
#if defined(WIN32)
        err = ENOSYS;
#elif defined(_WRS_KERNEL)
        if (kill(pid, signal) < 0) err = errno;
#else
        if (kill(pid, signal) < 0) err = errno;
#endif
    }

    write_errno(out, err);
    out->write(out, MARKER_EOM);
}

static void command_get_environment(char * token, InputStream * inp, OutputStream * out) {
    char ** p = environ;

    if (inp->read(inp) != MARKER_EOM) exception(ERR_JSON_SYNTAX);

    write_stringz(out, "R");
    write_stringz(out, token);
    write_errno(out, 0);
    out->write(out, '[');
    if (p != NULL) {
        while (*p != NULL) {
            if (p != environ) out->write(out, ',');
            json_write_string(out, *p++);
        }
    }
    out->write(out, ']');
    out->write(out, 0);
    out->write(out, MARKER_EOM);
}

static void command_start(char * token, InputStream * inp, OutputStream * out) {
    Context * ctx = NULL;
    int pid = 0;
    int err = 0;
    char dir[FILE_PATH_SIZE];
    char exe[FILE_PATH_SIZE];
    char ** args = NULL;
    char ** envp = NULL;
    int args_len = 0;
    int envp_len = 0;
    int attach = 0;
    Trap trap;

    if (set_trap(&trap)) {
        json_read_string(inp, dir, sizeof(dir));
        if (inp->read(inp) != 0) exception(ERR_JSON_SYNTAX);
        json_read_string(inp, exe, sizeof(exe));
        if (inp->read(inp) != 0) exception(ERR_JSON_SYNTAX);
        args = json_read_alloc_string_array(inp, &args_len);
        if (inp->read(inp) != 0) exception(ERR_JSON_SYNTAX);
        envp = json_read_alloc_string_array(inp, &envp_len);
        if (inp->read(inp) != 0) exception(ERR_JSON_SYNTAX);
        attach = json_read_boolean(inp);
        if (inp->read(inp) != 0) exception(ERR_JSON_SYNTAX);
        if (inp->read(inp) != MARKER_EOM) exception(ERR_JSON_SYNTAX);

        if (dir[0] != 0 && chdir(dir) < 0) err = errno;
        if (err == 0) {
#if defined(WIN32)
#elif defined(_WRS_KERNEL)
            char * ptr;
            SYM_TYPE type;
            if (symFindByName(sysSymTbl, exe, &ptr, &type) != OK) {
                err = errno;
                if (err == S_symLib_SYMBOL_NOT_FOUND) err = ERR_SYM_NOT_FOUND;
                assert(err != 0);
            }
            else {
                // TODO: arguments, environment
                pid = taskCreate("tTcf", 100, 0, 0x4000, (FUNCPTR)ptr, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                if (attach) {
                    taskStop(pid);
                    taskActivate(pid);
                    assert(taskIsStopped(pid));
                }
                else {
                    taskActivate(pid);
                }
            }
#else
            pid = fork();
            if (pid < 0) err = errno;
            if (pid == 0) {
                if (attach) tkill(getpid(), SIGSTOP);
                exit(execve(exe, args, envp));
            }
#endif            
        }
        if (attach) {
            if (err == 0 && context_attach(pid, &ctx) < 0) err = errno;
            if (ctx != NULL && !ctx->stopped) ctx->pending_intercept = 1;
        }

        write_stringz(out, "R");
        write_stringz(out, token);
        write_errno(out, err);
        if (err || pid == 0) {
            write_stringz(out, "null");
        }
        else {
            char bf[256];
            snprintf(dir, sizeof(dir), "/proc/%d", pid);
            write_context(out, strcpy(bf, pid2id(pid, 0)), dir);
            out->write(out, 0);
        }
        out->write(out, MARKER_EOM);
        clear_trap(&trap);
    }

    loc_free(args);
    loc_free(envp);

    if (trap.error) exception(trap.error);
}

void ini_processes_service(void) {
    add_command_handler(PROCESSES, "getContext", command_get_context);
    add_command_handler(PROCESSES, "getChildren", command_get_children);
    add_command_handler(PROCESSES, "attach", command_attach);
    add_command_handler(PROCESSES, "detach", command_detach);
    add_command_handler(PROCESSES, "terminate", command_terminate);
    add_command_handler(PROCESSES, "signal", command_signal);
    add_command_handler(PROCESSES, "getEnvironment", command_get_environment);
    add_command_handler(PROCESSES, "start", command_start);
}

#endif

