/*
 * Crude test driver for processing the VST and MCT testvector files
 * generated by the CMVP RNGVS product.
 *
 * Note the input files are assumed to have a _very_ specific format
 * as described in the NIST document "The Random Number Generator
 * Validation System (RNGVS)", May 25, 2004.
 *
 */
#include <openssl/opensslconf.h>

#ifndef OPENSSL_FIPS
#include <stdio.h>
int main()
{
    printf("No FIPS RNG support\n");
    return 0;
}
#else

#include <openssl/bn.h>
#include <openssl/dsa.h>
#include <openssl/fips.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/fips_rand.h>
#include <openssl/x509v3.h>
#include <string.h>
#include <ctype.h>

static int parse_line(char **pkw, char **pval, char *linebuf, char *olinebuf)
	{
	char *keyword, *value, *p, *q;
	strcpy(linebuf, olinebuf);
	keyword = linebuf;
	/* Skip leading space */
	while (isspace((unsigned char)*keyword))
		keyword++;

	/* Look for = sign */
	p = strchr(linebuf, '=');

	/* If no '=' exit */
	if (!p)
		return 0;

	q = p - 1;

	/* Remove trailing space */
	while (isspace((unsigned char)*q))
		*q-- = 0;

	*p = 0;
	value = p + 1;

	/* Remove leading space from value */
	while (isspace((unsigned char)*value))
		value++;

	/* Remove trailing space from value */
	p = value + strlen(value) - 1;

	while (*p == '\n' || isspace((unsigned char)*p))
		*p-- = 0;

	*pkw = keyword;
	*pval = value;
	return 1;
	}

int hex2bin(const char *in, unsigned char *out)
    {
    int n1, n2;
    unsigned char ch;

    for (n1=0,n2=0 ; in[n1] && in[n1] != '\n' ; )
	{ /* first byte */
	if ((in[n1] >= '0') && (in[n1] <= '9'))
	    ch = in[n1++] - '0';
	else if ((in[n1] >= 'A') && (in[n1] <= 'F'))
	    ch = in[n1++] - 'A' + 10;
	else if ((in[n1] >= 'a') && (in[n1] <= 'f'))
	    ch = in[n1++] - 'a' + 10;
	else
	    return -1;
	if(!in[n1])
	    {
	    out[n2++]=ch;
	    break;
	    }
	out[n2] = ch << 4;
	/* second byte */
	if ((in[n1] >= '0') && (in[n1] <= '9'))
	    ch = in[n1++] - '0';
	else if ((in[n1] >= 'A') && (in[n1] <= 'F'))
	    ch = in[n1++] - 'A' + 10;
	else if ((in[n1] >= 'a') && (in[n1] <= 'f'))
	    ch = in[n1++] - 'a' + 10;
	else
	    return -1;
	out[n2++] |= ch;
	}
    return n2;
    }

int bin2hex(const unsigned char *in,int len,char *out)
    {
    int n1, n2;
    unsigned char ch;

    for (n1=0,n2=0 ; n1 < len ; ++n1)
	{
	ch=in[n1] >> 4;
	if (ch <= 0x09)
	    out[n2++]=ch+'0';
	else
	    out[n2++]=ch-10+'a';
	ch=in[n1] & 0x0f;
	if(ch <= 0x09)
	    out[n2++]=ch+'0';
	else
	    out[n2++]=ch-10+'a';
	}
    out[n2]='\0';
    return n2;
    }

void pv(const char *tag,const unsigned char *val,int len)
    {
    char obuf[2048];

    bin2hex(val,len,obuf);
    printf("%s = %s\n",tag,obuf);
    }

void vst()
    {
    unsigned char *key;
    unsigned char *v;
    unsigned char *dt;
    unsigned char ret[16];
    char buf[1024];
    char lbuf[1024];
    char *keyword, *value;
    long i, keylen;

    keylen = 0;

    while(fgets(buf,sizeof buf,stdin) != NULL)
	{
	fputs(buf,stdout);
	if(!strncmp(buf,"[AES 128-Key]", 13))
		keylen = 16;
	else if(!strncmp(buf,"[AES 192-Key]", 13))
		keylen = 24;
	else if(!strncmp(buf,"[AES 256-Key]", 13))
		keylen = 32;
	if (!parse_line(&keyword, &value, lbuf, buf))
		continue;
	if(!strcmp(keyword,"Key"))
	    {
	    key=string_to_hex(value,&i);
	    if (i != keylen)
		{
		fprintf(stderr, "Invalid key length, expecting %ld\n", keylen);
		return;
		}
	    }
	else if(!strcmp(keyword,"DT"))
	    {
	    dt=string_to_hex(value,&i);
	    if (i != 16)
		{
		fprintf(stderr, "Invalid DT length\n");
		return;
		}
	    }
	else if(!strcmp(keyword,"V"))
	    {
	    v=string_to_hex(value,&i);
	    if (i != 16)
		{
		fprintf(stderr, "Invalid V length\n");
		return;
		}

	    if (!key || !dt)
		{
		fprintf(stderr, "Missing key or DT\n");
		return;
		}

	    FIPS_rand_set_key(key, keylen);
	    FIPS_rand_seed(v,16);
	    FIPS_rand_set_dt(dt);
	    if (FIPS_rand_bytes(ret,16) <= 0)
		{
		fprintf(stderr, "Error getting PRNG value\n");
	        return;
	        }

	    pv("R",ret,16);
	    putc('\n',stdout);
	    }
	}
    }

