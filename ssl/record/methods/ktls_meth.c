/*
 * Copyright 2018-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/rand.h>
#include "../../ssl_local.h"
#include "../record_local.h"
#include "recmethod_local.h"
#include "internal/ktls.h"

#ifndef OPENSSL_NO_KTLS_RX
 /*
  * Count the number of records that were not processed yet from record boundary.
  *
  * This function assumes that there are only fully formed records read in the
  * record layer. If read_ahead is enabled, then this might be false and this
  * function will fail.
  */
static int count_unprocessed_records(SSL_CONNECTION *s)
{
    SSL3_BUFFER *rbuf = s->rrlmethod->get0_rbuf(s->rrl);
    PACKET pkt, subpkt;
    int count = 0;

    if (!PACKET_buf_init(&pkt, rbuf->buf + rbuf->offset, rbuf->left))
        return -1;

    while (PACKET_remaining(&pkt) > 0) {
        /* Skip record type and version */
        if (!PACKET_forward(&pkt, 3))
            return -1;

        /* Read until next record */
        if (!PACKET_get_length_prefixed_2(&pkt, &subpkt))
            return -1;

        count += 1;
    }

    return count;
}

/*
 * The kernel cannot offload receive if a partial TLS record has been read.
 * Check the read buffer for unprocessed records.  If the buffer contains a
 * partial record, fail and return 0.  Otherwise, update the sequence
 * number at *rec_seq for the count of unprocessed records and return 1.
 */
static int check_rx_read_ahead(SSL_CONNECTION *s, unsigned char *rec_seq)
{
    int bit, count_unprocessed;

    count_unprocessed = count_unprocessed_records(s);
    if (count_unprocessed < 0)
        return 0;

    /* increment the crypto_info record sequence */
    while (count_unprocessed) {
        for (bit = 7; bit >= 0; bit--) { /* increment */
            ++rec_seq[bit];
            if (rec_seq[bit] != 0)
                break;
        }
        count_unprocessed--;

    }

    return 1;
}
#endif

#if defined(__FreeBSD__)
# include "crypto/cryptodev.h"

/*
 * TODO(RECLAYER): This is essentially a copy of ktls_int_check_supported_cipher
 * but using an SSL object instead of an OSSL_RECORD_LAYER object. Once
 * everything has been moved to the reocrd layer this can be deleted
 */
int ktls_check_supported_cipher(const SSL_CONNECTION *s, const EVP_CIPHER *c,
                                const EVP_MD *md, size_t taglen)
{

    switch (s->version) {
    case TLS1_VERSION:
    case TLS1_1_VERSION:
    case TLS1_2_VERSION:
    case TLS1_3_VERSION:
        break;
    default:
        return 0;
    }

    if (EVP_CIPHER_is_a(c, "AES-128-GCM")
        || EVP_CIPHER_is_a(c, "AES-256-GCM")
# ifdef OPENSSL_KTLS_CHACHA20_POLY1305
        || EVP_CIPHER_is_a(c, "CHACHA20-POLY1305")
# endif
       )
       return 1;

    if (!EVP_CIPHER_is_a(c, "AES-128-CBC")
            && !EVP_CIPHER_is_a(c, "AES-256-CBC"))
        return 0;

    if (s->ext.use_etm)
        return 0;

    if (md == NULL
            || EVP_MD_is_a(md, "SHA1")
            || EVP_MD_is_a(md, "SHA2-256")
            || EVP_MD_is_a(md, "SHA2-384"))
        return 1;

    return 0;
}

/*-
 * Check if a given cipher is supported by the KTLS interface.
 * The kernel might still fail the setsockopt() if no suitable
 * provider is found, but this checks if the socket option
 * supports the cipher suite used at all.
 */
