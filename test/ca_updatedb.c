/*
 * Copyright 2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifdef OPENSSL_SYS_UNIX
# include <unistd.h>
#endif
#include "../apps/include/ca.h"
#include "../apps/include/ca_logic.h"
#include "../apps/include/apps.h"

/* tweaks needed due to missing unistd.h on Windows */
#ifdef _WIN32
# define access _access
#endif

char *default_config_file = NULL;

time_t iso8601_utc_to_time_t(const char *dateStr)
{
    struct tm t;

    int success = sscanf(dateStr, "%d-%d-%dT%d:%dZ",
        &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min);

    if (success != 5) {
        return 0;
    }

    t.tm_year = t.tm_year - 1900;
    t.tm_mon = t.tm_mon - 1;
    t.tm_sec = 0;
    t.tm_wday = 0;
    t.tm_yday = 0;
    t.tm_isdst = 0;

    time_t localTime = mktime(&t);
    time_t utcTime = localTime - timezone;
    return utcTime;
}

int main(int argc, char *argv[])
{
    CA_DB *db = NULL;
    int r;
    time_t *testdate;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s indexfile testdate\n", argv[0]);
        fprintf(stderr, "       testdate format: YYYY-MM-DDThh:mm\n");
	exit(EXIT_FAILURE);
    }
    if (access(argv[1], F_OK) != 0) {
        fprintf(stderr, "Error: dbfile '%s' is not readable\n", argv[1]);
	exit(EXIT_FAILURE);
    }

    testdate = app_malloc(sizeof(time_t), "testdate");
    *testdate = iso8601_utc_to_time_t(argv[2]);
    if (*testdate == 0) {
        free(testdate);
        fprintf(stderr, "Error: testdate '%s' is invalid\n", argv[2]);
	exit(EXIT_FAILURE);
    }

    default_config_file = CONF_get1_default_config_file();
    if (default_config_file == NULL) {
        free(testdate);
        fprintf(stderr, "Error: could not get default config file\n");
	exit(EXIT_FAILURE);
    }

    db = load_index(argv[1], NULL);

    r = do_updatedb(db, testdate);

    if (r == -1)
	exit(EXIT_FAILURE);

    printf("Marked %i entries as expired\n", r);
    free(default_config_file);
    free_index(db);
    free(testdate);
    exit(EXIT_SUCCESS);
}
