/*
 * Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * low level APIs are deprecated for public use, but still ok for
 * internal use.
 */
#include "internal/deprecated.h"

#include <string.h>

#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/core_object.h>
#include <openssl/crypto.h>
#include <openssl/params.h>
#include <openssl/pem.h>         /* For public PVK functions */
#include <openssl/x509.h>
#include <openssl/err.h>
#include "internal/passphrase.h"
#include "crypto/pem.h"          /* For internal PVK and "blob" headers */
#include "crypto/rsa.h"
#include "prov/bio.h"
#include "prov/implementations.h"
#include "endecoder_local.h"

struct msblob2key_ctx_st;            /* Forward declaration */
typedef void *b2i_of_void_fn(const unsigned char **in, unsigned int bitlen,
                             int ispub);
typedef void adjust_key_fn(void *, struct msblob2key_ctx_st *ctx);
typedef void free_key_fn(void *);
struct keytype_desc_st {
    int type;                 /* EVP key type */
    const char *name;         /* Keytype */
    const OSSL_DISPATCH *fns; /* Keymgmt (to pilfer functions from) */

    b2i_of_void_fn *read_private_key;
    b2i_of_void_fn *read_public_key;
    adjust_key_fn *adjust_key;
    free_key_fn *free_key;
};

static OSSL_FUNC_decoder_freectx_fn msblob2key_freectx;
static OSSL_FUNC_decoder_gettable_params_fn msblob2key_gettable_params;
static OSSL_FUNC_decoder_get_params_fn msblob2key_get_params;
static OSSL_FUNC_decoder_decode_fn msblob2key_decode;
static OSSL_FUNC_decoder_export_object_fn msblob2key_export_object;

/*
 * Context used for DER to key decoding.
 */
struct msblob2key_ctx_st {
    PROV_CTX *provctx;
    const struct keytype_desc_st *desc;
};

static struct msblob2key_ctx_st *
msblob2key_newctx(void *provctx, const struct keytype_desc_st *desc)
{
    struct msblob2key_ctx_st *ctx = OPENSSL_zalloc(sizeof(*ctx));

    if (ctx != NULL) {
        ctx->provctx = provctx;
        ctx->desc = desc;
    }
    return ctx;
}

static void msblob2key_freectx(void *vctx)
{
    struct msblob2key_ctx_st *ctx = vctx;

    OPENSSL_free(ctx);
}

static const OSSL_PARAM *msblob2key_gettable_params(ossl_unused void *provctx)
{
    static const OSSL_PARAM gettables[] = {
        { OSSL_DECODER_PARAM_INPUT_TYPE, OSSL_PARAM_UTF8_PTR, NULL, 0, 0 },
        OSSL_PARAM_END,
    };

    return gettables;
}

static int msblob2key_get_params(OSSL_PARAM params[])
{
    OSSL_PARAM *p;

    p = OSSL_PARAM_locate(params, OSSL_DECODER_PARAM_INPUT_TYPE);
    if (p != NULL && !OSSL_PARAM_set_utf8_ptr(p, "MSBLOB"))
        return 0;

    return 1;
}