static int ktls_int_check_supported_cipher(OSSL_RECORD_LAYER *rl,
                                           const EVP_CIPHER *c,
                                           const EVP_MD *md,
                                           size_t taglen)
{
    switch (rl->version) {
    case TLS1_VERSION:
    case TLS1_1_VERSION:
    case TLS1_2_VERSION:
    case TLS1_3_VERSION:
        break;
    default:
        return 0;
    }

    if (EVP_CIPHER_is_a(c, "AES-128-GCM")
        || EVP_CIPHER_is_a(c, "AES-256-GCM")
# ifdef OPENSSL_KTLS_CHACHA20_POLY1305
        || EVP_CIPHER_is_a(c, "CHACHA20-POLY1305")
# endif
       )
       return 1;

    if (!EVP_CIPHER_is_a(c, "AES-128-CBC")
            && !EVP_CIPHER_is_a(c, "AES-256-CBC"))
        return 0;

    if (rl->use_etm)
        return 0;

    if (md == NULL
            || EVP_MD_is_a(md, "SHA1")
            || EVP_MD_is_a(md, "SHA2-256")
            || EVP_MD_is_a(md, "SHA2-384"))
        return 1;

    return 0;
}

/* Function to configure kernel TLS structure */
int ktls_configure_crypto(SSL_CONNECTION *s, const EVP_CIPHER *c,
                          void *rl_sequence, ktls_crypto_info_t *crypto_info,
                          int is_tx, unsigned char *iv, size_t ivlen,
                          unsigned char *key, size_t keylen,
                          unsigned char *mac_key, size_t mac_secret_size)
{
    memset(crypto_info, 0, sizeof(*crypto_info));
    switch (s->s3.tmp.new_cipher->algorithm_enc) {
    case SSL_AES128GCM:
    case SSL_AES256GCM:
        crypto_info->cipher_algorithm = CRYPTO_AES_NIST_GCM_16;
        crypto_info->iv_len = ivlen;
        break;
# ifdef OPENSSL_KTLS_CHACHA20_POLY1305
    case SSL_CHACHA20POLY1305:
        crypto_info->cipher_algorithm = CRYPTO_CHACHA20_POLY1305;
        crypto_info->iv_len = ivlen;
        break;
# endif
    case SSL_AES128:
    case SSL_AES256:
        switch (s->s3.tmp.new_cipher->algorithm_mac) {
        case SSL_SHA1:
            crypto_info->auth_algorithm = CRYPTO_SHA1_HMAC;
            break;
        case SSL_SHA256:
            crypto_info->auth_algorithm = CRYPTO_SHA2_256_HMAC;
            break;
        case SSL_SHA384:
            crypto_info->auth_algorithm = CRYPTO_SHA2_384_HMAC;
            break;
        default:
            return 0;
        }
        crypto_info->cipher_algorithm = CRYPTO_AES_CBC;
        crypto_info->iv_len = ivlen;
        crypto_info->auth_key = mac_key;
        crypto_info->auth_key_len = mac_secret_size;
        break;
    default:
        return 0;
    }
    crypto_info->cipher_key = key;
    crypto_info->cipher_key_len = keylen;
    crypto_info->iv = iv;
    crypto_info->tls_vmajor = (s->version >> 8) & 0x000000ff;
    crypto_info->tls_vminor = (s->version & 0x000000ff);
# ifdef TCP_RXTLS_ENABLE
    memcpy(crypto_info->rec_seq, rl_sequence, sizeof(crypto_info->rec_seq));
    if (!is_tx && !check_rx_read_ahead(s, crypto_info->rec_seq))
        return 0;
# else
    if (!is_tx)
        return 0;
# endif
    return 1;
};

#endif                         /* __FreeBSD__ */

#if defined(OPENSSL_SYS_LINUX)

/*
 * TODO(RECLAYER): This is essentially a copy of ktls_int_check_supported_cipher
 * but using an SSL object instead of an OSSL_RECORD_LAYER object. Once
 * everything has been moved to the reocrd layer this can be deleted
 */
int ktls_check_supported_cipher(const SSL_CONNECTION *s, const EVP_CIPHER *c,
                                const EVP_MD *md, size_t taglen)
{
    switch (s->version) {
    case TLS1_2_VERSION:
    case TLS1_3_VERSION:
        break;
    default:
        return 0;
    }

    /* check that cipher is AES_GCM_128, AES_GCM_256, AES_CCM_128
     * or Chacha20-Poly1305
     */
# ifdef OPENSSL_KTLS_AES_CCM_128
    if (EVP_CIPHER_is_a(c, "AES-128-CCM")) {
        if (s->version == TLS_1_3_VERSION /* broken on 5.x kernels */
            || taglen != EVP_CCM_TLS_TAG_LEN)
            return 0;
        return 1;
    } else
# endif
    if (0
# ifdef OPENSSL_KTLS_AES_GCM_128
        || EVP_CIPHER_is_a(c, "AES-128-GCM")
# endif
# ifdef OPENSSL_KTLS_AES_GCM_256
        || EVP_CIPHER_is_a(c, "AES-256-GCM")
# endif
# ifdef OPENSSL_KTLS_CHACHA20_POLY1305
        || EVP_CIPHER_is_a(c, "ChaCha20-Poly1305")
# endif
        ) {
        return 1;
    }
    return 0;
}

