//
// Created by Kenneth Cochran on 10/29/26.
//

#ifndef TESTS_H
#define TESTS_H

#include <stdio.h>

#include "util/preamble.h"

static u64 g_failed = 0;
static u64 g_passed = 0;

static void expect(int cond, const char* file, int line, const char* expr) {
    if (cond) {
        g_passed++;
        return;
    }
    g_failed++;
    fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expr);
}

#define EXPECT(cond) expect(!!(cond), __FILE__, __LINE__, #cond)

#endif //TESTS_H
