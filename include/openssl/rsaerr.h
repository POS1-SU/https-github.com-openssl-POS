/*
 * Generated by util/mkerr.pl DO NOT EDIT
 * Copyright 1995-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OPENSSL_RSAERR_H
# define OPENSSL_RSAERR_H
# pragma once

# include <openssl/macros.h>
# ifndef OPENSSL_NO_DEPRECATED_3_0
#  define HEADER_RSAERR_H
# endif

# include <openssl/opensslconf.h>
# include <openssl/symhacks.h>


# ifdef  __cplusplus
extern "C"
# endif
int ERR_load_RSA_strings(void);

/*
 * RSA function codes.
 */
# ifndef OPENSSL_NO_DEPRECATED_3_0
#  define RSA_F_CHECK_PADDING_MD                           0
#  define RSA_F_ENCODE_PKCS1                               0
#  define RSA_F_INT_RSA_VERIFY                             0
#  define RSA_F_OLD_RSA_PRIV_DECODE                        0
#  define RSA_F_PKEY_PSS_INIT                              0
#  define RSA_F_PKEY_RSA_CTRL                              0
#  define RSA_F_PKEY_RSA_CTRL_STR                          0
#  define RSA_F_PKEY_RSA_SIGN                              0
#  define RSA_F_PKEY_RSA_VERIFY                            0
#  define RSA_F_PKEY_RSA_VERIFYRECOVER                     0
#  define RSA_F_RSA_ALGOR_TO_MD                            0
#  define RSA_F_RSA_BUILTIN_KEYGEN                         0
#  define RSA_F_RSA_CHECK_KEY                              0
#  define RSA_F_RSA_CHECK_KEY_EX                           0
#  define RSA_F_RSA_CMS_DECRYPT                            0
#  define RSA_F_RSA_CMS_VERIFY                             0
#  define RSA_F_RSA_FIPS186_4_GEN_PROB_PRIMES              0
#  define RSA_F_RSA_ITEM_VERIFY                            0
#  define RSA_F_RSA_METH_DUP                               0
#  define RSA_F_RSA_METH_NEW                               0
#  define RSA_F_RSA_METH_SET1_NAME                         0
#  define RSA_F_RSA_MGF1_TO_MD                             0
#  define RSA_F_RSA_MULTIP_INFO_NEW                        0
#  define RSA_F_RSA_NEW_METHOD                             0
#  define RSA_F_RSA_NULL                                   0
#  define RSA_F_RSA_NULL_PRIVATE_DECRYPT                   0
#  define RSA_F_RSA_NULL_PRIVATE_ENCRYPT                   0
#  define RSA_F_RSA_NULL_PUBLIC_DECRYPT                    0
#  define RSA_F_RSA_NULL_PUBLIC_ENCRYPT                    0
#  define RSA_F_RSA_OSSL_PRIVATE_DECRYPT                   0
#  define RSA_F_RSA_OSSL_PRIVATE_ENCRYPT                   0
#  define RSA_F_RSA_OSSL_PUBLIC_DECRYPT                    0
#  define RSA_F_RSA_OSSL_PUBLIC_ENCRYPT                    0
#  define RSA_F_RSA_PADDING_ADD_NONE                       0
#  define RSA_F_RSA_PADDING_ADD_PKCS1_OAEP                 0
#  define RSA_F_RSA_PADDING_ADD_PKCS1_OAEP_MGF1            0
#  define RSA_F_RSA_PADDING_ADD_PKCS1_PSS                  0
#  define RSA_F_RSA_PADDING_ADD_PKCS1_PSS_MGF1             0
#  define RSA_F_RSA_PADDING_ADD_PKCS1_TYPE_1               0
#  define RSA_F_RSA_PADDING_ADD_PKCS1_TYPE_2               0
#  define RSA_F_RSA_PADDING_ADD_SSLV23                     0
#  define RSA_F_RSA_PADDING_ADD_X931                       0
#  define RSA_F_RSA_PADDING_CHECK_NONE                     0
#  define RSA_F_RSA_PADDING_CHECK_PKCS1_OAEP               0
#  define RSA_F_RSA_PADDING_CHECK_PKCS1_OAEP_MGF1          0
#  define RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_1             0
#  define RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_2             0
#  define RSA_F_RSA_PADDING_CHECK_SSLV23                   0
#  define RSA_F_RSA_PADDING_CHECK_X931                     0
#  define RSA_F_RSA_PARAM_DECODE                           0
#  define RSA_F_RSA_PRINT                                  0
#  define RSA_F_RSA_PRINT_FP                               0
#  define RSA_F_RSA_PRIV_DECODE                            0
#  define RSA_F_RSA_PRIV_ENCODE                            0
#  define RSA_F_RSA_PSS_GET_PARAM                          0
#  define RSA_F_RSA_PSS_TO_CTX                             0
#  define RSA_F_RSA_PUB_DECODE                             0
#  define RSA_F_RSA_SETUP_BLINDING                         0
#  define RSA_F_RSA_SIGN                                   0
#  define RSA_F_RSA_SIGN_ASN1_OCTET_STRING                 0
#  define RSA_F_RSA_SP800_56B_CHECK_KEYPAIR                0
#  define RSA_F_RSA_SP800_56B_CHECK_PUBLIC                 0
#  define RSA_F_RSA_SP800_56B_PAIRWISE_TEST                0
#  define RSA_F_RSA_SP800_56B_VALIDATE_STRENGTH            0
#  define RSA_F_RSA_VERIFY                                 0
#  define RSA_F_RSA_VERIFY_ASN1_OCTET_STRING               0
#  define RSA_F_RSA_VERIFY_PKCS1_PSS_MGF1                  0
#  define RSA_F_SETUP_TBUF                                 0
# endif