/* Function to check supported ciphers in Linux */
static int ktls_int_check_supported_cipher(OSSL_RECORD_LAYER *rl,
                                           const EVP_CIPHER *c,
                                           const EVP_MD *md,
                                           size_t taglen)
{
    switch (rl->version) {
    case TLS1_2_VERSION:
    case TLS1_3_VERSION:
        break;
    default:
        return 0;
    }

    /* check that cipher is AES_GCM_128, AES_GCM_256, AES_CCM_128
     * or Chacha20-Poly1305
     */
# ifdef OPENSSL_KTLS_AES_CCM_128
    if (EVP_CIPHER_is_a(c, "AES-128-CCM")) {
        if (rl->version == TLS_1_3_VERSION /* broken on 5.x kernels */
            || taglen != EVP_CCM_TLS_TAG_LEN)
            return 0;
        return 1;
    } else
# endif
    if (0
# ifdef OPENSSL_KTLS_AES_GCM_128
        || EVP_CIPHER_is_a(c, "AES-128-GCM")
# endif
# ifdef OPENSSL_KTLS_AES_GCM_256
        || EVP_CIPHER_is_a(c, "AES-256-GCM")
# endif
# ifdef OPENSSL_KTLS_CHACHA20_POLY1305
        || EVP_CIPHER_is_a(c, "ChaCha20-Poly1305")
# endif
        ) {
        return 1;
    }
    return 0;
}

