/*
 * Copyright 2019 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/core_numbers.h>
#include <openssl/core_names.h>
#include <openssl/bn.h>
#include <openssl/params.h>
#include "internal/param_build.h"
#include "prov/implementations.h"
#include "prov/providercommon.h"
#include "prov/provider_ctx.h"
#include "crypto/dsa.h"

static OSSL_OP_keymgmt_importdomparams_fn dsa_importdomparams;
static OSSL_OP_keymgmt_exportdomparams_fn dsa_exportdomparams;
static OSSL_OP_keymgmt_get_domparam_params_fn dsa_get_domparam_params;
static OSSL_OP_keymgmt_isdomparams_fn dsa_isdomparams;
static OSSL_OP_keymgmt_cmpdomparams_fn dsa_cmpdomparams;
static OSSL_OP_keymgmt_dupdomparams_fn dsa_dupdomparams;
static OSSL_OP_keymgmt_importkey_fn dsa_importkey;
static OSSL_OP_keymgmt_exportkey_fn dsa_exportkey;
static OSSL_OP_keymgmt_get_key_params_fn dsa_get_key_params;
static OSSL_OP_keymgmt_iskey_fn dsa_iskey;
static OSSL_OP_keymgmt_cmpkey_fn dsa_cmpkey;
static OSSL_OP_keymgmt_dupkey_fn dsa_dupkey;

#define DSA_DEFAULT_MD "SHA256"

static int params_to_domparams(DSA *dsa, const OSSL_PARAM params[])
{
    const OSSL_PARAM *param_p, *param_q, *param_g;
    BIGNUM *p = NULL, *q = NULL, *g = NULL;

    if (dsa == NULL)
        return 0;

    param_p = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_FFC_P);
    param_q = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_FFC_Q);
    param_g = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_FFC_G);

    if ((param_p != NULL && !OSSL_PARAM_get_BN(param_p, &p))
        || (param_q != NULL && !OSSL_PARAM_get_BN(param_q, &q))
        || (param_g != NULL && !OSSL_PARAM_get_BN(param_g, &g)))
        goto err;

    if (!DSA_set0_pqg(dsa, p, q, g))
        goto err;

    return 1;

 err:
    BN_free(p);
    BN_free(q);
    BN_free(g);
    return 0;
}

static int domparams_to_params(DSA *dsa, OSSL_PARAM_BLD *tmpl)
{
    const BIGNUM *dsa_p = NULL, *dsa_q = NULL, *dsa_g = NULL;

    if (dsa == NULL)
        return 0;

    DSA_get0_pqg(dsa, &dsa_p, &dsa_q, &dsa_g);
    if (dsa_p != NULL
        && !ossl_param_bld_push_BN(tmpl, OSSL_PKEY_PARAM_FFC_P, dsa_p))
        return 0;
    if (dsa_q != NULL
        && !ossl_param_bld_push_BN(tmpl, OSSL_PKEY_PARAM_FFC_Q, dsa_q))
        return 0;
    if (dsa_g != NULL
        && !ossl_param_bld_push_BN(tmpl, OSSL_PKEY_PARAM_FFC_G, dsa_g))
        return 0;

    return 1;
}

static int params_to_key(DSA *dsa, const OSSL_PARAM params[])
{
    const OSSL_PARAM *param_priv_key, *param_pub_key;
    BIGNUM *priv_key = NULL, *pub_key = NULL;

    if (dsa == NULL)
        return 0;

    if (!params_to_domparams(dsa, params))
        return 0;

    param_priv_key =
        OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_DSA_PRIV_KEY);
    param_pub_key =
        OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_DSA_PUB_KEY);

    /*
     * DSA documentation says that a public key must be present if a private key
     * is.
     */
    if (param_priv_key != NULL && param_pub_key == NULL)
        return 0;

    if ((param_priv_key != NULL
         && !OSSL_PARAM_get_BN(param_priv_key, &priv_key))
        || (param_pub_key != NULL
            && !OSSL_PARAM_get_BN(param_pub_key, &pub_key)))
        goto err;

    if (pub_key != NULL && !DSA_set0_key(dsa, pub_key, priv_key))
        goto err;

    return 1;

 err:
    BN_free(priv_key);
    BN_free(pub_key);
    return 0;
}

