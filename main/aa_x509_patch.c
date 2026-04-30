/* Linker-wrap replacement for mbedtls_x509_get_time().
 *
 * The original aasdk head-unit certificate carries UTCTime values with a
 * trailing timezone offset (`140704000000-0700`). RFC 5280 forbids that
 * format and mbedTLS rejects it with MBEDTLS_ERR_X509_INVALID_DATE.
 * Stock OpenSSL accepts it though, which is why every aasdk-based head
 * unit (openauto, headunit-revived, …) just works with gearhead.
 *
 * gearhead validates the AA cert's signature against a hard-coded Google
 * Automotive Link CA, so we can't replace the cert with our own
 * self-signed one — phone says "Communication error 7". We have to ship
 * the real aasdk cert and convince mbedTLS to read it.
 *
 * Wired up via target_link_options(... -Wl,--wrap=mbedtls_x509_get_time)
 * in main/CMakeLists.txt — the linker rewrites every internal mbedTLS
 * call to mbedtls_x509_get_time() into __wrap_mbedtls_x509_get_time()
 * (this function), and the original symbol is still reachable as
 * __real_mbedtls_x509_get_time() if needed.
 */

#include <stddef.h>
#include <stdint.h>

#include "mbedtls/asn1.h"
#include "mbedtls/error.h"
#include "mbedtls/x509.h"

static int parse2(const unsigned char *p)
{
    int d1 = p[0] - '0';
    int d2 = p[1] - '0';
    if (d1 < 0 || d1 > 9 || d2 < 0 || d2 > 9) return -1;
    return d1 * 10 + d2;
}

static int date_is_valid(const mbedtls_x509_time *t)
{
    int days_in_month;
    switch (t->mon) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12: days_in_month = 31; break;
        case 4: case 6: case 9: case 11:                          days_in_month = 30; break;
        case 2: {
            int y = t->year;
            int leap = ((y % 4) == 0 && (y % 100) != 0) || (y % 400) == 0;
            days_in_month = leap ? 29 : 28;
            break;
        }
        default: return MBEDTLS_ERR_X509_INVALID_DATE;
    }
    if (t->day < 1 || t->day > days_in_month) return MBEDTLS_ERR_X509_INVALID_DATE;
    if (t->year < 0 || t->year > 9999)        return MBEDTLS_ERR_X509_INVALID_DATE;
    if (t->hour < 0 || t->hour > 23)          return MBEDTLS_ERR_X509_INVALID_DATE;
    if (t->min  < 0 || t->min  > 59)          return MBEDTLS_ERR_X509_INVALID_DATE;
    if (t->sec  < 0 || t->sec  > 59)          return MBEDTLS_ERR_X509_INVALID_DATE;
    return 0;
}

int __wrap_mbedtls_x509_get_time(unsigned char **p, const unsigned char *end,
                                 mbedtls_x509_time *tm);

int __wrap_mbedtls_x509_get_time(unsigned char **p, const unsigned char *end,
                                 mbedtls_x509_time *tm)
{
    if (end - *p < 1) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_DATE,
                                 MBEDTLS_ERR_ASN1_OUT_OF_DATA);
    }

    unsigned char tag = **p;
    size_t year_len;
    if (tag == MBEDTLS_ASN1_UTC_TIME)         year_len = 2;
    else if (tag == MBEDTLS_ASN1_GENERALIZED_TIME) year_len = 4;
    else {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_DATE,
                                 MBEDTLS_ERR_ASN1_UNEXPECTED_TAG);
    }

    (*p)++;
    size_t len;
    int ret = mbedtls_asn1_get_len(p, end, &len);
    if (ret) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_X509_INVALID_DATE, ret);
    }

    /* Accepted lengths:
     *   year_len+10            "YYMMDDHHMMSS"        (no TZ — pre-RFC-5280)
     *   year_len+11 (last='Z') "YYMMDDHHMMSSZ"       (RFC 5280, Zulu)
     *   year_len+15 (TZ +/-)   "YYMMDDHHMMSS±HHMM"   (X.690 offset, what aasdk uses)
     */
    int has_offset = 0;
    if (len == year_len + 10) {
        /* no TZ */
    } else if (len == year_len + 11 && (*p)[len - 1] == 'Z') {
        /* Zulu */
    } else if (len == year_len + 15 &&
               ((*p)[year_len + 10] == '+' || (*p)[year_len + 10] == '-')) {
        has_offset = 1;
    } else {
        (*p) += len;  /* skip so caller can keep going */
        return MBEDTLS_ERR_X509_INVALID_DATE;
    }

    const unsigned char *date = *p;
    (*p) += len;

    /* Year */
    int y;
    if (year_len == 2) {
        y = parse2(date);
        if (y < 0) return MBEDTLS_ERR_X509_INVALID_DATE;
        y += (y < 50) ? 2000 : 1900;
    } else {
        int hi = parse2(date);
        int lo = parse2(date + 2);
        if (hi < 0 || lo < 0) return MBEDTLS_ERR_X509_INVALID_DATE;
        y = hi * 100 + lo;
    }
    tm->year = y;
    tm->mon  = parse2(date + year_len);
    tm->day  = parse2(date + year_len + 2);
    tm->hour = parse2(date + year_len + 4);
    tm->min  = parse2(date + year_len + 6);
    tm->sec  = parse2(date + year_len + 8);
    if (tm->mon < 0 || tm->day < 0 || tm->hour < 0 || tm->min < 0 || tm->sec < 0) {
        return MBEDTLS_ERR_X509_INVALID_DATE;
    }

    /* Offset is intentionally ignored — matches OpenSSL's behaviour and
     * aasdk has been doing this since 2014. The values are local-time
     * markers from JVC Kenwood that nobody actually time-checks. */
    (void)has_offset;

    return date_is_valid(tm);
}
