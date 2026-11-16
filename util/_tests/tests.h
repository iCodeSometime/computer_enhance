//
// Created by Kenneth Cochran on 10/29/26.
//

#ifndef TESTS_H
#define TESTS_H

#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>

#include "util/preamble.h"

static u64 g_failed = 0;
static u64 g_passed = 0;

static void expect(int cond, const char* file, int line, const char* expr) {
    if (cond) {
        g_passed++;
        return;
    }
    g_failed++;
    printf("FAIL %s:%d: %s\n", file, line, expr);
}

static void expect_with_msg(int cond, const char* file, int line, const char* expr, const char* msg) {
    if (cond) {
        g_passed++;
        return;
    }
    g_failed++;
    printf("%s\nFAIL %s:%d: %s\n", msg, file, line, expr);
}

#define EXPECT(cond) expect(!!(cond), __FILE__, __LINE__, #cond)
#define EXPECT_WITH_MSG(cond, msg) expect_with_msg(!!(cond), __FILE__, __LINE__, #cond, msg)

static sigjmp_buf g_fault_jmp;
static volatile sig_atomic_t g_in_test = 0;

static void on_fault(int sig) {
    if (g_in_test) {
        siglongjmp(g_fault_jmp, sig);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

static void run_test(void (*fn)(void), const char* name) {
    printf("-- %s\n", name);
    g_in_test = 1;
    const int fault = sigsetjmp(g_fault_jmp, 1);
    if (fault == 0) {
        fn();
    } else {
        g_failed++;
        fprintf(stderr, "FAIL %s: signal %d\n", name, fault);
    }
    g_in_test = 0;
}
static bool has_prepared = false;
static void prepare_tests(char* suite_name) {
    printf("\n***** %s *****\n", suite_name);
    g_passed = 0;
    g_failed = 0;

    if (!has_prepared) {
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = on_fault;
        sigemptyset(&action.sa_mask);
        sigaction(SIGSEGV, &action, NULL);
        sigaction(SIGBUS, &action, NULL);
        has_prepared = true;
    }
}

#endif //TESTS_H
