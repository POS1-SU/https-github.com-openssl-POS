/*
 * Copyright 2019 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/core_numbers.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/types.h>
#include <openssl/params.h>
#include "prov/bio.h"
#include "prov/implementations.h"
#include "prov/providercommonerr.h"
#include "serializer_local.h"

static OSSL_OP_serializer_newctx_fn rsa_pub_newctx;
static OSSL_OP_serializer_serialize_data_fn rsa_pub_der_data;
static OSSL_OP_serializer_serialize_object_fn rsa_pub_der;
static OSSL_OP_serializer_serialize_data_fn rsa_pub_pem_data;
static OSSL_OP_serializer_serialize_object_fn rsa_pub_pem;

static OSSL_OP_serializer_serialize_data_fn rsa_pub_print_data;
static OSSL_OP_serializer_serialize_object_fn rsa_pub_print;

/* Public key : context */

/*
 * There's no specific implementation context, so we use the provider context
 */
static void *rsa_pub_newctx(void *provctx)
{
    return provctx;
}

/* Public key : DER */
static int rsa_pub_der_data(void *ctx, const OSSL_PARAM params[], BIO *out,
                            OSSL_PASSPHRASE_CALLBACK *cb, void *cbarg)
{
    OSSL_OP_keymgmt_importkey_fn *rsa_importkey =
        ossl_prov_get_rsa_importkey();
    int ok = 0;

    if (rsa_importkey != NULL) {
        RSA *rsa = rsa_importkey(ctx, params); /* ctx == provctx */

        ok = rsa_pub_der(ctx, rsa, out, cb, cbarg);
        RSA_free(rsa);
    }
    return ok;
}

static int rsa_pub_der(void *ctx, void *rsa, BIO *out,
                       OSSL_PASSPHRASE_CALLBACK *cb, void *cbarg)
{
    return i2d_RSA_PUBKEY_bio(out, rsa);
}

/* Public key : PEM */
static int rsa_pub_pem_data(void *ctx, const OSSL_PARAM params[], BIO *out,
                            OSSL_PASSPHRASE_CALLBACK *cb, void *cbarg)
{
    OSSL_OP_keymgmt_importkey_fn *rsa_importkey =
        ossl_prov_get_rsa_importkey();
    int ok = 0;

    if (rsa_importkey != NULL) {
        RSA *rsa = rsa_importkey(ctx, params); /* ctx == provctx */

        ok = rsa_pub_pem(ctx, rsa, out, cb, cbarg);
        RSA_free(rsa);
    }
    return ok;
}

static int rsa_pub_pem(void *ctx, void *rsa, BIO *out,
                       OSSL_PASSPHRASE_CALLBACK *cb, void *cbarg)
{
    return PEM_write_bio_RSA_PUBKEY(out, rsa);
}

static int rsa_pub_print_data(void *ctx, const OSSL_PARAM params[], BIO *out,
                              OSSL_PASSPHRASE_CALLBACK *cb, void *cbarg)
{
    OSSL_OP_keymgmt_importkey_fn *rsa_importkey =
        ossl_prov_get_rsa_importkey();
    int ok = 0;

    if (rsa_importkey != NULL) {
        RSA *rsa = rsa_importkey(ctx, params); /* ctx == provctx */

        ok = rsa_pub_print(ctx, rsa, out, cb, cbarg);
        RSA_free(rsa);
    }
    return ok;
}

static int rsa_pub_print(void *ctx, void *rsa, BIO *out,
                         OSSL_PASSPHRASE_CALLBACK *cb, void *cbarg)
{
    return ossl_prov_print_rsa(out, rsa, 0);
}

const OSSL_DISPATCH rsa_pub_der_serializer_functions[] = {
    { OSSL_FUNC_SERIALIZER_NEWCTX, (void (*)(void))rsa_pub_newctx },
    { OSSL_FUNC_SERIALIZER_SERIALIZE_DATA, (void (*)(void))rsa_pub_der_data },
    { OSSL_FUNC_SERIALIZER_SERIALIZE_OBJECT, (void (*)(void))rsa_pub_der },
    { 0, NULL }
};

const OSSL_DISPATCH rsa_pub_pem_serializer_functions[] = {
    { OSSL_FUNC_SERIALIZER_NEWCTX, (void (*)(void))rsa_pub_newctx },
    { OSSL_FUNC_SERIALIZER_SERIALIZE_DATA, (void (*)(void))rsa_pub_pem_data },
    { OSSL_FUNC_SERIALIZER_SERIALIZE_OBJECT, (void (*)(void))rsa_pub_pem },
    { 0, NULL }
};

const OSSL_DISPATCH rsa_pub_text_serializer_functions[] = {
    { OSSL_FUNC_SERIALIZER_NEWCTX, (void (*)(void))rsa_pub_newctx },
    { OSSL_FUNC_SERIALIZER_SERIALIZE_OBJECT, (void (*)(void))rsa_pub_print },
    { OSSL_FUNC_SERIALIZER_SERIALIZE_DATA,
      (void (*)(void))rsa_pub_print_data },
    { 0, NULL }
};