void mct()
    {
    unsigned char *key;
    unsigned char *v;
    unsigned char *dt;
    unsigned char ret[16];
    char buf[1024];
    char lbuf[1024];
    char *keyword, *value;
    long i, keylen;
    int j;

    keylen = 0;

    while(fgets(buf,sizeof buf,stdin) != NULL)
	{
	fputs(buf,stdout);
	if(!strncmp(buf,"[AES 128-Key]", 13))
		keylen = 16;
	else if(!strncmp(buf,"[AES 192-Key]", 13))
		keylen = 24;
	else if(!strncmp(buf,"[AES 256-Key]", 13))
		keylen = 32;
	if (!parse_line(&keyword, &value, lbuf, buf))
		continue;
	if(!strcmp(keyword,"Key"))
	    {
	    key=string_to_hex(value,&i);
	    if (i != keylen)
		{
		fprintf(stderr, "Invalid key length, expecting %ld\n", keylen);
		return;
		}
	    }
	else if(!strcmp(keyword,"DT"))
	    {
	    dt=string_to_hex(value,&i);
	    if (i != 16)
		{
		fprintf(stderr, "Invalid DT length\n");
		return;
		}
	    }
	else if(!strcmp(keyword,"V"))
	    {
	    v=string_to_hex(value,&i);
	    if (i != 16)
		{
		fprintf(stderr, "Invalid V length\n");
		return;
		}

	    if (!key || !dt)
		{
		fprintf(stderr, "Missing key or DT\n");
		return;
		}

	    FIPS_rand_set_key(key, keylen);
	    FIPS_rand_seed(v,16);
	    for (i = 0; i < 10000; i++)
		{
		    FIPS_rand_set_dt(dt);
		    if (FIPS_rand_bytes(ret,16) <= 0)
			{
			fprintf(stderr, "Error getting PRNG value\n");
		        return;
		        }
		    /* Increment DT */
		    for (j = 15; j >= 0; j--)
			{
			dt[j]++;
			if (dt[j])
				break;
			}
		}

	    pv("R",ret,16);
	    putc('\n',stdout);
	    }
	}
    }


#if 0
void mct()
    {
    unsigned char key1[8];
    unsigned char key2[8];
    unsigned char v[8];
    unsigned char dt[8];
    unsigned char ret[8];
    char buf[1024];
    char lbuf[1024];
    char *keyword, *value;
    int n;

    BIGNUM *bn;
    BIGNUM *pbn;
    bn = BN_new();

    if (FIPS_rand_reset() && !FIPS_rand_test_mode())
	{
	fprintf(stderr, Error setting PRNG test mode\n");
	return;
	}

    while(fgets(buf,sizeof buf,stdin) != NULL)
	{
	fputs(buf,stdout);
	if (!parse_line(&keyword, &value, lbuf, buf))
		continue;
	if(!strcmp(keyword,"Key1"))
	    {
	    n=hex2bin(value,key1);
	    }
	else if(!strcmp(keyword,"Key2"))
	    {
	    n=hex2bin(value,key2);
	    }
	else if(!strcmp(keyword,"DT"))
	    {
	    n=hex2bin(value,dt);
	    }
	else if(!strcmp(keyword,"V"))
	    {
	    int iter;
	    n=hex2bin(value,v);

	    FIPS_rand_method()->cleanup();
	    FIPS_set_prng_key(key1,key2);
	    FIPS_rand_seed(v,8);
	    for (iter=0; iter < 10000; ++iter)
		{
	        FIPS_test_mode(1,dt);
		if (FIPS_rand_method()->bytes(ret,8) <= 0)
		    {
		    FIPS_test_mode(0,NULL);
		    FIPSerr(FIPS_F_FIPS_SELFTEST_RNG,FIPS_R_SELFTEST_FAILED);
		    return;
		    }
		pbn = BN_bin2bn(dt,8,bn);
		n = BN_add(bn,bn,BN_value_one());
		n = BN_bn2bin(bn,dt);
		}

	    pv("R",ret,8);
	    putc('\n',stdout);
	    }
	}
    BN_free(bn);
    }
#endif

int main(int argc,char **argv)
    {
    if(argc != 2)
	{
	fprintf(stderr,"%s [mct|vst]\n",argv[0]);
	exit(1);
	}
    if(!FIPS_mode_set(1))
	{
	ERR_print_errors(BIO_new_fp(stderr,BIO_NOCLOSE));
	exit(1);
	}
    FIPS_rand_reset();
    if (!FIPS_rand_test_mode())
	{
	fprintf(stderr, "Error setting PRNG test mode\n");
	ERR_print_errors_fp(stderr);
	exit(1);
	}
    if(!strcmp(argv[1],"mct"))
	mct();
    else if(!strcmp(argv[1],"vst"))
	vst();
    else
	{
	fprintf(stderr,"Don't know how to %s.\n",argv[1]);
	exit(1);
	}

    return 0;
    }
#endif