static int msblob2key_decode(void *vctx, OSSL_CORE_BIO *cin, int selection,
                             OSSL_CALLBACK *data_cb, void *data_cbarg,
                             OSSL_PASSPHRASE_CALLBACK *pw_cb, void *pw_cbarg)
{
    struct msblob2key_ctx_st *ctx = vctx;
    BIO *in = bio_new_from_core_bio(ctx->provctx, cin);
    const unsigned char *p;
    unsigned char hdr_buf[16], *buf = NULL;
    unsigned int bitlen, magic, length;
    int isdss = -1;
    int ispub = -1;
    void *key = NULL;
    int ok = 0;

    if (BIO_read(in, hdr_buf, 16) != 16) {
        ERR_raise(ERR_LIB_PEM, PEM_R_KEYBLOB_TOO_SHORT);
        goto err;
    }
    p = hdr_buf;
    if (ossl_do_blob_header(&p, 16, &magic, &bitlen, &isdss, &ispub) <= 0)
        goto err;

    if ((isdss && ctx->desc->type != EVP_PKEY_DSA)
        || (!isdss && ctx->desc->type != EVP_PKEY_RSA))
        goto err;

    length = ossl_blob_length(bitlen, isdss, ispub);
    if (length > BLOB_MAX_LENGTH) {
        ERR_raise(ERR_LIB_PEM, PEM_R_HEADER_TOO_LONG);
        goto err;
    }
    buf = OPENSSL_malloc(length);
    if (buf == NULL) {
        ERR_raise(ERR_LIB_PEM, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    p = buf;
    if (BIO_read(in, buf, length) != (int)length) {
        ERR_raise(ERR_LIB_PEM, PEM_R_KEYBLOB_TOO_SHORT);
        goto err;
    }

    if ((selection == 0
         || (selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) != 0)
        && !ispub
        && ctx->desc->read_private_key != NULL) {
        struct ossl_passphrase_data_st pwdata;

        memset(&pwdata, 0, sizeof(pwdata));
        if (!ossl_pw_set_ossl_passphrase_cb(&pwdata, pw_cb, pw_cbarg))
            goto err;
        p = buf;
        key = ctx->desc->read_private_key(&p, bitlen, ispub);
        if (selection != 0 && key == NULL)
            goto next;
    }
    if (key == NULL && (selection == 0
         || (selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0)
        && ispub
        && ctx->desc->read_public_key != NULL) {
        p = buf;
        key = ctx->desc->read_public_key(&p, bitlen, ispub);
        if (selection != 0 && key == NULL)
            goto next;
    }

    if (key != NULL && ctx->desc->adjust_key != NULL)
        ctx->desc->adjust_key(key, ctx);

 next:
    /*
     * We free resources here so it's not held up during the callback, because
     * we know the process is recursive and the allocated chunks of memory
     * add up.
     */
    OPENSSL_free(buf);
    BIO_free(in);
    buf = NULL;
    in = NULL;

    if (key != NULL) {
        OSSL_PARAM params[4];
        int object_type = OSSL_OBJECT_PKEY;

        params[0] =
            OSSL_PARAM_construct_int(OSSL_OBJECT_PARAM_TYPE, &object_type);
        params[1] =
            OSSL_PARAM_construct_utf8_string(OSSL_OBJECT_PARAM_DATA_TYPE,
                                             (char *)ctx->desc->name, 0);
        /* The address of the key becomes the octet string */
        params[2] =
            OSSL_PARAM_construct_octet_string(OSSL_OBJECT_PARAM_REFERENCE,
                                              &key, sizeof(key));
        params[3] = OSSL_PARAM_construct_end();

        ok = data_cb(params, data_cbarg);
    }

 err:
    BIO_free(in);
    OPENSSL_free(buf);
    ctx->desc->free_key(key);

    return ok;
}

static int
msblob2key_export_object(void *vctx,
                         const void *reference, size_t reference_sz,
                         OSSL_CALLBACK *export_cb, void *export_cbarg)
{
    struct msblob2key_ctx_st *ctx = vctx;
    OSSL_FUNC_keymgmt_export_fn *export =
        ossl_prov_get_keymgmt_export(ctx->desc->fns);
    void *keydata;

    if (reference_sz == sizeof(keydata) && export != NULL) {
        /* The contents of the reference is the address to our object */
        keydata = *(void **)reference;

        return export(keydata, OSSL_KEYMGMT_SELECT_ALL,
                      export_cb, export_cbarg);
    }
    return 0;
}

/* ---------------------------------------------------------------------- */

#define dsa_decode_private_key  (b2i_of_void_fn *)ossl_b2i_DSA_after_header
#define dsa_decode_public_key   (b2i_of_void_fn *)ossl_b2i_DSA_after_header
#define dsa_adjust              NULL
#define dsa_free                (void (*)(void *))DSA_free

/* ---------------------------------------------------------------------- */

#define rsa_decode_private_key  (b2i_of_void_fn *)ossl_b2i_RSA_after_header
#define rsa_decode_public_key   (b2i_of_void_fn *)ossl_b2i_RSA_after_header

static void rsa_adjust(void *key, struct msblob2key_ctx_st *ctx)
{
    ossl_rsa_set0_libctx(key, PROV_LIBCTX_OF(ctx->provctx));
}

#define rsa_free                        (void (*)(void *))RSA_free

/* ---------------------------------------------------------------------- */

#define IMPLEMENT_MSBLOB(KEYTYPE, keytype)                              \
    static const struct keytype_desc_st mstype##2##keytype##_desc = {   \
        EVP_PKEY_##KEYTYPE, #KEYTYPE,                                   \
        ossl_##keytype##_keymgmt_functions,                             \
        keytype##_decode_private_key,                                   \
        keytype##_decode_public_key,                                    \
        keytype##_adjust,                                               \
        keytype##_free                                                  \
    };                                                                  \
    static OSSL_FUNC_decoder_newctx_fn msblob2##keytype##_newctx;       \
    static void *msblob2##keytype##_newctx(void *provctx)               \
    {                                                                   \
        return msblob2key_newctx(provctx, &mstype##2##keytype##_desc);  \
    }                                                                   \
    const OSSL_DISPATCH                                                 \
    ossl_msblob_to_##keytype##_decoder_functions[] = {                  \
        { OSSL_FUNC_DECODER_NEWCTX,                                     \
          (void (*)(void))msblob2##keytype##_newctx },                  \
        { OSSL_FUNC_DECODER_FREECTX,                                    \
          (void (*)(void))msblob2key_freectx },                         \
        { OSSL_FUNC_DECODER_GETTABLE_PARAMS,                            \
          (void (*)(void))msblob2key_gettable_params },                 \
        { OSSL_FUNC_DECODER_GET_PARAMS,                                 \
          (void (*)(void))msblob2key_get_params },                      \
        { OSSL_FUNC_DECODER_DECODE,                                     \
          (void (*)(void))msblob2key_decode },                          \
        { OSSL_FUNC_DECODER_EXPORT_OBJECT,                              \
          (void (*)(void))msblob2key_export_object },                   \
        { 0, NULL }                                                     \
    }

#ifndef OPENSSL_NO_DSA
IMPLEMENT_MSBLOB(DSA, dsa);
#endif
IMPLEMENT_MSBLOB(RSA, rsa);