static int key_to_params(DSA *dsa, OSSL_PARAM_BLD *tmpl)
{
    const BIGNUM *priv_key = NULL, *pub_key = NULL;

    if (dsa == NULL)
        return 0;
    if (!domparams_to_params(dsa, tmpl))
        return 0;

    DSA_get0_key(dsa, &pub_key, &priv_key);
    if (priv_key != NULL
        && !ossl_param_bld_push_BN(tmpl, OSSL_PKEY_PARAM_DSA_PRIV_KEY, priv_key))
        return 0;
    if (pub_key != NULL
        && !ossl_param_bld_push_BN(tmpl, OSSL_PKEY_PARAM_DSA_PUB_KEY, pub_key))
        return 0;

    return 1;
}

static void *dsa_importdomparams(void *provctx, const OSSL_PARAM params[])
{
    DSA *dsa;

    if ((dsa = DSA_new()) == NULL
        || !params_to_domparams(dsa, params)) {
        DSA_free(dsa);
        dsa = NULL;
    }
    return dsa;
}

static int dsa_exportdomparams(void *domparams,
                               OSSL_CALLBACK *param_cb, void *cbarg)
{
    DSA *dsa = domparams;
    OSSL_PARAM_BLD tmpl;
    OSSL_PARAM *params = NULL;
    int ret;

    ossl_param_bld_init(&tmpl);
    if (dsa == NULL
        || !domparams_to_params(dsa, &tmpl)
        || (params = ossl_param_bld_to_param(&tmpl)) == NULL)
        return 0;
    ret = param_cb(params, cbarg);
    ossl_param_bld_free(params);
    return ret;
}

static int dsa_isdomparams(const void *domparams)
{
    /*
     * dsa should always contain the domain parameters, so we could as well
     * return 1 here and be done with it.  However, future development might
     * change this, so we make this future proof and test for real.
     */
    return DSA_get0_p(domparams) != NULL
        && DSA_get0_q(domparams) != NULL
        && DSA_get0_g(domparams) != NULL;
}

static int dsa_cmpdomparams(const void *domparams1, const void *domparams2)
{
    if (BN_cmp(DSA_get0_p(domparams1), DSA_get0_p(domparams2)) != 0
        && BN_cmp(DSA_get0_q(domparams1), DSA_get0_q(domparams2)) != 0
        && BN_cmp(DSA_get0_g(domparams1), DSA_get0_g(domparams2)) != 0)
        return 0;
    return 1;
}

static void *dsa_dupdomparams(void *domparams, int do_copy)
{
    DSA *new = domparams;

    if (do_copy)
        new = DSAparams_dup(domparams);
    else
        DSA_up_ref(new);
    return new;
}

static void *dsa_importkey(void *provctx, const OSSL_PARAM params[])
{
    DSA *dsa;

    if ((dsa = DSA_new()) == NULL
        || !params_to_key(dsa, params)) {
        DSA_free(dsa);
        dsa = NULL;
    }
    return dsa;
}

static int dsa_exportkey(void *key, OSSL_CALLBACK *param_cb, void *cbarg)
{
    DSA *dsa = key;
    OSSL_PARAM_BLD tmpl;
    OSSL_PARAM *params = NULL;
    int ret;

    ossl_param_bld_init(&tmpl);
    if (dsa == NULL
        || !key_to_params(dsa, &tmpl)
        || (params = ossl_param_bld_to_param(&tmpl)) == NULL)
        return 0;
    ret = param_cb(params, cbarg);
    ossl_param_bld_free(params);
    return ret;
}

