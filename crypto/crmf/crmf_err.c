/*
 * Generated by util/mkerr.pl DO NOT EDIT
 * Copyright 1995-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/err.h>
#include <openssl/crmferr.h>
#include "crypto/crmferr.h"

#ifndef OPENSSL_NO_CRMF

# ifndef OPENSSL_NO_ERR

static const ERR_STRING_DATA CRMF_str_reasons[] = {
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_BAD_PBM_ITERATIONCOUNT),
    "bad pbm iterationcount"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_CRMFERROR), "crmferror"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_ERROR), "error"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_ERROR_DECODING_CERTIFICATE),
    "error decoding certificate"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_ERROR_DECRYPTING_CERTIFICATE),
    "error decrypting certificate"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_ERROR_DECRYPTING_SYMMETRIC_KEY),
    "error decrypting symmetric key"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_FAILURE_OBTAINING_RANDOM),
    "failure obtaining random"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_ITERATIONCOUNT_BELOW_100),
    "iterationcount below 100"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_MALFORMED_IV), "malformed iv"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_NULL_ARGUMENT), "null argument"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_POPOSKINPUT_NOT_SUPPORTED),
    "poposkinput not supported"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_POPO_INCONSISTENT_PUBLIC_KEY),
    "popo inconsistent public key"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_POPO_MISSING), "popo missing"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_POPO_MISSING_PUBLIC_KEY),
    "popo missing public key"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_POPO_MISSING_SUBJECT),
    "popo missing subject"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_POPO_RAVERIFIED_NOT_ACCEPTED),
    "popo raverified not accepted"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_SETTING_MAC_ALGOR_FAILURE),
    "setting mac algor failure"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_SETTING_OWF_ALGOR_FAILURE),
    "setting owf algor failure"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_UNSUPPORTED_ALGORITHM),
    "unsupported algorithm"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_UNSUPPORTED_CIPHER),
    "unsupported cipher"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_UNSUPPORTED_METHOD_FOR_CREATING_POPO),
    "unsupported method for creating popo"},
    {ERR_PACK(ERR_LIB_CRMF, 0, CRMF_R_UNSUPPORTED_POPO_METHOD),
    "unsupported popo method"},
    {0, NULL}
};

# endif

int ossl_err_load_CRMF_strings(void)
{
# ifndef OPENSSL_NO_ERR
    if (ERR_reason_error_string(CRMF_str_reasons[0].error) == NULL)
        ERR_load_strings_const(CRMF_str_reasons);
# endif
    return 1;
}
#else
NON_EMPTY_TRANSLATION_UNIT
#endif
