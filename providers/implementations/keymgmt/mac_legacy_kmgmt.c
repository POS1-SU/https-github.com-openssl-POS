/*
 * Copyright 2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include "openssl/param_build.h"
#include "internal/param_build_set.h"
#include "prov/implementations.h"
#include "prov/providercommon.h"
#include "prov/provider_ctx.h"
#include "prov/macsignature.h"


static OSSL_FUNC_keymgmt_new_fn mac_new;
static OSSL_FUNC_keymgmt_free_fn mac_free;
static OSSL_FUNC_keymgmt_gen_init_fn mac_gen_init;
static OSSL_FUNC_keymgmt_gen_fn mac_gen;
static OSSL_FUNC_keymgmt_gen_cleanup_fn mac_gen_cleanup;
static OSSL_FUNC_keymgmt_get_params_fn mac_get_params;
static OSSL_FUNC_keymgmt_gettable_params_fn mac_gettable_params;
static OSSL_FUNC_keymgmt_set_params_fn mac_set_params;
static OSSL_FUNC_keymgmt_settable_params_fn mac_settable_params;
static OSSL_FUNC_keymgmt_has_fn mac_has;
static OSSL_FUNC_keymgmt_match_fn mac_match;
static OSSL_FUNC_keymgmt_import_fn mac_import;
static OSSL_FUNC_keymgmt_import_types_fn mac_imexport_types;
static OSSL_FUNC_keymgmt_export_fn mac_export;
static OSSL_FUNC_keymgmt_export_types_fn mac_imexport_types;

struct mac_gen_ctx {
    OPENSSL_CTX *libctx;
    int selection;
    unsigned char *priv_key;
    size_t priv_key_len;
};

MAC_KEY *mac_key_new(OPENSSL_CTX *libctx)
{
    MAC_KEY *mackey = OPENSSL_zalloc(sizeof(*mackey));

    if (mackey == NULL)
        return NULL;

    mackey->lock = CRYPTO_THREAD_lock_new();
    if (mackey->lock == NULL) {
        OPENSSL_free(mackey);
        return NULL;
    }
    mackey->libctx = libctx;
    mackey->refcnt = 1;

    return mackey;
}

void mac_key_free(MAC_KEY *mackey)
{
    int ref = 0;

    if (mackey == NULL)
        return;

    CRYPTO_DOWN_REF(&mackey->refcnt, &ref, mackey->lock);
    if (ref > 0)
        return;

    OPENSSL_secure_clear_free(mackey->priv_key, mackey->priv_key_len);
    CRYPTO_THREAD_lock_free(mackey->lock);
    OPENSSL_free(mackey);
}

int mac_key_up_ref(MAC_KEY *mackey)
{
    int ref = 0;

    CRYPTO_UP_REF(&mackey->refcnt, &ref, mackey->lock);
    return 1;
}

static void *mac_new(void *provctx)
{
    return mac_key_new(PROV_LIBRARY_CONTEXT_OF(provctx));
}

static void mac_free(void *mackey)
{
    mac_key_free(mackey);
}

static int mac_has(void *keydata, int selection)
{
    MAC_KEY *key = keydata;
    int ok = 0;

    if (key != NULL) {
        /*
         * MAC keys always have all the parameters they need (i.e. none).
         * Therefore we always return with 1, if asked about parameters.
         * Similarly for public keys.
         */
        ok = 1;

        if ((selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) != 0)
            ok = key->priv_key != NULL;
    }
    return ok;
}

static int mac_match(const void *keydata1, const void *keydata2, int selection)
{
    const MAC_KEY *key1 = keydata1;
    const MAC_KEY *key2 = keydata2;
    int ok = 1;

    if ((selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) != 0) {
        if ((key1->priv_key == NULL && key2->priv_key != NULL)
                || (key1->priv_key != NULL && key2->priv_key == NULL)
                || key1->priv_key_len != key2->priv_key_len)
            ok = 0;
        else
            ok = ok && (key1->priv_key == NULL /* implies key2->privkey == NULL */
                        || CRYPTO_memcmp(key1->priv_key, key2->priv_key,
                                         key1->priv_key_len) == 0);
    }
    return ok;
}

