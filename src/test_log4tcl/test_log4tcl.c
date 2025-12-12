

#include <common/cstrbuf.h>
#include <common/pathlib.h>
#include <common/emerglog.h>
#include <common/timeapi.h>

#include <clogger/clogger_helper.h>

#ifndef PLATFORM_WINDOWS
    #include <sys/wait.h>
#endif

#include <tcl.h>

static void appexit_cleanup(void)
{
    printf("[test_log4tcl] logger_manager_uninit\n");
    logger_manager_uninit();
}

#define NUM_CHILDS  10

///////////////////////////////////////////////////////////////////////
// 主进程创建多个子进程，每个子进程加载 debug.tcl 脚本
// 主进程和子进程都使用 libclogger 写日志。debug.tcl 脚本使用 log4tcl 写日志。
//
// 杀死全部 test_log4tcl 进程:
//  $ ps -ef | grep [t]est_log4tcl | awk '{print $2}' | xargs -n1 kill -9

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("[main::test_log4tcl] no tcl script file\n");
        return 1;
    }

    cstrbuf debug_tcl = cstrbufNew2(argv[1], -1);

    printf("[main::test_log4tcl] debug_tcl=%.*s\n", CBSTRLEN(debug_tcl), CBSTR(debug_tcl));

    clog_errcode_t err;

    err = logger_manager_init(0, 0);
    if (err) {
        printf("[main::test_log4tcl] logger_manager_init error=%d\n", err);
        return 1;
    }

    clog_logger logger = logger_manager_load("test_main");
    assert(logger);

    atexit(appexit_cleanup);

#ifndef PLATFORM_WINDOWS
    CLOG_INFO(logger, "fork child procs...");

    // 创建多个子进程
    for (int i = 0; i < NUM_CHILDS; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            // fork 失败
            CLOG_ERROR(logger, "fork failed: %d", i);
            continue;
        } else if (pid == 0) {
            // 子进程代码
            logger_manager_uninit();

            printf("[child::test_log4tcl] logger_manager_init\n");

            logger_manager_init(0, 0);

            clog_logger logger = logger_manager_load("test_child");
            assert(logger);

            CLOG_INFO(logger, "Child process %d started (PID: %d)", i, getpid());

            // 在 Tcl 中，Tcl_Interp *interp 不是线程安全的。以下是关键原因和注意事项：
            // 每个 Tcl_Interp 对象必须严格绑定到创建它的线程。
            // 解释器内部状态（如变量、命令、执行栈）不能跨线程共享。
            // 若在非创建线程中访问 interp，会导致未定义行为（崩溃、数据损坏）
            Tcl_Interp *interp = Tcl_CreateInterp();

            // 初始化 Tcl 核心
            if (Tcl_Init(interp) != TCL_OK) {
                CLOG_ERROR(logger, "Tcl_Init failed: %s\n", Tcl_GetStringResult(interp));
                logger_manager_uninit();
                exit(1);
            }

            CLOG_INFO(logger, "Child process %d loading: %.*s", i, CBSTRLEN(debug_tcl), CBSTR(debug_tcl));

            if (Tcl_EvalFile(interp, CBSTR(debug_tcl)) != TCL_OK) {
                CLOG_ERROR(logger, "Tcl_EvalFile failed: %s\n", Tcl_GetStringResult(interp));
                logger_manager_uninit();
                exit(1);
            }

            int a = 0;
            while (a++ < 100) {
                // 调用Tcl函数命令并获取返回值
                CLOG_DEBUG(logger, "Tcl_Eval: main_proc");

                if (Tcl_Eval(interp, "main_proc {hello tcl}") != TCL_OK) {
                    const char* result = Tcl_GetStringResult(interp);
                    CLOG_ERROR(logger, "Tcl_Eval failed: %s", result);
                    continue;
                }

                sleep_msec(20);
            }

            Tcl_DeleteInterp(interp);

            logger_manager_uninit();
            exit(0); // 确保子进程退出
        } else {
            // 父进程代码
            CLOG_INFO(logger, "Forked child %d with PID: %d", i, pid);
        }
    }
#else
    // Windows 环境下不使用 fork
    CLOG_INFO(logger, "Windows environment: not forking child processes");

    // 在主进程中执行 Tcl 脚本
    Tcl_Interp *interp = Tcl_CreateInterp();

    // 初始化 Tcl 核心
    if (Tcl_Init(interp) != TCL_OK) {
        CLOG_ERROR(logger, "Tcl_Init failed: %s\n", Tcl_GetStringResult(interp));
        return 1;
    }

    CLOG_INFO(logger, "Loading: %.*s", CBSTRLEN(debug_tcl), CBSTR(debug_tcl));

    if (Tcl_EvalFile(interp, CBSTR(debug_tcl)) != TCL_OK) {
        CLOG_ERROR(logger, "Tcl_EvalFile failed: %s\n", Tcl_GetStringResult(interp));
        return 1;
    }

    int a = 0;
    while (a++ < 100) {
        // 调用Tcl函数命令并获取返回值
        CLOG_DEBUG(logger, "Tcl_Eval: main_proc");

        if (Tcl_Eval(interp, "main_proc {hello tcl}") != TCL_OK) {
            const char* result = Tcl_GetStringResult(interp);
            CLOG_ERROR(logger, "Tcl_Eval failed: %s", result);
            continue;
        }

        sleep_msec(20);
    }

    Tcl_DeleteInterp(interp);
#endif

#ifndef PLATFORM_WINDOWS
    // 等待所有子进程结束
    int status;
    pid_t child_pid;
    while ((child_pid = wait(&status)) > 0) {
        if (WIFEXITED(status)) {
            CLOG_INFO(logger, "Child process %d exited with status %d", child_pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            CLOG_INFO(logger, "Child process %d killed by signal %d", child_pid, WTERMSIG(status));
        }
    }
#else
    // Windows 环境下不等待子进程
    CLOG_INFO(logger, "Windows environment: not waiting for child processes");
#endif

    CLOG_WARN(logger, "main proc exit 0");
    return 0;
}
