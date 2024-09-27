/*
 * Copyright 2023-2024 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/err.h>
#include <openssl/proverr.h>
#include <openssl/params.h>
#include <openssl/core_names.h>
#include <openssl/hss.h>
#include "internal/common.h"
#include "crypto/hss.h"
#include "lms_local.h"

/**
 * @param hsskey A HSS_KEY object that holds 'L' initialized LMS keys
 * @returns The total number of OTS signatures that can be generated
 */
static uint64_t hss_keys_total(HSS_KEY *hsskey)
{
    /*
     * Multiply (2^h) together for all tree levels
     * Where 2^h1 * 2^h2 = 2^(h1 + h2)
     */
    uint32_t height = 0;
    LMS_KEY *lmskey;
    uint32_t d, L = hsskey->L;

    for (d = 0; d < L; ++d) {
        lmskey = LMS_KEY_get(hsskey, d);
        if (lmskey == NULL)
            return 0;
        height += lmskey->lms_params->h;
    }
    /* Only allow 2^64 - 1 signatures */
    return height >= 64 ? UINT64_MAX : (uint64_t)1 << height;
}

/*
 * @returns The total number of remaining key pairs. Signatures can no longer be
 * generated if this number reaches zero. For a single level LMS tree of Height 5
 * this returns 32 (2^5) initially. For a 2 level HSS tree of heights 10 and 5
 * this returns 1024*32 (2^10 * 2^5 = 2^15) initially. The number decrements
 * after each successful sign operation.
 */
uint64_t ossl_hss_keys_remaining(const HSS_KEY *hsskey)
{
    return hsskey->remaining;
}

/*
 * @returns 1 if the root LMS_KEY exists and contains a private key
 *          or 0 otherwise.
 */
static int hss_key_is_private(const HSS_KEY *hsskey)
{
    LMS_KEY *root = LMS_KEY_get(hsskey, 0);

    return root != NULL && root->priv.data != NULL;
}

/*
 * @brief HSS key generation
 * See RFC 8554 Algorithm 7: Generating a HSS Key Pair
 *
 * @param hsskey A HSS_KEY object that will hold keys and signatures.
 *               Any existing keys or signatures will be cleared.
 * @param levels The number of levels of HSS LMS trees.
 *               This is the size of |lms_types| and |ots_types|
 * @param lms_types An array of LMS types such as OSSL_LMS_TYPE_SHA256_N32_H5
 * @param ots_types An array of OTS types such as OSSL_LM_OTS_TYPE_SHA256_N32_W1
 * @param gen_type OSSL_HSS_KEYGEN_TYPE_DETERMINISTIC or OSSL_HSS_KEYGEN_TYPE_RANDOM
 * @returns 1 on success, or 0 on failure.
 */
int ossl_hss_generate_key(HSS_KEY *hsskey, uint32_t levels,
                          uint32_t *lms_types, uint32_t *ots_types,
                          uint32_t gen_type)
{
    uint32_t i;
    LMS_SIG *sig = NULL;
    LMS_KEY *key = NULL;
    LMS_KEY *parent = NULL;

    hsskey->L = levels;
    hsskey->gen_type = gen_type;

    /* Clear any existing keys & signatures */
    if (!ossl_hss_lists_init(&hsskey->lists))
        goto err;
    /*
     * Create an active LMS tree for each level.
     * For each level this generates OTS keypairs and calculates the public key.
     */
    for (i = 0; i < levels; ++i) {
        /* Do LMS keygen, this calculates the public key T(1) for each tree */
        key = ossl_lms_key_gen(lms_types[i], ots_types[i], gen_type,
                               hsskey->libctx, hsskey->propq, parent);
        if (key == NULL)
            goto err;
        if (!LMS_KEY_add(hsskey, key))
            goto err;
        /*
         * Set up objects for the signatures for each level. This loop does not
         * calculate the signatures.
         */
        sig = ossl_lms_sig_new(gen_type);
        if (sig == NULL)
            goto err;
        sig->params = key->lms_params;
        sig->sig.params = key->ots_params;
        if (!LMS_SIG_add(hsskey, sig))
            goto err;
        sig = NULL;
        parent = key;
    }
    hsskey->index = 0;
    hsskey->remaining = hss_keys_total(hsskey);

    /*
     * For each intermediate tree except the leaf, generate a LMS signature,
     * using the private key of the tree above to sign the encoded public key.
     */
    parent = LMS_KEY_get(hsskey, 0);
    for (i = 1; i < levels; ++i) {
        key = LMS_KEY_get(hsskey, i);
        if (!ossl_lms_signature_gen(parent, key->pub.encoded, key->pub.encodedlen,
                                    LMS_SIG_get(hsskey, i - 1)))
            goto err;
        parent = key;
    }

    return 1;
err:
    ossl_lms_sig_free(sig);
    ossl_hss_lists_free(&hsskey->lists);
    return 0;
}

/*
 * @brief Duplicates an existing private key.
 * It also sets a count of how many times this new key can be used.
 * This function is used along with ossl_hss_key_advance() to split
 * a HSS key into 2 parts.
 *
 * @param src The HSS_KEY to split into 2 parts.
 * @param count The number of times this new key can be used for signing.
 * @returns The newly created HSS_KEY on success or NULL on failure.
 * ossl_hss_key_reserve() will fail if used again on the returned object.
 * @notes The caller is responsible for ensuring that count <= src->remaining.
 */
HSS_KEY *ossl_hss_key_reserve(const HSS_KEY *src, uint64_t count)
{
    HSS_KEY *dst;

    if (src->reserved)
        return NULL;
    /* Do not allow a public key to be split */
    if (!hss_key_is_private(src))
        return NULL;

    dst = OPENSSL_zalloc(sizeof(*dst));
    if (dst == NULL)
        return NULL;
    if (!CRYPTO_NEW_REF(&dst->references, 1)) {
        OPENSSL_free(dst);
        return NULL;
    }
    if (!ossl_hss_lists_copy(&dst->lists, &src->lists))
        goto err;
    dst->L = src->L;
    dst->libctx = src->libctx;
    dst->reserved = 1;
    dst->remaining = count;
    dst->index = src->index;
    return dst;
err:
    ossl_hss_key_free(dst);
    return NULL;
}