/* Function to configure kernel TLS structure */
int ktls_configure_crypto(SSL_CONNECTION *s, const EVP_CIPHER *c,
                          void *rl_sequence, ktls_crypto_info_t *crypto_info,
                          int is_tx, unsigned char *iv, size_t ivlen,
                          unsigned char *key, size_t keylen,
                          unsigned char *mac_key, size_t mac_secret_size)
{
    unsigned char geniv[EVP_GCM_TLS_EXPLICIT_IV_LEN];
    unsigned char *eiv = NULL;
    SSL_CTX *sctx = SSL_CONNECTION_GET_CTX(s);

# ifdef OPENSSL_NO_KTLS_RX
    if (!is_tx)
        return 0;
# endif

    if (EVP_CIPHER_get_mode(c) == EVP_CIPH_GCM_MODE
            || EVP_CIPHER_get_mode(c) == EVP_CIPH_CCM_MODE) {
        if (!ossl_assert(EVP_GCM_TLS_FIXED_IV_LEN == EVP_CCM_TLS_FIXED_IV_LEN)
                || !ossl_assert(EVP_GCM_TLS_EXPLICIT_IV_LEN
                                == EVP_CCM_TLS_EXPLICIT_IV_LEN))
            return 0;
        if (s->version == TLS1_2_VERSION) {
            if (!ossl_assert(ivlen == EVP_GCM_TLS_FIXED_IV_LEN))
                return 0;
            if (is_tx) {
                if (RAND_bytes_ex(sctx->libctx, geniv,
                                EVP_GCM_TLS_EXPLICIT_IV_LEN, 0) <= 0)
                    return 0;
            } else {
                memset(geniv, 0, EVP_GCM_TLS_EXPLICIT_IV_LEN);
            }
            eiv = geniv;
        } else {
            if (!ossl_assert(ivlen == EVP_GCM_TLS_FIXED_IV_LEN
                                      + EVP_GCM_TLS_EXPLICIT_IV_LEN))
                return 0;
            eiv = iv + TLS_CIPHER_AES_GCM_128_SALT_SIZE;
        }
    }

    memset(crypto_info, 0, sizeof(*crypto_info));
    switch (EVP_CIPHER_get_nid(c))
    {
# ifdef OPENSSL_KTLS_AES_GCM_128
    case NID_aes_128_gcm:
        if (!ossl_assert(TLS_CIPHER_AES_GCM_128_SALT_SIZE == EVP_GCM_TLS_FIXED_IV_LEN)
                || !ossl_assert(TLS_CIPHER_AES_GCM_128_IV_SIZE == EVP_GCM_TLS_EXPLICIT_IV_LEN))
            return 0;
        crypto_info->gcm128.info.cipher_type = TLS_CIPHER_AES_GCM_128;
        crypto_info->gcm128.info.version = s->version;
        crypto_info->tls_crypto_info_len = sizeof(crypto_info->gcm128);
        memcpy(crypto_info->gcm128.iv, eiv, TLS_CIPHER_AES_GCM_128_IV_SIZE);
        memcpy(crypto_info->gcm128.salt, iv, TLS_CIPHER_AES_GCM_128_SALT_SIZE);
        memcpy(crypto_info->gcm128.key, key, keylen);
        memcpy(crypto_info->gcm128.rec_seq, rl_sequence,
               TLS_CIPHER_AES_GCM_128_REC_SEQ_SIZE);
        if (!is_tx && !check_rx_read_ahead(s, crypto_info->gcm128.rec_seq))
            return 0;
        return 1;
# endif
# ifdef OPENSSL_KTLS_AES_GCM_256
    case NID_aes_256_gcm:
        if (!ossl_assert(TLS_CIPHER_AES_GCM_256_SALT_SIZE == EVP_GCM_TLS_FIXED_IV_LEN)
                || !ossl_assert(TLS_CIPHER_AES_GCM_256_IV_SIZE == EVP_GCM_TLS_EXPLICIT_IV_LEN))
            return 0;
        crypto_info->gcm256.info.cipher_type = TLS_CIPHER_AES_GCM_256;
        crypto_info->gcm256.info.version = s->version;
        crypto_info->tls_crypto_info_len = sizeof(crypto_info->gcm256);
        memcpy(crypto_info->gcm256.iv, eiv, TLS_CIPHER_AES_GCM_256_IV_SIZE);
        memcpy(crypto_info->gcm256.salt, iv, TLS_CIPHER_AES_GCM_256_SALT_SIZE);
        memcpy(crypto_info->gcm256.key, key, keylen);
        memcpy(crypto_info->gcm256.rec_seq, rl_sequence,
               TLS_CIPHER_AES_GCM_256_REC_SEQ_SIZE);
        if (!is_tx && !check_rx_read_ahead(s, crypto_info->gcm256.rec_seq))
            return 0;

        return 1;
# endif
# ifdef OPENSSL_KTLS_AES_CCM_128
    case NID_aes_128_ccm:
        if (!ossl_assert(TLS_CIPHER_AES_CCM_128_SALT_SIZE == EVP_CCM_TLS_FIXED_IV_LEN)
                || !ossl_assert(TLS_CIPHER_AES_CCM_128_IV_SIZE == EVP_CCM_TLS_EXPLICIT_IV_LEN))
            return 0;
        crypto_info->ccm128.info.cipher_type = TLS_CIPHER_AES_CCM_128;
        crypto_info->ccm128.info.version = s->version;
        crypto_info->tls_crypto_info_len = sizeof(crypto_info->ccm128);
        memcpy(crypto_info->ccm128.iv, eiv, TLS_CIPHER_AES_CCM_128_IV_SIZE);
        memcpy(crypto_info->ccm128.salt, iv, TLS_CIPHER_AES_CCM_128_SALT_SIZE);
        memcpy(crypto_info->ccm128.key, key, keylen);
        memcpy(crypto_info->ccm128.rec_seq, rl_sequence,
               TLS_CIPHER_AES_CCM_128_REC_SEQ_SIZE);
        if (!is_tx && !check_rx_read_ahead(s, crypto_info->ccm128.rec_seq))
            return 0;
        return 1;
# endif
# ifdef OPENSSL_KTLS_CHACHA20_POLY1305
    case NID_chacha20_poly1305:
        if (!ossl_assert(ivlen == TLS_CIPHER_CHACHA20_POLY1305_IV_SIZE))
            return 0;
        crypto_info->chacha20poly1305.info.cipher_type = TLS_CIPHER_CHACHA20_POLY1305;
        crypto_info->chacha20poly1305.info.version = s->version;
        crypto_info->tls_crypto_info_len = sizeof(crypto_info->chacha20poly1305);
        memcpy(crypto_info->chacha20poly1305.iv, iv, ivlen);
        memcpy(crypto_info->chacha20poly1305.key, key, keylen);
        memcpy(crypto_info->chacha20poly1305.rec_seq, rl_sequence,
               TLS_CIPHER_CHACHA20_POLY1305_REC_SEQ_SIZE);
        if (!is_tx
                && !check_rx_read_ahead(s,
                                        crypto_info->chacha20poly1305.rec_seq))
            return 0;
        return 1;
# endif
    default:
        return 0;
    }

}