static int mac_key_fromdata(MAC_KEY *key, const OSSL_PARAM params[])
{
    const OSSL_PARAM *p;

    p = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_PRIV_KEY);
    if (p != NULL) {
        if (p->data_type != OSSL_PARAM_OCTET_STRING) {
            ERR_raise(ERR_LIB_PROV, ERR_R_PASSED_INVALID_ARGUMENT);
            return 0;
        }
        OPENSSL_secure_clear_free(key->priv_key, key->priv_key_len);
        key->priv_key = OPENSSL_secure_malloc(p->data_size);
        if (key->priv_key == NULL) {
            ERR_raise(ERR_LIB_PROV, ERR_R_MALLOC_FAILURE);
            return 0;
        }
        memcpy(key->priv_key, p->data, p->data_size);
        key->priv_key_len = p->data_size;

        return 1;
    }
    return 0;
}

static int mac_import(void *keydata, int selection, const OSSL_PARAM params[])
{
    MAC_KEY *key = keydata;

    if (key == NULL)
        return 0;

    if ((selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) == 0)
        return 0;

    return mac_key_fromdata(key, params);
}

static int key_to_params(MAC_KEY *key, OSSL_PARAM_BLD *tmpl,
                         OSSL_PARAM params[])
{
    if (key == NULL)
        return 0;

    if (key->priv_key != NULL
        && !ossl_param_build_set_octet_string(tmpl, params,
                                              OSSL_PKEY_PARAM_PRIV_KEY,
                                              key->priv_key, key->priv_key_len))
        return 0;

    return 1;
}

static int mac_export(void *keydata, int selection, OSSL_CALLBACK *param_cb,
                      void *cbarg)
{
    MAC_KEY *key = keydata;
    OSSL_PARAM_BLD *tmpl;
    OSSL_PARAM *params = NULL;
    int ret = 0;

    if (key == NULL)
        return 0;

    tmpl = OSSL_PARAM_BLD_new();
    if (tmpl == NULL)
        return 0;

    if ((selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) != 0
         && !key_to_params(key, tmpl, NULL))
        goto err;

    params = OSSL_PARAM_BLD_to_param(tmpl);
    if (params == NULL)
        goto err;

    ret = param_cb(params, cbarg);
    OSSL_PARAM_BLD_free_params(params);
err:
    OSSL_PARAM_BLD_free(tmpl);
    return ret;
}

static const OSSL_PARAM mac_key_types[] = {
    OSSL_PARAM_octet_string(OSSL_PKEY_PARAM_PRIV_KEY, NULL, 0),
    OSSL_PARAM_END
};
static const OSSL_PARAM *mac_imexport_types(int selection)
{
    if ((selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) != 0)
        return mac_key_types;
    return NULL;
}

static int mac_get_params(void *key, OSSL_PARAM params[])
{
    return key_to_params(key, NULL, params);
}

static const OSSL_PARAM *mac_gettable_params(void *provctx)
{
    static const OSSL_PARAM gettable_params[] = {
        OSSL_PARAM_octet_string(OSSL_PKEY_PARAM_PRIV_KEY, NULL, 0),
        OSSL_PARAM_END
    };
    return gettable_params;
}

static int mac_set_params(void *keydata, const OSSL_PARAM params[])
{
    MAC_KEY *key = keydata;
    const OSSL_PARAM *p;

    if (key == NULL)
        return 0;

    p = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_PRIV_KEY);
    if (p != NULL)
        return mac_key_fromdata(key, params);

    return 1;
}

static const OSSL_PARAM *mac_settable_params(void *provctx)
{
    static const OSSL_PARAM settable_params[] = {
        OSSL_PARAM_octet_string(OSSL_PKEY_PARAM_PRIV_KEY, NULL, 0),
        OSSL_PARAM_END
    };
    return settable_params;
}

static void *mac_gen_init(void *provctx, int selection)
{
    OPENSSL_CTX *libctx = PROV_LIBRARY_CONTEXT_OF(provctx);
    struct mac_gen_ctx *gctx = NULL;

    if ((gctx = OPENSSL_malloc(sizeof(*gctx))) != NULL) {
        gctx->libctx = libctx;
        gctx->selection = selection;
    }
    return gctx;
}


