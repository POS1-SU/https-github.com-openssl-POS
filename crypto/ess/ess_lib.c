/*
 * Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/ess.h>
#include "crypto/ess.h"
#include "crypto/x509.h"

static ESS_CERT_ID *ESS_CERT_ID_new_init(X509 *cert, int issuer_needed);
static ESS_CERT_ID_V2 *ESS_CERT_ID_V2_new_init(const EVP_MD *hash_alg,
                                               X509 *cert, int issuer_needed);

ESS_SIGNING_CERT *ossl_ess_signing_cert_new_init(X509 *signcert,
                                                 STACK_OF(X509) *certs,
                                                 int issuer_needed)
{
    ESS_CERT_ID *cid = NULL;
    ESS_SIGNING_CERT *sc;
    int i;

    if ((sc = ESS_SIGNING_CERT_new()) == NULL)
        goto err;
    if (sc->cert_ids == NULL
        && (sc->cert_ids = sk_ESS_CERT_ID_new_null()) == NULL)
        goto err;

    if ((cid = ESS_CERT_ID_new_init(signcert, issuer_needed)) == NULL
        || !sk_ESS_CERT_ID_push(sc->cert_ids, cid))
        goto err;
    for (i = 0; i < sk_X509_num(certs); ++i) {
        X509 *cert = sk_X509_value(certs, i);
        if ((cid = ESS_CERT_ID_new_init(cert, 1)) == NULL
            || !sk_ESS_CERT_ID_push(sc->cert_ids, cid))
            goto err;
    }

    return sc;
 err:
    ESS_SIGNING_CERT_free(sc);
    ESS_CERT_ID_free(cid);
    ERR_raise(ERR_LIB_ESS, ERR_R_MALLOC_FAILURE);
    return NULL;
}

static ESS_CERT_ID *ESS_CERT_ID_new_init(X509 *cert, int issuer_needed)
{
    ESS_CERT_ID *cid = NULL;
    GENERAL_NAME *name = NULL;
    unsigned char cert_sha1[SHA_DIGEST_LENGTH];

    /* Call for side-effect of computing hash and caching extensions */
    if (!ossl_x509v3_cache_extensions(cert))
        return NULL;

    if ((cid = ESS_CERT_ID_new()) == NULL)
        goto err;
    /* TODO(3.0): fetch sha1 algorithm from providers */
    if (!X509_digest(cert, EVP_sha1(), cert_sha1, NULL))
        goto err;
    if (!ASN1_OCTET_STRING_set(cid->hash, cert_sha1, SHA_DIGEST_LENGTH))
        goto err;

    /* Setting the issuer/serial if requested. */
    if (!issuer_needed)
        return cid;

    if (cid->issuer_serial == NULL
        && (cid->issuer_serial = ESS_ISSUER_SERIAL_new()) == NULL)
        goto err;
    if ((name = GENERAL_NAME_new()) == NULL)
        goto err;
    name->type = GEN_DIRNAME;
    if ((name->d.dirn = X509_NAME_dup(X509_get_issuer_name(cert))) == NULL)
        goto err;
    if (!sk_GENERAL_NAME_push(cid->issuer_serial->issuer, name))
        goto err;
    name = NULL;            /* Ownership is lost. */
    ASN1_INTEGER_free(cid->issuer_serial->serial);
    if ((cid->issuer_serial->serial =
          ASN1_INTEGER_dup(X509_get0_serialNumber(cert))) == NULL)
        goto err;

    return cid;
 err:
    GENERAL_NAME_free(name);
    ESS_CERT_ID_free(cid);
    ERR_raise(ERR_LIB_ESS, ERR_R_MALLOC_FAILURE);
    return NULL;
}

ESS_SIGNING_CERT_V2 *ossl_ess_signing_cert_v2_new_init(const EVP_MD *hash_alg,
                                                       X509 *signcert,
                                                       STACK_OF(X509) *certs,
                                                       int issuer_needed)
{
    ESS_CERT_ID_V2 *cid = NULL;
    ESS_SIGNING_CERT_V2 *sc;
    int i;

    if ((sc = ESS_SIGNING_CERT_V2_new()) == NULL)
        goto err;
    if ((cid = ESS_CERT_ID_V2_new_init(hash_alg, signcert, issuer_needed)) == NULL)
        goto err;
    if (!sk_ESS_CERT_ID_V2_push(sc->cert_ids, cid))
        goto err;
    cid = NULL;

    for (i = 0; i < sk_X509_num(certs); ++i) {
        X509 *cert = sk_X509_value(certs, i);

        if ((cid = ESS_CERT_ID_V2_new_init(hash_alg, cert, 1)) == NULL)
            goto err;
        if (!sk_ESS_CERT_ID_V2_push(sc->cert_ids, cid))
            goto err;
        cid = NULL;
    }

    return sc;
 err:
    ESS_SIGNING_CERT_V2_free(sc);
    ESS_CERT_ID_V2_free(cid);
    ERR_raise(ERR_LIB_ESS, ERR_R_MALLOC_FAILURE);
    return NULL;
}

