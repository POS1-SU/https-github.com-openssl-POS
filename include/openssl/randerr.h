/*
 * Generated by util/mkerr.pl DO NOT EDIT
 * Copyright 1995-2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef HEADER_RANDERR_H
# define HEADER_RANDERR_H

# ifdef  __cplusplus
extern "C" {
# endif
int ERR_load_RAND_strings(void);
# ifdef  __cplusplus
}
# endif

/*
 * RAND function codes.
 */
# define RAND_F_DRBG_BYTES                                101
# define RAND_F_DRBG_CHECK                                102
# define RAND_F_DRBG_GET_ENTROPY                          105
# define RAND_F_DRBG_RESEED                               103
# define RAND_F_RAND_BYTES                                100
# define RAND_F_RAND_DRBG_GENERATE                        107
# define RAND_F_RAND_DRBG_INSTANTIATE                     108
# define RAND_F_RAND_DRBG_NEW                             109
# define RAND_F_RAND_DRBG_SET                             104
# define RAND_F_RAND_LOAD_FILE                            106
# define RAND_F_RAND_WRITE_FILE                           110

/*
 * RAND reason codes.
 */
# define RAND_R_ADDITIONAL_INPUT_TOO_LONG                 102
# define RAND_R_ALREADY_INSTANTIATED                      103
# define RAND_R_CANNOT_OPEN_FILE                          121
# define RAND_R_DRBG_NOT_INITIALISED                      104
# define RAND_R_DRBG_STUCK                                105
# define RAND_R_ENTROPY_SOURCE_STUCK                      106
# define RAND_R_ERROR_INITIALISING_DRBG                   107
# define RAND_R_ERROR_INSTANTIATING_DRBG                  108
# define RAND_R_ERROR_RETRIEVING_ADDITIONAL_INPUT         109
# define RAND_R_ERROR_RETRIEVING_ENTROPY                  110
# define RAND_R_ERROR_RETRIEVING_NONCE                    111
# define RAND_R_FUNC_NOT_IMPLEMENTED                      101
# define RAND_R_FWRITE_ERROR                              123
# define RAND_R_GENERATE_ERROR                            112
# define RAND_R_INTERNAL_ERROR                            113
# define RAND_R_IN_ERROR_STATE                            114
# define RAND_R_NOT_A_REGULAR_FILE                        122
# define RAND_R_NOT_INSTANTIATED                          115
# define RAND_R_PERSONALISATION_STRING_TOO_LONG           116
# define RAND_R_PRNG_NOT_SEEDED                           100
# define RAND_R_REQUEST_TOO_LARGE_FOR_DRBG                117
# define RAND_R_RESEED_ERROR                              118
# define RAND_R_SELFTEST_FAILURE                          119
# define RAND_R_UNSUPPORTED_DRBG_TYPE                     120

#endif
