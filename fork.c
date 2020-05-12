#ifdef __linux__
#define _GNU_SOURCE
#else
#define _POSIX_C_SOURCE 200809L
#endif

#include <fcntl.h>
#include <unistd.h>
#include <spawn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <janet.h>

typedef struct {
    pid_t pid;
    int close_signal;
    int exited;
    int wstatus;
} Process;

/*
   Get a process exit code, the process must have had process_wait called.
   Returns -1 and sets errno on error, otherwise returns the exit code.
*/
static int process_exit_code(Process *p) {
    if (!p->exited || p->pid == -1) {
        errno = EINVAL;
        return -1;
    }

    int exit_code = 0;

    if (WIFEXITED(p->wstatus)) {
        exit_code = WEXITSTATUS(p->wstatus);
    } else if (WIFSIGNALED(p->wstatus)) {
        // Should this be a function of the signal?
        exit_code = 129;
    } else {
        /* This should be unreachable afaik */
        errno = EINVAL;
        return -1;
    }

    return exit_code;
}

/*
   Returns -1 and sets errno on error, otherwise returns the process exit code.
*/

static int process_wait(Process *p, int *exit, int flags) {
    int _exit = 0;
    if (!exit)
        exit = &_exit;

    if (p->pid == -1) {
        errno = EINVAL;
        return -1;
    }

    if (p->exited) {
        *exit = process_exit_code(p);
        return 0;
    }

    int err;

    do {
        err = waitpid(p->pid, &p->wstatus, flags);
    } while (err < 0 && errno == EINTR);

    if (err < 0)
        return -1;

    if ((flags & WNOHANG && err == 0)) {
        *exit = -1;
        return 0;
    }

    p->exited = 1;
    *exit = process_exit_code(p);
    return 0;
}

static int process_signal(Process *p, int sig) {
    int err;

    if (p->exited || p->pid == -1)
        return 0;

    do {
        err = kill(p->pid, sig);
    } while (err < 0 && errno == EINTR);

    if (err < 0)
        return -1;

    return 0;
}

static int process_gc(void *ptr, size_t s) {
    (void)s;
    int err;

    Process *p = (Process *)ptr;
    if (!p->exited && p->pid != -1) {
        do {
            err = kill(p->pid, p->close_signal);
        } while (err < 0 && errno == EINTR);
        if (process_wait(p, NULL, 0) < 0) {
            /* Not much we can do here. */
            p->exited = 1;
        }
    }
    return 0;
}

static Janet fork_close(int32_t argc, Janet *argv);
static Janet fork_wait(int32_t argc, Janet *argv);
static Janet fork_signal(int32_t argc, Janet *argv);

static JanetMethod process_methods[] = {
    {"close", fork_close}, /* So processes can be used with 'with' */
    {"wait",  fork_wait},
    {"signal", fork_signal},
    {NULL, NULL}
};

static int process_get(void *ptr, Janet key, Janet *out) {
    Process *p = (Process *)ptr;

    if (!janet_checktype(key, JANET_KEYWORD))
        return 0;

    if (janet_keyeq(key, "pid")) {
        *out = (p->pid == -1) ? janet_wrap_nil() : janet_wrap_integer(p->pid);
        return 1;
    }

    if (janet_keyeq(key, "exit-code")) {
        int exit_code;

        if (process_wait(p, &exit_code, WNOHANG) != 0)
            janet_panicf("error checking exit status: %s", strerror(errno));

        *out = (exit_code == -1) ? janet_wrap_nil() : janet_wrap_integer(exit_code);
        return 1;
    }

    return janet_getmethod(janet_unwrap_keyword(key), process_methods, out);
}

static const JanetAbstractType process_type = {
    "fork/process", process_gc, NULL, process_get, JANET_ATEND_GET
};

static Janet fork_wait(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 1);
    Process *p = (Process *)janet_getabstract(argv, 0, &process_type);

    int exit_code;

    if (process_wait(p, &exit_code, 0) != 0)
        janet_panicf("error waiting for process - %s", strerror(errno));

    return janet_wrap_integer(exit_code);
}

static Janet fork_signal(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 2);
    Process *p = (Process *)janet_getabstract(argv, 0, &process_type);
    int sig = janet_getinteger(argv, 1);
    if (sig == -1)
        janet_panic("invalid signal");

    int rc = process_signal(p, sig);
    if (rc < 0)
        janet_panicf("unable to signal process - %s", strerror(errno));

    return janet_wrap_nil();
}

static Janet fork_close(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 1);
    Process *p = (Process *)janet_getabstract(argv, 0, &process_type);

    if (p->exited)
        return janet_wrap_nil();

    int rc;

    rc = process_signal(p, p->close_signal);
    if (rc < 0)
        janet_panicf("unable to signal process - %s", strerror(errno));

    rc = process_wait(p, NULL, 0);
    if (rc < 0)
        janet_panicf("unable to wait for process - %s", strerror(errno));

    return janet_wrap_nil();
}

static Janet jfork(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 1);

    pid_t child = fork();
    if (child == -1)
        janet_panicf("error fork failed - %s", strerror(errno));

    if (child == 0)
        return janet_wrap_nil();


    Process *p = (Process *)janet_abstract(&process_type, sizeof(Process));
    p->close_signal = SIGTERM;
    p->pid = child;
    p->exited = 0;
    p->wstatus = 0;

    if (!janet_checktype(argv[0], JANET_NIL)) {
        int close_signal_int = janet_getnumber(argv, 0);
        if (close_signal_int == -1)
            janet_panic("invalid value for :close-signal");
        p->close_signal = close_signal_int;
    }

    return janet_wrap_abstract(p);
}

static const JanetReg cfuns[] = {
    {"fork", jfork, "(fork/fork)\n\n"},
    {"signal", fork_signal, "(fork/signal p sig)\n\n"},
    {"close", fork_close, "(fork/close p)\n\n"},
    {"wait", fork_wait, "(fork/wait p)\n\n"},
    {NULL, NULL, NULL}
};

JANET_MODULE_ENTRY(JanetTable *env) {
    janet_cfuns(env, "fork", cfuns);
}