static int mac_gen_set_params(void *genctx, const OSSL_PARAM params[])
{
    struct mac_gen_ctx *gctx = genctx;
    const OSSL_PARAM *p;

    if (gctx == NULL)
        return 0;

    p = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_PRIV_KEY);
    if (p != NULL) {
        if (p->data_type != OSSL_PARAM_OCTET_STRING) {
            ERR_raise(ERR_LIB_PROV, ERR_R_PASSED_INVALID_ARGUMENT);
            return 0;
        }
        gctx->priv_key = OPENSSL_secure_malloc(p->data_size);
        if (gctx->priv_key == NULL) {
            ERR_raise(ERR_LIB_PROV, ERR_R_MALLOC_FAILURE);
            return 0;
        }
        memcpy(gctx->priv_key, p->data, p->data_size);
        gctx->priv_key_len = p->data_size;
    }

    return 1;
}

static const OSSL_PARAM *mac_gen_settable_params(void *provctx)
{
    static OSSL_PARAM settable[] = {
        OSSL_PARAM_octet_string(OSSL_PKEY_PARAM_PRIV_KEY, NULL, 0),
        OSSL_PARAM_END
    };
    return settable;
}

static void *mac_gen(void *genctx, OSSL_CALLBACK *cb, void *cbarg)
{
    struct mac_gen_ctx *gctx = genctx;
    MAC_KEY *key;

    if (gctx == NULL)
        return NULL;

    if ((key = mac_key_new(gctx->libctx)) == NULL) {
        ERR_raise(ERR_LIB_PROV, ERR_R_MALLOC_FAILURE);
        return NULL;
    }

    /* If we're doing parameter generation then we just return a blank key */
    if ((gctx->selection & OSSL_KEYMGMT_SELECT_KEYPAIR) == 0)
        return key;

    if (gctx->priv_key == NULL) {
        ERR_raise(ERR_LIB_PROV, EVP_R_INVALID_KEY);
        mac_key_free(key);
        return NULL;
    }

    /*
     * This is horrible but required for backwards compatibility. We don't
     * actually do real key generation at all. We simply copy the key that was
     * previously set in the gctx. Hopefully at some point in the future all
     * of this can be removed and we will only support the EVP_KDF APIs.
     */
    key->priv_key = gctx->priv_key;
    key->priv_key_len = gctx->priv_key_len;
    gctx->priv_key_len = 0;
    gctx->priv_key = NULL;

    return key;
}

static void mac_gen_cleanup(void *genctx)
{
    struct mac_gen_ctx *gctx = genctx;

    OPENSSL_secure_clear_free(gctx->priv_key, gctx->priv_key_len);
    OPENSSL_free(gctx);
}

const OSSL_DISPATCH mac_keymgmt_functions[] = {
    { OSSL_FUNC_KEYMGMT_NEW, (void (*)(void))mac_new },
    { OSSL_FUNC_KEYMGMT_FREE, (void (*)(void))mac_free },
    { OSSL_FUNC_KEYMGMT_GET_PARAMS, (void (*) (void))mac_get_params },
    { OSSL_FUNC_KEYMGMT_GETTABLE_PARAMS, (void (*) (void))mac_gettable_params },
    { OSSL_FUNC_KEYMGMT_SET_PARAMS, (void (*) (void))mac_set_params },
    { OSSL_FUNC_KEYMGMT_SETTABLE_PARAMS, (void (*) (void))mac_settable_params },
    { OSSL_FUNC_KEYMGMT_HAS, (void (*)(void))mac_has },
    { OSSL_FUNC_KEYMGMT_MATCH, (void (*)(void))mac_match },
    { OSSL_FUNC_KEYMGMT_IMPORT, (void (*)(void))mac_import },
    { OSSL_FUNC_KEYMGMT_IMPORT_TYPES, (void (*)(void))mac_imexport_types },
    { OSSL_FUNC_KEYMGMT_EXPORT, (void (*)(void))mac_export },
    { OSSL_FUNC_KEYMGMT_EXPORT_TYPES, (void (*)(void))mac_imexport_types },
    { OSSL_FUNC_KEYMGMT_GEN_INIT, (void (*)(void))mac_gen_init },
    { OSSL_FUNC_KEYMGMT_GEN_SET_PARAMS, (void (*)(void))mac_gen_set_params },
    { OSSL_FUNC_KEYMGMT_GEN_SETTABLE_PARAMS,
        (void (*)(void))mac_gen_settable_params },
    { OSSL_FUNC_KEYMGMT_GEN, (void (*)(void))mac_gen },
    { OSSL_FUNC_KEYMGMT_GEN_CLEANUP, (void (*)(void))mac_gen_cleanup },
    { 0, NULL }
};