/*
 * Same function for domain parameters and for keys.
 * "dpk" = "domain parameters & keys"
 */
static ossl_inline int dsa_get_dpk_params(void *key, OSSL_PARAM params[])
{
    DSA *dsa = key;
    OSSL_PARAM *p;

    if ((p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_BITS)) != NULL
        && !OSSL_PARAM_set_int(p, DSA_bits(dsa)))
        return 0;
    if ((p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_SECURITY_BITS)) != NULL
        && !OSSL_PARAM_set_int(p, DSA_security_bits(dsa)))
        return 0;
    if ((p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_MAX_SIZE)) != NULL
        && !OSSL_PARAM_set_int(p, DSA_size(dsa)))
        return 0;
    if ((p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_DEFAULT_DIGEST)) != NULL
        && !OSSL_PARAM_set_utf8_string(p, DSA_DEFAULT_MD))
        return 0;
    return 1;
}

/*
 * We have wrapper functions to make sure we get signatures right, see
 * the forward declarations at the beginning of this file.
 */
static int dsa_get_domparam_params(void *domparams, OSSL_PARAM params[])
{
    return dsa_get_dpk_params(domparams, params);
}

static int dsa_get_key_params(void *key, OSSL_PARAM params[])
{
    return dsa_get_dpk_params(key, params);
}

static int dsa_iskey(const void *key)
{
    return DSA_get0_pub_key(key) != NULL;
}

static int dsa_cmpkey(const void *key1, const void *key2)
{
    if (BN_cmp(DSA_get0_pub_key(key1), DSA_get0_pub_key(key2)) != 0)
        return 0;
    return 1;
}

static void *dsa_dupkey(void *key, int do_copy)
{
    if (do_copy)
        /*
         * the EVP library currently only supports copying domain params,
         * so we don't need to care...  besides, if we want to support
         * copying DSA keys, there should be a function in the low level
         * DSA library.
         */
        return NULL;
    else
        DSA_up_ref(key);
    return key;
}

const OSSL_DISPATCH dsa_keymgmt_functions[] = {
    { OSSL_FUNC_KEYMGMT_IMPORTDOMPARAMS, (void (*)(void))dsa_importdomparams },
    { OSSL_FUNC_KEYMGMT_EXPORTDOMPARAMS, (void (*)(void))dsa_exportdomparams },
    { OSSL_FUNC_KEYMGMT_FREEDOMPARAMS, (void (*)(void))DSA_free },
    { OSSL_FUNC_KEYMGMT_GET_DOMPARAM_PARAMS,
      (void (*) (void))dsa_get_domparam_params },
    { OSSL_FUNC_KEYMGMT_ISDOMPARAMS, (void (*)(void))dsa_isdomparams },
    { OSSL_FUNC_KEYMGMT_CMPDOMPARAMS, (void (*)(void))dsa_cmpdomparams },
    { OSSL_FUNC_KEYMGMT_DUPDOMPARAMS, (void (*)(void))dsa_dupdomparams },
    { OSSL_FUNC_KEYMGMT_IMPORTKEY, (void (*)(void))dsa_importkey },
    { OSSL_FUNC_KEYMGMT_EXPORTKEY, (void (*)(void))dsa_exportkey },
    { OSSL_FUNC_KEYMGMT_FREEKEY, (void (*)(void))DSA_free },
    { OSSL_FUNC_KEYMGMT_GET_KEY_PARAMS,  (void (*) (void))dsa_get_key_params },
    { OSSL_FUNC_KEYMGMT_ISKEY, (void (*)(void))dsa_iskey },
    { OSSL_FUNC_KEYMGMT_CMPKEY, (void (*)(void))dsa_cmpkey },
    { OSSL_FUNC_KEYMGMT_DUPKEY, (void (*)(void))dsa_dupkey },
    { 0, NULL }
};
