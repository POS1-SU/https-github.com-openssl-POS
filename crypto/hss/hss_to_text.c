#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include "internal/packet.h"
#include "crypto/hss.h"
#include "lms_local.h"

/**
 * @brief Various functions to print out HSS related objects using a BIO.
 */

/**
 * @brief Convert a byte buffer into a textual representation.
 * There are no line breaks and each byte is written as 2 Hex characters.
 *
 * @param out The BIO object to write to.
 * @param buf The input byte buffer.
 * @param buflen The size of buf.
 * @returns 1 on success, or 0 otherwise.
 */
static int print_hex(BIO *out, const unsigned char *buf, size_t buflen)
{
    size_t i;

    for (i = 0; i < buflen; i++) {
        if (BIO_printf(out, "%02x", buf[i]) <= 0)
            return 0;
    }
    if (BIO_printf(out, "\n") <= 0)
        return 0;
    return 1;
}

/**
 * @brief Convert a byte buffer into a textual representation with a prepended label.
 * There are no line breaks and each byte is written as 2 Hex characters.
 *
 * @param out The BIO object to write to.
 * @param label A string to write to the BIO.
 * @param buf The input byte buffer to write after the label.
 * @param buflen The size of buf.
 * @returns 1 on success, or 0 otherwise.
 */
static int print_labeled_hex(BIO *out, const char *label,
                             const unsigned char *buf, size_t buflen)
{
    if (BIO_printf(out, "%s", label) <= 0)
        return 0;
    return print_hex(out, buf, buflen);
}

/**
 * @brief Convert an LMS_PARAMS object into a textual representation.
 *
 * @param out A BIO object to write to
 * @param prms A LMS_PARAMS object.
 * @returns 1 on success or 0 otherwise.
 */
int ossl_lms_params_to_text(BIO *out, const LMS_PARAMS *prms)
{
    return BIO_printf(out, "LMS type:   %u     # LM_%s_M%u_H%u\n",
                      (unsigned)prms->lms_type, prms->digestname,
                      (unsigned)prms->n, (unsigned)prms->h) > 0;
}

/**
 * @brief Convert an LM_OTS_PARAMS object into a textual representation.
 *
 * @params out A BIO object to write to
 * @params prms A LM_OTS_PARAMS object.
 * @returns 1 on success or 0 otherwise.
 */
int ossl_lm_ots_params_to_text(BIO *out, const LM_OTS_PARAMS *prms)
{
    return BIO_printf(out, "LMOTS type: %u     # LMOTS_%s_N%u_W%u (p=%u)\n",
                      (unsigned)prms->lm_ots_type, prms->digestname,
                      (unsigned)prms->n, (unsigned)prms->w,
                      (unsigned)prms->p) > 0;
}

/**
 * @brief Convert an LMS_KEY object into a textual representation.
 *
 * @params out A BIO object to write to
 * @params lmskey A LMS_KEY object representing either a public or private key
 * @params selection One of OSSL_KEYMGMT_SELECT_PUBLIC_KEY, OSSL_KEYMGMT_SELECT_KEYPAIR
 *                   or OSSL_KEYMGMT_SELECT_PRIVATE_KEY depending on what info
 *                   is required.
 * @returns 1 on success or 0 otherwise.
 */
int ossl_lms_key_to_text(BIO *out, LMS_KEY *lmskey, int selection)
{
    int ret = 0;

    if (lmskey == NULL)
        return 0;

    if (!ossl_lms_params_to_text(out, lmskey->lms_params)
            || !ossl_lm_ots_params_to_text(out, lmskey->ots_params))
        goto err;

    if ((selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0) {
        if (lmskey->pub.K != NULL
                && !print_labeled_hex(out, "K:", lmskey->pub.K,
                                      lmskey->lms_params->n))
            goto err;
    }

    if ((selection & OSSL_KEYMGMT_SELECT_KEYPAIR) != 0) {
        if (lmskey->Id != NULL
                && !print_labeled_hex(out, "I:", lmskey->Id, LMS_SIZE_I))
            goto err;
    }
    if ((selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) != 0) {
        if (lmskey->priv.seed != NULL
                && !print_labeled_hex(out, "SEED:", lmskey->priv.seed,
                                      lmskey->lms_params->n))
            goto err;
        if (BIO_printf(out, "q: %u\n", (unsigned)lmskey->q) <= 0)
            goto err;
    }

    ret = 1;
err:
    return ret;
}

/**
 * @brief Convert an LMS_SIG object into a textual representation.
 *
 * @params out A BIO object to write to
 * @params lmssig A LMS_SIG object (which is a component of a hss signature)
 * @returns 1 on success or 0 otherwise.
 */
static int lms_sig_to_text(BIO *out, LMS_SIG *lmssig)
{
    int ret = 0;
    uint32_t i;

    if (BIO_printf(out, "q: %u\n", (unsigned)lmssig->q) <= 0)
        goto err;
    if (!ossl_lm_ots_params_to_text(out, lmssig->sig.params))
        goto err;
    if (!print_labeled_hex(out, "C:", lmssig->sig.C, lmssig->sig.params->n))
        goto err;

    for (i = 0; i < lmssig->sig.params->p; i++) {
        if (BIO_printf(out, "y[%u]: ", (unsigned)i) <= 0)
            goto err;
        if (!print_hex(out, lmssig->sig.y + i * lmssig->sig.params->n,
                       lmssig->sig.params->n))
            goto err;
    }
    if (!ossl_lms_params_to_text(out, lmssig->params))
        goto err;
    for (i = 0; i < lmssig->params->h; i++) {
        if (BIO_printf(out, "path[%u]: ", (unsigned)i) <= 0)
            goto err;
        if (!print_hex(out, lmssig->paths + i * lmssig->sig.params->n,
                       lmssig->sig.params->n))
            goto err;
    }
    ret = 1;
err:
    return ret;
}

/**
 * @brief Convert a HSS signature into a textual representation.
 *        This is for debugging purposes only.
 * @params out A BIO object to write to
 * @params hsskey A HSS_KEY object containing lists of LMS signatures and keys
 * @params selection One of OSSL_KEYMGMT_SELECT_PUBLIC_KEY, OSSL_KEYMGMT_SELECT_KEYPAIR
 *                   or OSSL_KEYMGMT_SELECT_PRIVATE_KEY depending on what info
 *                   is required.
 * @returns 1 on success or 0 otherwise.
 */
int ossl_hss_sig_to_text(BIO *out, HSS_KEY *hsskey, int selection)
{
    LMS_SIG *sig;
    LMS_KEY *key;
    uint32_t i;

    if (BIO_printf(out, "\n------\nNspk: %d\n", (unsigned)(hsskey->L - 1)) <= 0)
        return 0;

    for (i = 0; i < hsskey->L; ++i) {
        sig = LMS_SIG_get(hsskey, i);
        key = LMS_KEY_get(hsskey, i);
        if (key == NULL || sig == NULL)
            return 0;
        if (i != 0) {
            /* The root HSS public key is not part of the signature */
            if (!ossl_lms_key_to_text(out, key, selection))
                return 0;
        }
        if (BIO_printf(out, "\nSig[%u]:\n", (unsigned)i) <= 0)
            return 0;
        if (!lms_sig_to_text(out, sig))
            return 0;
    }
    return 1;
}
