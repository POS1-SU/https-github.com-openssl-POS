/*
 * Copyright 2018-2019 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/err.h>
#include <openssl/ossl_typ.h>
#include <openssl/asn1.h>
#include <openssl/hmac.h>
#include "internal/evp_int.h"

/* local HMAC context structure */

/* typedef EVP_MAC_IMPL */
struct evp_mac_impl_st {
    const EVP_MD *md;         /* HMAC digest */
    const ENGINE *engine;     /* HMAC digest engine */
    HMAC_CTX *ctx;               /* HMAC context */
    unsigned char *key;
    size_t key_len;
};

static EVP_MAC_IMPL *hmac_new(void)
{
    EVP_MAC_IMPL *hctx;

    if ((hctx = OPENSSL_zalloc(sizeof(*hctx))) == NULL
        || (hctx->ctx = HMAC_CTX_new()) == NULL) {
        OPENSSL_free(hctx);
        return NULL;
    }

    return hctx;
}

static void hmac_free(EVP_MAC_IMPL *hctx)
{
    if (hctx != NULL) {
        OPENSSL_clear_free(hctx->key, hctx->key_len);
        HMAC_CTX_free(hctx->ctx);
        OPENSSL_free(hctx);
    }
}

static int hmac_copy(EVP_MAC_IMPL *hdst, EVP_MAC_IMPL *hsrc)
{
    if (HMAC_CTX_get_md(hsrc->ctx) != NULL
            && !HMAC_CTX_copy(hdst->ctx, hsrc->ctx))
        return 0;

    hdst->engine = hsrc->engine;
    hdst->md = hsrc->md;
    OPENSSL_clear_free(hdst->key, hdst->key_len);
    hdst->key = NULL;
    hdst->key_len = 0;
    if (hsrc->key != NULL) {
        hdst->key = OPENSSL_memdup(hsrc->key, hsrc->key_len);
        hdst->key_len = hsrc->key_len;
        return hdst->key != NULL;
    }
    return 1;
}

static size_t hmac_size(EVP_MAC_IMPL *hctx)
{
    return HMAC_size(hctx->ctx);
}

static int hmac_init(EVP_MAC_IMPL *hctx)
{
    return HMAC_Init_ex(hctx->ctx, hctx->key, hctx->key_len, hctx->md,
                       (ENGINE *)hctx->engine);
}

static int hmac_update(EVP_MAC_IMPL *hctx, const unsigned char *data,
                       size_t datalen)
{
    return HMAC_Update(hctx->ctx, data, datalen);
}

static int hmac_final(EVP_MAC_IMPL *hctx, unsigned char *out)
{
    unsigned int hlen;

    return HMAC_Final(hctx->ctx, out, &hlen);
}

static int hmac_ctrl(EVP_MAC_IMPL *hctx, int cmd, va_list args)
{
    switch (cmd) {
    case EVP_MAC_CTRL_SET_FLAGS:
        {
            unsigned long flags = va_arg(args, unsigned long);

            HMAC_CTX_set_flags(hctx->ctx, flags);
        }
        break;
    case EVP_MAC_CTRL_SET_KEY:
        {
            const unsigned char *key = va_arg(args, const unsigned char *);
            size_t keylen = va_arg(args, size_t);

            OPENSSL_clear_free(hctx->key, hctx->key_len);
            hctx->key = OPENSSL_memdup(key, keylen);
            hctx->key_len = keylen;

            return hctx->key != NULL;
        }
        break;
    case EVP_MAC_CTRL_SET_MD:
        hctx->md = va_arg(args, const EVP_MD *);
        break;
    case EVP_MAC_CTRL_SET_ENGINE:
        hctx->engine = va_arg(args, const ENGINE *);
        break;
    default:
        return -2;

    }
    return 1;
}

static int hmac_ctrl_int(EVP_MAC_IMPL *hctx, int cmd, ...)
{
    int rv;
    va_list args;

    va_start(args, cmd);
    rv = hmac_ctrl(hctx, cmd, args);
    va_end(args);

    return rv;
}

static int hmac_ctrl_str_cb(void *hctx, int cmd, void *buf, size_t buflen)
{
    return hmac_ctrl_int(hctx, cmd, buf, buflen);
}

static int hmac_ctrl_str(EVP_MAC_IMPL *hctx, const char *type,
                         const char *value)
{
    if (!value)
        return 0;
    if (strcmp(type, "digest") == 0) {
        const EVP_MD *d = EVP_get_digestbyname(value);

        if (d == NULL)
            return 0;
        return hmac_ctrl_int(hctx, EVP_MAC_CTRL_SET_MD, d);
    }
    if (strcmp(type, "key") == 0)
        return EVP_str2ctrl(hmac_ctrl_str_cb, hctx, EVP_MAC_CTRL_SET_KEY,
                            value);
    if (strcmp(type, "hexkey") == 0)
        return EVP_hex2ctrl(hmac_ctrl_str_cb, hctx, EVP_MAC_CTRL_SET_KEY,
                            value);
    return -2;
}

const EVP_MAC hmac_meth = {
    EVP_MAC_HMAC,
    hmac_new,
    hmac_copy,
    hmac_free,
    hmac_size,
    hmac_init,
    hmac_update,
    hmac_final,
    hmac_ctrl,
    hmac_ctrl_str
};