static ESS_CERT_ID_V2 *ESS_CERT_ID_V2_new_init(const EVP_MD *hash_alg,
                                               X509 *cert, int issuer_needed)
{
    ESS_CERT_ID_V2 *cid;
    GENERAL_NAME *name = NULL;
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = sizeof(hash);
    X509_ALGOR *alg = NULL;

    memset(hash, 0, sizeof(hash));

    if ((cid = ESS_CERT_ID_V2_new()) == NULL)
        goto err;

    if (!EVP_MD_is_a(hash_alg, SN_sha256)) {
        alg = X509_ALGOR_new();
        if (alg == NULL)
            goto err;
        X509_ALGOR_set_md(alg, hash_alg);
        if (alg->algorithm == NULL)
            goto err;
        cid->hash_alg = alg;
        alg = NULL;
    } else {
        cid->hash_alg = NULL;
    }

    /* TODO(3.0): fetch sha1 algorithm from providers */
    if (!X509_digest(cert, hash_alg, hash, &hash_len))
        goto err;

    if (!ASN1_OCTET_STRING_set(cid->hash, hash, hash_len))
        goto err;

    if (!issuer_needed)
        return cid;

    if ((cid->issuer_serial = ESS_ISSUER_SERIAL_new()) == NULL)
        goto err;
    if ((name = GENERAL_NAME_new()) == NULL)
        goto err;
    name->type = GEN_DIRNAME;
    if ((name->d.dirn = X509_NAME_dup(X509_get_issuer_name(cert))) == NULL)
        goto err;
    if (!sk_GENERAL_NAME_push(cid->issuer_serial->issuer, name))
        goto err;
    name = NULL;            /* Ownership is lost. */
    ASN1_INTEGER_free(cid->issuer_serial->serial);
    cid->issuer_serial->serial = ASN1_INTEGER_dup(X509_get0_serialNumber(cert));
    if (cid->issuer_serial->serial == NULL)
        goto err;

    return cid;
 err:
    X509_ALGOR_free(alg);
    GENERAL_NAME_free(name);
    ESS_CERT_ID_V2_free(cid);
    ERR_raise(ERR_LIB_ESS, ERR_R_MALLOC_FAILURE);
    return NULL;
}

ESS_SIGNING_CERT *ossl_ess_get_signing_cert(const PKCS7_SIGNER_INFO *si)
{
    ASN1_TYPE *attr;
    const unsigned char *p;

    attr = PKCS7_get_signed_attribute(si, NID_id_smime_aa_signingCertificate);
    if (attr == NULL)
        return NULL;
    p = attr->value.sequence->data;
    return d2i_ESS_SIGNING_CERT(NULL, &p, attr->value.sequence->length);
}

ESS_SIGNING_CERT_V2 *ossl_ess_get_signing_cert_v2(const PKCS7_SIGNER_INFO *si)
{
    ASN1_TYPE *attr;
    const unsigned char *p;

    attr = PKCS7_get_signed_attribute(si, NID_id_smime_aa_signingCertificateV2);
    if (attr == NULL)
        return NULL;
    p = attr->value.sequence->data;
    return d2i_ESS_SIGNING_CERT_V2(NULL, &p, attr->value.sequence->length);
}

