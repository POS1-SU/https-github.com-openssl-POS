/*
 * Copyright 2014-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "testutil.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "e_os.h"

#include <openssl/opensslconf.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

/*
 * Declares the structures needed to register each test case function.
 */
typedef struct test_info {
    const char *test_case_name;
    int (*test_fn) ();
    int (*param_test_fn)(int idx);
    int num;
} TEST_INFO;

static TEST_INFO all_tests[1024];
static int num_tests = 0;
/*
 * A parameterised tests runs a loop of test cases.
 * |num_test_cases| counts the total number of test cases
 * across all tests.
 */
static int num_test_cases = 0;

void add_test(const char *test_case_name, int (*test_fn) ())
{
    assert(num_tests != OSSL_NELEM(all_tests));
    all_tests[num_tests].test_case_name = test_case_name;
    all_tests[num_tests].test_fn = test_fn;
    all_tests[num_tests].num = -1;
    ++num_test_cases;
    ++num_tests;
}

void add_all_tests(const char *test_case_name, int(*test_fn)(int idx),
                   int num)
{
    assert(num_tests != OSSL_NELEM(all_tests));
    all_tests[num_tests].test_case_name = test_case_name;
    all_tests[num_tests].param_test_fn = test_fn;
    all_tests[num_tests].num = num;
    ++num_tests;
    num_test_cases += num;
}

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
static int should_report_leaks()
{
    /*
     * When compiled with enable-crypto-mdebug, OPENSSL_DEBUG_MEMORY=0
     * can be used to disable leak checking at runtime.
     * Note this only works when running the test binary manually;
     * the test harness always enables OPENSSL_DEBUG_MEMORY.
     */
    char *mem_debug_env = getenv("OPENSSL_DEBUG_MEMORY");
    return mem_debug_env == NULL
        || (strcmp(mem_debug_env, "0") && strcmp(mem_debug_env, ""));
}
#endif


void setup_test()
{
#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (should_report_leaks()) {
        CRYPTO_set_mem_debug(1);
        CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
    }
#endif
}

int finish_test(int ret)
{
#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (should_report_leaks() && CRYPTO_mem_leaks_fp(stderr) <= 0)
        return EXIT_FAILURE;
#endif
    return ret;
}

static void finalize(int success)
{
    if (success)
        ERR_clear_error();
    else
        ERR_print_errors_fp(stderr);
}

int run_tests(const char *test_prog_name)
{
    int num_failed = 0;

    int i, j;

    printf("%s: %d test case%s\n", test_prog_name, num_test_cases,
           num_test_cases == 1 ? "" : "s");

    for (i = 0; i != num_tests; ++i) {
        if (all_tests[i].num == -1) {
            int ret = all_tests[i].test_fn();

            if (!ret) {
                printf("** %s failed **\n--------\n",
                       all_tests[i].test_case_name);
                ++num_failed;
            }
            finalize(ret);
        } else {
            for (j = 0; j < all_tests[i].num; j++) {
                int ret = all_tests[i].param_test_fn(j);

                if (!ret) {
                    printf("** %s failed test %d\n--------\n",
                           all_tests[i].test_case_name, j);
                    ++num_failed;
                }
                finalize(ret);
            }
        }
    }

    if (num_failed != 0) {
        printf("%s: %d test%s failed (out of %d)\n", test_prog_name,
               num_failed, num_failed != 1 ? "s" : "", num_test_cases);
        return EXIT_FAILURE;
    }
    printf("  All tests passed.\n");
    return EXIT_SUCCESS;
}

static const char *print_string_maybe_null(const char *s)
{
    return s == NULL ? "(NULL)" : s;
}

int strings_equal(const char *desc, const char *s1, const char *s2)
{
    if (s1 == NULL && s2 == NULL)
      return 1;
    if (s1 == NULL || s2 == NULL || strcmp(s1, s2) != 0) {
        fprintf(stderr, "%s mismatch: %s vs %s\n", desc, print_string_maybe_null(s1),
                print_string_maybe_null(s2));
        return 0;
    }
    return 1;
}