#endif /* OPENSSL_SYS_LINUX */

/* TODO(RECLAYER): Handle OPENSSL_NO_COMP */
static int ktls_set_crypto_state(OSSL_RECORD_LAYER *rl, int level,
                                 unsigned char *key, size_t keylen,
                                 unsigned char *iv, size_t ivlen,
                                 unsigned char *mackey, size_t mackeylen,
                                 const EVP_CIPHER *ciph,
                                 size_t taglen,
                                 /* TODO(RECLAYER): This probably should not be an int */
                                 int mactype,
                                 const EVP_MD *md,
                                 const SSL_COMP *comp,
                                 /* TODO(RECLAYER): Remove me */
                                 SSL_CONNECTION *s)
{
    void *rl_sequence;
    ktls_crypto_info_t crypto_info;

    /*
     * Check if we are suitable for KTLS. If not suitable we return
     * OSSL_RECORD_RETURN_NON_FATAL_ERR so that other record layers can be tried
     * instead
     */

    if (comp != NULL)
        return OSSL_RECORD_RETURN_NON_FATAL_ERR;

    /* ktls supports only the maximum fragment size */
    if (ssl_get_max_send_fragment(s) != SSL3_RT_MAX_PLAIN_LENGTH)
        return OSSL_RECORD_RETURN_NON_FATAL_ERR;

    /* check that cipher is supported */
    if (!ktls_int_check_supported_cipher(rl, ciph, md, taglen))
        return OSSL_RECORD_RETURN_NON_FATAL_ERR;

    /*
     * TODO(RECLAYER): For the write side we need to add a check for
     * use of s->record_padding_cb
     */

    /* All future data will get encrypted by ktls. Flush the BIO or skip ktls */
    if (rl->direction == OSSL_RECORD_DIRECTION_WRITE) {
       if (BIO_flush(rl->bio) <= 0)
           return OSSL_RECORD_RETURN_NON_FATAL_ERR;
    }

    if (rl->direction == OSSL_RECORD_DIRECTION_WRITE)
        rl_sequence = RECORD_LAYER_get_write_sequence(&s->rlayer);
    else
        rl_sequence = RECORD_LAYER_get_read_sequence(&s->rlayer);

    if (!ktls_configure_crypto(s, ciph, rl_sequence, &crypto_info,
                               rl->direction == OSSL_RECORD_DIRECTION_WRITE,
                               iv, ivlen, key, keylen, mackey, mackeylen))
       return OSSL_RECORD_RETURN_NON_FATAL_ERR;

    if (!BIO_set_ktls(rl->bio, &crypto_info, rl->direction))
        return OSSL_RECORD_RETURN_NON_FATAL_ERR;

    return OSSL_RECORD_RETURN_SUCCESS;
}

static int ktls_read_n(OSSL_RECORD_LAYER *rl, size_t n, size_t max, int extend,
                       int clearold, size_t *readbytes)
{
    int ret;

    ret = tls_default_read_n(rl, n, max, extend, clearold, readbytes);

    if (ret < OSSL_RECORD_RETURN_RETRY) {
        switch (errno) {
        case EBADMSG:
            RLAYERfatal(rl, SSL_AD_BAD_RECORD_MAC,
                        SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC);
            break;
        case EMSGSIZE:
            RLAYERfatal(rl, SSL_AD_RECORD_OVERFLOW,
                        SSL_R_PACKET_LENGTH_TOO_LONG);
            break;
        case EINVAL:
            RLAYERfatal(rl, SSL_AD_PROTOCOL_VERSION,
                        SSL_R_WRONG_VERSION_NUMBER);
            break;
        default:
            break;
        }
    }

    return ret;
}

