/**
 * @file test_clogger.c
 * @author cheungmine@qq.com
 * @copyright © 2024-2025 anomura.cc All Rights Reserved.
 * @version 0.0.35
 * @since 2025-08-24 21:43:56
 * @date 2025-11-29 18:27:57
 * @brief 临时测试程序
 */
#include <common/mscrtdbg.h>
#include <common/cstrbuf.h>
#include <common/pathlib.h>
#include <common/emerglog.h>
#include <common/timeapi.h>

// pthread or pthread-w32
#include <sched.h>
#include <pthread.h>

#if defined(PLATFORM_WINDOWS)
    # include <common/win32/getoptw.h>
#else
    # include <getopt.h>
#endif

#include <clogger/clogger_helper.h>

#define APPVER    "0.9.1"
#define APPNAME   "test_clogger"

cstrbuf config = 0;
cstrbuf ident = 0;

int threads = 10;
int messages = 10000;
int microsecond = 0;


typedef struct
{
    int threadno;
    char bbuf[256];
} threadinfo_t;


static void appexit_cleanup(void)
{
    logger_manager_uninit();

    cstrbufFree(&config);
    cstrbufFree(&ident);

    emerg_syslog_message(0, 0, "libclogger", "test_clogger exit(0)", -1);
}


static void* test_clogger_thread(void* arg)
{
    threadinfo_t* thrinfo = (threadinfo_t*)arg;

    clog_logger logger = logger_manager_load("test_clogger");

    int tid = thrinfo->threadno;

    int count = 0;

    time_t t1, t0 = time(0);

    int blen = snprintf(thrinfo->bbuf, sizeof(thrinfo->bbuf), "[thread-%d] starting(messages=%d)...",
                    tid, messages);

    emerg_syslog_message(0, 0, ident->str, thrinfo->bbuf, blen);

    while (count < messages) {
        count++;

        CLOG_TRACE(logger, "[%d:%lld] clogger is a high-performance, reliable, threads safety, easy to use, pure C logging library.", tid, count);
        CLOG_DEBUG(logger, "[%d:%lld] As far as I know in the C world there was NO perfect logging facility for applications like logback in java or log4cxx in c++.", tid, count);
        CLOG_INFO(logger, "[%d:%lld] Using printf can work, but can not be redirected or reformatted easily.", tid, count);
        CLOG_WARN(logger, "[%d:%lld] syslog is slow and is designed for system use.", tid, count);
        CLOG_ERROR(logger, "[%d:%lld] Others like LOG4C(has BUGs) or ZLOG(over-design) is somewhat of complication.", tid, count);
        CLOG_FATAL(logger, "[%d:%lld] So I wrote CLOGGER from the bottom up!", tid, count);

        sleep_usec(microsecond);
    }

    t1 = time(0);

    blen = snprintf(thrinfo->bbuf, sizeof(thrinfo->bbuf), "[thread-%d] stopped (messages=%d logged). elapsed seconds=%d speed=%d/s",
        tid, count, (int)(t1 - t0), (int)(count / (t1 - t0 + 0.1)));

    emerg_syslog_message(0, 0, ident->str, thrinfo->bbuf, blen);

    free(thrinfo);
    return (void*)0;
}


void print_usage(void)
{
#if defined(PLATFORM_WINDOWS) || defined(PLATFORM_CYGWIN)
    fprintf(stdout, "Usage: %s.exe [Options...] \n", APPNAME);
#else
    fprintf(stdout, "Usage: %s [Options...] \n", APPNAME);
#endif

    fprintf(stdout, "  %s is a clog test tool.\n", APPNAME);

    fprintf(stdout, "Options:\n");
    fprintf(stdout, "  -h, --help                  display help information.\n");
    fprintf(stdout, "  -V, --version               show %s version.\n\n", APPNAME);
    fprintf(stdout, "\n");
    fprintf(stdout, "  -I, --ident=NAME            name for clogger identifer. ('%s' default)\n", APPNAME);
    fprintf(stdout, "  -C, --config=<CFGFILE>      initialize with config (CFGFILE or default).\n");
    fprintf(stdout, "  -t, --threads=NUM           number of threads. ('%d' default)\n", threads);
    fprintf(stdout, "  -n, --messages=NUM          number of messages. ('%d' default)\n", messages);
    fprintf(stdout, "  -u, --microsecond=USEC      sleep for microsecond. ('%d' default)\n", microsecond);
    fprintf(stdout, "  -D, --daemon                runs in background. (not default)\n");

    fflush(stdout);
}

#ifndef PLATFORM_WINDOWS
// SIGUSR1信号处理函数
static void sigusr1_handler(int signum) {
    if (signum == SIGUSR1) {
        printf("接收到信号时刷新日志级别\n");
        // 接收到信号时刷新日志级别
        logger_manager_update_loglevel();
    }
}