/*
 * RSA reason codes.
 */
# define RSA_R_ALGORITHM_MISMATCH                         100
# define RSA_R_BAD_E_VALUE                                101
# define RSA_R_BAD_FIXED_HEADER_DECRYPT                   102
# define RSA_R_BAD_PAD_BYTE_COUNT                         103
# define RSA_R_BAD_SIGNATURE                              104
# define RSA_R_BLOCK_TYPE_IS_NOT_01                       106
# define RSA_R_BLOCK_TYPE_IS_NOT_02                       107
# define RSA_R_DATA_TOO_LARGE                             109
# define RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE                110
# define RSA_R_DATA_TOO_LARGE_FOR_MODULUS                 132
# define RSA_R_DATA_TOO_SMALL                             111
# define RSA_R_DATA_TOO_SMALL_FOR_KEY_SIZE                122
# define RSA_R_DIGEST_DOES_NOT_MATCH                      158
# define RSA_R_DIGEST_NOT_ALLOWED                         145
# define RSA_R_DIGEST_TOO_BIG_FOR_RSA_KEY                 112
# define RSA_R_DMP1_NOT_CONGRUENT_TO_D                    124
# define RSA_R_DMQ1_NOT_CONGRUENT_TO_D                    125
# define RSA_R_D_E_NOT_CONGRUENT_TO_1                     123
# define RSA_R_FIRST_OCTET_INVALID                        133
# define RSA_R_ILLEGAL_OR_UNSUPPORTED_PADDING_MODE        144
# define RSA_R_INVALID_DIGEST                             157
# define RSA_R_INVALID_DIGEST_LENGTH                      143
# define RSA_R_INVALID_HEADER                             137
# define RSA_R_INVALID_KEYPAIR                            171
# define RSA_R_INVALID_KEY_LENGTH                         173
# define RSA_R_INVALID_LABEL                              160
# define RSA_R_INVALID_MESSAGE_LENGTH                     131
# define RSA_R_INVALID_MGF1_MD                            156
# define RSA_R_INVALID_MODULUS                            174
# define RSA_R_INVALID_MULTI_PRIME_KEY                    167
# define RSA_R_INVALID_OAEP_PARAMETERS                    161
# define RSA_R_INVALID_PADDING                            138
# define RSA_R_INVALID_PADDING_MODE                       141
# define RSA_R_INVALID_PSS_PARAMETERS                     149
# define RSA_R_INVALID_PSS_SALTLEN                        146
# define RSA_R_INVALID_REQUEST                            175
# define RSA_R_INVALID_SALT_LENGTH                        150
# define RSA_R_INVALID_STRENGTH                           176
# define RSA_R_INVALID_TRAILER                            139
# define RSA_R_INVALID_X931_DIGEST                        142
# define RSA_R_IQMP_NOT_INVERSE_OF_Q                      126
# define RSA_R_KEY_PRIME_NUM_INVALID                      165
# define RSA_R_KEY_SIZE_TOO_SMALL                         120
# define RSA_R_LAST_OCTET_INVALID                         134
# define RSA_R_MGF1_DIGEST_NOT_ALLOWED                    152
# define RSA_R_MISSING_PRIVATE_KEY                        179
# define RSA_R_MODULUS_TOO_LARGE                          105
# define RSA_R_MP_COEFFICIENT_NOT_INVERSE_OF_R            168
# define RSA_R_MP_EXPONENT_NOT_CONGRUENT_TO_D             169
# define RSA_R_MP_R_NOT_PRIME                             170
# define RSA_R_NO_PUBLIC_EXPONENT                         140
# define RSA_R_NULL_BEFORE_BLOCK_MISSING                  113
# define RSA_R_N_DOES_NOT_EQUAL_PRODUCT_OF_PRIMES         172
# define RSA_R_N_DOES_NOT_EQUAL_P_Q                       127
# define RSA_R_OAEP_DECODING_ERROR                        121
# define RSA_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE   148
# define RSA_R_PADDING_CHECK_FAILED                       114
# define RSA_R_PAIRWISE_TEST_FAILURE                      177
# define RSA_R_PKCS_DECODING_ERROR                        159
# define RSA_R_PSS_SALTLEN_TOO_SMALL                      164
# define RSA_R_PUB_EXPONENT_OUT_OF_RANGE                  178
# define RSA_R_P_NOT_PRIME                                128
# define RSA_R_Q_NOT_PRIME                                129
# define RSA_R_RSA_OPERATIONS_NOT_SUPPORTED               130
# define RSA_R_SLEN_CHECK_FAILED                          136
# define RSA_R_SLEN_RECOVERY_FAILED                       135
# define RSA_R_SSLV3_ROLLBACK_ATTACK                      115
# define RSA_R_THE_ASN1_OBJECT_IDENTIFIER_IS_NOT_KNOWN_FOR_THIS_MD 116
# define RSA_R_UNKNOWN_ALGORITHM_TYPE                     117
# define RSA_R_UNKNOWN_DIGEST                             166
# define RSA_R_UNKNOWN_MASK_DIGEST                        151
# define RSA_R_UNKNOWN_PADDING_TYPE                       118
# define RSA_R_UNSUPPORTED_ENCRYPTION_TYPE                162
# define RSA_R_UNSUPPORTED_LABEL_SOURCE                   163
# define RSA_R_UNSUPPORTED_MASK_ALGORITHM                 153
# define RSA_R_UNSUPPORTED_MASK_PARAMETER                 154
# define RSA_R_UNSUPPORTED_SIGNATURE_TYPE                 155
# define RSA_R_VALUE_MISSING                              147
# define RSA_R_WRONG_SIGNATURE_LENGTH                     119

#endif