int ossl_ess_signing_cert_add(PKCS7_SIGNER_INFO *si, ESS_SIGNING_CERT *sc)
{
    ASN1_STRING *seq = NULL;
    unsigned char *p, *pp = NULL;
    int len;

    len = i2d_ESS_SIGNING_CERT(sc, NULL);
    if (len <= 0)
        goto err;
    if ((pp = OPENSSL_malloc(len)) == NULL) {
        ERR_raise(ERR_LIB_ESS, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    p = pp;
    i2d_ESS_SIGNING_CERT(sc, &p);
    if ((seq = ASN1_STRING_new()) == NULL || !ASN1_STRING_set(seq, pp, len)) {
        ERR_raise(ERR_LIB_ESS, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    OPENSSL_free(pp);
    pp = NULL;
    return PKCS7_add_signed_attribute(si,
                                      NID_id_smime_aa_signingCertificate,
                                      V_ASN1_SEQUENCE, seq);
 err:
    ASN1_STRING_free(seq);
    OPENSSL_free(pp);

    return 0;
}

int ossl_ess_signing_cert_v2_add(PKCS7_SIGNER_INFO *si, ESS_SIGNING_CERT_V2 *sc)
{
    ASN1_STRING *seq = NULL;
    unsigned char *p, *pp = NULL;
    int len = i2d_ESS_SIGNING_CERT_V2(sc, NULL);

    if (len <= 0)
        goto err;
    if ((pp = OPENSSL_malloc(len)) == NULL) {
        ERR_raise(ERR_LIB_ESS, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    p = pp;
    i2d_ESS_SIGNING_CERT_V2(sc, &p);
    if ((seq = ASN1_STRING_new()) == NULL || !ASN1_STRING_set(seq, pp, len)) {
        ERR_raise(ERR_LIB_ESS, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    OPENSSL_free(pp);
    pp = NULL;
    return PKCS7_add_signed_attribute(si,
                                      NID_id_smime_aa_signingCertificateV2,
                                      V_ASN1_SEQUENCE, seq);
 err:
    ASN1_STRING_free(seq);
    OPENSSL_free(pp);
    return 0;
}

static int ess_issuer_serial_cmp(const ESS_ISSUER_SERIAL *is, const X509 *cert)
{
    GENERAL_NAME *issuer;

    if (is == NULL || cert == NULL || sk_GENERAL_NAME_num(is->issuer) != 1)
        return -1;

    issuer = sk_GENERAL_NAME_value(is->issuer, 0);
    if (issuer->type != GEN_DIRNAME
        || X509_NAME_cmp(issuer->d.dirn, X509_get_issuer_name(cert)) != 0)
        return -1;

    return ASN1_INTEGER_cmp(is->serial, X509_get0_serialNumber(cert));
}

/*
 * Find cert referenced by |cid| (if not NULL, else |cidv2|) in |certs|.
 * If the cid{,v2} index is 0, the cert must be in the first in |certs| list.
 * Return 0 on not found, -1 on error, else 1 + the position in |certs|.
 */
static int find(const ESS_CERT_ID *cid, const ESS_CERT_ID_V2 *cid_v2,
                int index, const STACK_OF(X509) *certs)
{
    const X509 *cert;
    const EVP_MD *md;
    unsigned char cert_digest[EVP_MAX_MD_SIZE];
    unsigned int len, cid_hash_len;
    const ESS_ISSUER_SERIAL *is;
    int i;

    if (cid == NULL && cid_v2 == NULL) {
        ERR_raise(ERR_LIB_ESS, ERR_R_PASSED_INVALID_ARGUMENT);
        return -1;
    }

    /* Look for cert with cid in the certs. */
    for (i = 0; i < sk_X509_num(certs); ++i) {
        cert = sk_X509_value(certs, i);

        if (cid != NULL)
            md = EVP_sha1();
        else
            md = cid_v2->hash_alg == NULL ? EVP_sha256() :
                EVP_get_digestbyobj(cid_v2->hash_alg->algorithm);
        if (md == NULL) {
            ERR_raise(ERR_LIB_ESS, ESS_R_ESS_DIGEST_ALG_UNKNOWN);
            return -1;
        }

        cid_hash_len = cid != NULL ? cid->hash->length : cid_v2->hash->length;
        if (!X509_digest(cert, md, cert_digest, &len)
                || cid_hash_len != len) {
            ERR_raise(ERR_LIB_ESS, ESS_R_ESS_CERT_DIGEST_ERROR);
            return -1;
        }

        if (memcmp(cid != NULL ? cid->hash->data : cid_v2->hash->data,
                   cert_digest, len) == 0) {
            is = cid != NULL ? cid->issuer_serial : cid_v2->issuer_serial;
            /* Well, it's not really required to match the serial numbers. */
            if (is == NULL || ess_issuer_serial_cmp(is, cert) == 0) {
                if ((i == 0) == (index == 0))
                    return i + 1;
                ERR_raise(ERR_LIB_ESS, ESS_R_ESS_CERT_ID_WRONG_ORDER);
                return -1;
            }
        }
    }

    ERR_raise(ERR_LIB_ESS, ESS_R_ESS_CERT_ID_NOT_FOUND);
    return 0;
}

/*
 * If ESSCertID and/or ESSCertIDv2 exist, which must be non-empty if given,
 * check if their first ID entry matches the signer cert first in chain
 * and each further ID entry matches any further cert in the chain.
 */
int ossl_ess_check_signing_certs(const ESS_SIGNING_CERT *ss,
                                 const ESS_SIGNING_CERT_V2 *ssv2,
                                 const STACK_OF(X509) *chain,
                                 int require_signing_cert)
{
    int n_v1 = ss == NULL ? -1 : sk_ESS_CERT_ID_num(ss->cert_ids);
    int n_v2 = ssv2 == NULL ? -1 : sk_ESS_CERT_ID_V2_num(ssv2->cert_ids);
    int i;

    if (require_signing_cert && ss == NULL && ssv2 == NULL) {
        ERR_raise(ERR_LIB_CMS, ESS_R_MISSING_SIGNING_CERTIFICATE_ATTRIBUTE);
        return 0;
    }
    if (n_v1 == 0 || n_v2 == 0) {
        ERR_raise(ERR_LIB_ESS, ESS_R_EMPTY_ESS_CERT_ID_LIST);
        return 0;
    }
    /* If both ss and ssv2 exist, as required evaluate them independently. */
    for (i = 0; i < n_v1; i++)
        if (find(sk_ESS_CERT_ID_value(ss->cert_ids, i), NULL, i, chain) <= 0)
            return 0;
    for (i = 0; i < n_v2; i++)
        if (find(NULL, sk_ESS_CERT_ID_V2_value(ssv2->cert_ids, i), i, chain) <= 0)
            return 0;
    return 1;
}