// 注册信号处理函数
static void setup_signal_handler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigusr1_handler; // 绑定处理函数
    sa.sa_flags = 0; // 不使用特殊标志（确保信号安全）

    // 屏蔽其他信号，避免处理期间被打断
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGUSR1);

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("注册SIGUSR1处理函数失败");
        exit(EXIT_FAILURE);
    }
}
#else
// Windows 环境下不使用信号处理
static void setup_signal_handler() {
    // 空实现
}
#endif


////////////////////////////////////////////////////////////////////////////
// $ cd build/linux-x86_64/libs/
// $ valgrind --leak-check=full --show-leak-kinds=all ./test_clogger -t 1 -n 100
//
int main(int argc, const char* argv[])
{
    MSCRTDBG_INIT();

    int opt, optindex, background = 0;

    ident = cstrbufNew(0, APPNAME, -1);

    const struct option lopts[] = {
        {"help",           no_argument, 0, 'h'},
        {"version",        no_argument, 0, 'V'},
        {"ident",          required_argument, 0, 'I'},
        {"config",         optional_argument, 0, 'C'},
        {"threads",        required_argument, 0, 't'},
        {"messages",       required_argument, 0, 'n'},
        {"microsecond",    required_argument, 0, 'u'},
#ifndef PLATFORM_WINDOWS
        {"daemon",         no_argument,       0, 'D'},
#endif
        {0, 0, 0, 0}
    };

    printf("[%s:%d] 程序启动...\n", APPNAME, getprocessid());

    // read option args
    while ((opt = getopt_long_only(argc, (char* const*)argv,
#if !defined(PLATFORM_WINDOWS) && !defined(PLATFORM_CYGWIN)
        "hVDC::I:t:n:u:",
#else
        "hVC::I:t:n:u:",
#endif
        lopts, &optindex)) != -1) {
        switch (opt) {
        case '?':
            printf("error: specified option not found.\n");
            exit(EXIT_FAILURE);

        case 'h':
            print_usage();
            exit(0);
            break;
#if !defined(PLATFORM_WINDOWS) && !defined(PLATFORM_CYGWIN)
        case 'D':
            background = 1;
            break;
#endif
        case 'V':
#ifdef NDEBUG
            fprintf(stdout, "%s-%s, Build Release: %s %s\n\n", APPNAME, APPVER, __DATE__, __TIME__);
#else
            fprintf(stdout, "%s-%s, Build Debug: %s %s\n\n", APPNAME, APPVER, __DATE__, __TIME__);
#endif
            exit(0);
            break;

        case 'I':     // ident
            ident = cstrbufDup(ident, optarg, -1);
            break;

        case 'C':     // config
            if (optarg) {
                config = cstrbufNew(128, optarg, -1);
            }
            break;

        case 't': // threads
            threads = atoi(optarg);
            break;

        case 'n': // messages
            messages = atoi(optarg);
            break;

        case 'u': // microsecond
            microsecond = atoi(optarg);
            break;
        }
    }

    if (background) {
#if !defined(PLATFORM_WINDOWS) && !defined(PLATFORM_CYGWIN)
        // runs in background
        if (daemon(0, 0)) {
            emerglog_check_syscall(-1, ident->str, "daemon failed");
        }
        emerg_syslog_message(0, 0, ident->str, "runs as daemon", -1);
#endif
    }

    /* initialize once for logger manager */
    if (config) {
        logger_manager_init(config->str, 0);
    }
    else {
        logger_manager_init(0, 0);
    }

    printf("[%s] liblogger version: %s\n", APPNAME, libclogger_version(0, 0, 0));

    // 打印进程ID
    printf("程序已启动，PID: %d\n", getprocessid());

#ifndef PLATFORM_WINDOWS
    printf("发送信号刷新日志级别: kill -USR1 %d\n", getpid());
#endif

    setup_signal_handler();

    // register on exit cleanup
    atexit(appexit_cleanup);

    // load other idents as you need!
    pthread_t tids[128] = { 0 };

    for (int i = 0; i < threads; i++) {
        threadinfo_t* thrinfo = (threadinfo_t*) malloc(sizeof(*thrinfo));

        thrinfo->threadno = i + 1;

        int err = pthread_create(&tids[i], NULL, test_clogger_thread, (void*)thrinfo);
        emerglog_check_syscall(err, ident->str, "pthread_create");
    }

    for (int i = 0; i < threads; i++) {
        int err = pthread_join(tids[i], NULL);
        emerglog_check_syscall(err, ident->str, "pthread_join");
    }

    emerg_syslog_message(0, 0, ident->str, "exit(0)", -1);
    return 0;
}