static int ktls_cipher(OSSL_RECORD_LAYER *rl, SSL3_RECORD *inrecs, size_t n_recs,
                       int sending, SSL_MAC_BUF *mac, size_t macsize,
                       /* TODO(RECLAYER): Remove me */ SSL_CONNECTION *s)
{
    return 1;
}

static int ktls_validate_record_header(OSSL_RECORD_LAYER *rl, SSL3_RECORD *rec)
{
    if (rec->rec_version != TLS1_2_VERSION) {
        RLAYERfatal(rl, SSL_AD_DECODE_ERROR, SSL_R_WRONG_VERSION_NUMBER);
        return 0;
    }

    return 1;
}

static int ktls_post_process_record(OSSL_RECORD_LAYER *rl, SSL3_RECORD *rec,
                                    SSL_CONNECTION *s)
{
    if (rl->version == TLS1_3_VERSION)
        return tls13_common_post_process_record(rl, rec, s);

    return 1;
}

static struct record_functions_st ossl_ktls_funcs = {
    ktls_set_crypto_state,
    ktls_read_n,
    ktls_cipher,
    NULL,
    tls_default_set_protocol_version,
    ktls_validate_record_header,
    ktls_post_process_record
};

static int
ktls_new_record_layer(OSSL_LIB_CTX *libctx, const char *propq, int vers,
                      int role, int direction, int level, unsigned char *key,
                      size_t keylen, unsigned char *iv, size_t ivlen,
                      unsigned char *mackey, size_t mackeylen,
                      const EVP_CIPHER *ciph, size_t taglen,
                      /* TODO(RECLAYER): This probably should not be an int */
                      int mactype,
                      const EVP_MD *md, const SSL_COMP *comp, BIO *prev,
                      BIO *transport, BIO *next, BIO_ADDR *local, BIO_ADDR *peer,
                      const OSSL_PARAM *settings, const OSSL_PARAM *options,
                      OSSL_RECORD_LAYER **retrl,
                      /* TODO(RECLAYER): Remove me */
                      SSL_CONNECTION *s)
{
    int ret;

    ret = tls_int_new_record_layer(libctx, propq, vers, role, direction, level,
                                   key, keylen, iv, ivlen, mackey, mackeylen,
                                   ciph, taglen, mactype, md, comp, prev,
                                   transport, next, local, peer, settings,
                                   options, retrl, s);

    if (ret != OSSL_RECORD_RETURN_SUCCESS)
        return ret;

    (*retrl)->funcs = &ossl_ktls_funcs;

    ret = (*retrl)->funcs->set_crypto_state(*retrl, level, key, keylen, iv,
                                            ivlen, mackey, mackeylen, ciph,
                                            taglen, mactype, md, comp, s);

    if (ret != OSSL_RECORD_RETURN_SUCCESS) {
        OPENSSL_free(*retrl);
        *retrl = NULL;
    } else {
        /*
         * With KTLS we always try and read as much as possible and fill the
         * buffer
         */
        (*retrl)->read_ahead = 1;
    }
    return ret;
}

const OSSL_RECORD_METHOD ossl_ktls_record_method = {
    ktls_new_record_layer,
    tls_free,
    tls_reset,
    tls_unprocessed_read_pending,
    tls_processed_read_pending,
    tls_app_data_pending,
    tls_write_pending,
    tls_get_max_record_len,
    tls_get_max_records,
    tls_write_records,
    tls_retry_write_records,
    tls_read_record,
    tls_release_record,
    tls_get_alert_code,
    tls_set1_bio,
    tls_set_protocol_version,
    tls_set_plain_alerts,
    tls_set_first_handshake,

    /*
     * TODO(RECLAYER): Remove these. These function pointers are temporary hacks
     * during the record layer refactoring. They need to be removed before the
     * refactor is complete.
     */
    tls_default_read_n,
    tls_get0_rbuf,
    tls_get0_packet,
    tls_set0_packet,
    tls_get_packet_length,
    tls_reset_packet_length
};
