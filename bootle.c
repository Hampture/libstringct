#include <openssl/bn.h>
#include <openssl/ec.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bootle.h"
#include "echash.h"

EC_POINT *COMb(EC_GROUP *group, BN_CTX *bnctx, 
		BIGNUM ***x, size_t m, size_t n, BIGNUM *r)
{
	EC_POINT *A = EC_POINT_new(group);
	const EC_POINT *g = EC_GROUP_get0_generator(group);

	EC_POINT_mul(group, A, r, 0, 0, bnctx);

	size_t i, j;
	EC_POINT *gn = EC_POINT_new(group);
	EC_POINT *gnh = EC_POINT_new(group);
	BIGNUM *t = BN_new();
	unsigned char *gnbuf;
	for(i=0; i<m; i++)
	{
		for(j=0; j<n; j++)
		{
			BN_set_word(t, i*n+j+1);
			EC_POINT_mul(group, gn, 0, g, t, bnctx);
			int gnbuf_len = EC_POINT_point2buf(group, gn, 
				POINT_CONVERSION_UNCOMPRESSED, &gnbuf, bnctx);
			BIGNUM *h = BN_hash(gnbuf, gnbuf_len);
			EC_POINT_bn2point(group, h, gnh, bnctx); // TODO need ECHASH
			EC_POINT_mul(group, gnh, 0, gnh, x[i][j], bnctx); // TODO check whether gnh can be used as param
			EC_POINT_add(group, A, A, gnh, bnctx);
			BN_free(h);
			OPENSSL_free(gnbuf);
		}
	}
	EC_POINT_clear_free(gn);
	EC_POINT_clear_free(gnh);
	BN_clear_free(t);
	return A;
}

EC_POINT *COMp(EC_GROUP *group, BN_CTX *bnctx, BIGNUM ***x, size_t m, size_t n, BIGNUM *r)
{
	const EC_POINT *g = EC_GROUP_get0_generator(group);
	EC_POINT *ret = EC_POINT_new(group);
	EC_POINT_mul(group, ret, 0, g, r, bnctx);
	EC_POINT *xg = EC_POINT_new(group);
	EC_POINT *hash;
	size_t i;
	size_t j;
	for(i=0; i<m; i++)
		for(j=0; j<n; j++)
		{
			EC_POINT_mul(group, xg, 0, g, x[i][j], bnctx);
		}
	// TODO need ECHASH
	(void)hash;
	return ret;
}

int *ndecompose(int base, int n, int dexp)
{
	int *ret = OPENSSL_malloc(sizeof(int)*dexp);
	int i;
	int basepow;
	for(i=dexp-1; i>=0; i--)
	{
		basepow = (int)pow((double)base,(double)i);
		ret[i] = n/basepow;
		n-=basepow*ret[i];
	}
	return ret;
}

static BIGNUM **COEFPROD(BIGNUM **c, int clen, BIGNUM **d, int dlen)
{
	int maxlen = dlen ^ ((clen ^ dlen) & -(clen < dlen));
	int rlen = 2*maxlen-1;
	BIGNUM **ret = OPENSSL_malloc(sizeof(BIGNUM*)*rlen);
	int i;
	int j;
	BIGNUM *t = BN_new();
	for(i=0; i<rlen; i++) BN_zero(ret[i]);
	for(i=0; i<maxlen; i++)
		for(j=0; j<maxlen; j++)
		{
			
			BN_mul(t, c[i], d[i], 0);
			BN_add(ret[i+j], ret[i+j], t);
		}
	return ret;
}

static BIGNUM ***COEFS(BIGNUM ***a, int n, int m, int asterisk)
{
	int ring_size = (int)(pow((double)n, (double)m));
	int *asterisk_seq = ndecompose(n, asterisk, m);

	BIGNUM ***ret = OPENSSL_malloc(sizeof(BIGNUM**)*ring_size);
	int i,j;
	for(i=0; i<ring_size; i++)
	{
		int *kseq = ndecompose(n, i, m);
		ret[i] = OPENSSL_malloc(sizeof(BIGNUM*)*2);
		ret[i][0] = a[0][kseq[0]];
		asterisk_seq[0] == kseq[0] ? BN_one(ret[i][1]) : BN_zero(ret[i][1]);

		BIGNUM **cprodparam = OPENSSL_malloc(2*sizeof(BIGNUM*));
		for(j=1; j<m; j++)
		{
			cprodparam[0] = BN_dup(a[j][kseq[j]]);
			asterisk_seq[j] == kseq[j] ? BN_one(cprodparam[1]) : BN_zero(cprodparam[1]);
			ret[i] = COEFPROD(ret[i], m, cprodparam, 2);
		}
	}
	
	for(i=0; i<ring_size; i++)
	{
		for(j=0; j<ring_size; j++)
		{
			if(i<m) ret[i][j] = a[i][j];
		}
	}
	
	return ret;
}

struct BOOTLE_SIGMA1 *
BOOTLE_SIGMA1_new(EC_GROUP *group, BN_CTX *bnctx,
		BIGNUM ***b, size_t m, size_t n, BIGNUM *r)
{
	BIGNUM *rA = BN_new();
	BIGNUM *rC = BN_new();
	BIGNUM *rD = BN_new();

	BN_rand(rA, 32, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY);
	BN_rand(rC, 32, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY);
	BN_rand(rD, 32, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY);

	BIGNUM ***a = OPENSSL_malloc(sizeof(BIGNUM**)*m);
	size_t i = 0;
	size_t j = 0;
	for(i=0; i<m; i++)
	{
		a[i] = OPENSSL_malloc(sizeof(BIGNUM*)*n);
		for(j=1; j<n; j++)
		{
			a[i][j] = BN_new();
			BN_rand(a[i][j], 32, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY);
		}
	}

	for(i=0; i<m; i++)
	{
		a[i][0] = BN_new();
		BN_zero(a[i][0]);
		for(j=1; j<n; j++)
		{
			BN_sub(a[i][0], a[i][0], a[i][j]);
		}
	}
	EC_POINT *A = COMb(group, bnctx, a, m, n, rA);

	BIGNUM ***c = OPENSSL_malloc(sizeof(BIGNUM**)*m);
	BIGNUM ***d = OPENSSL_malloc(sizeof(BIGNUM**)*m);
	BIGNUM *t1 = BN_new();
	BN_one(t1);
	BIGNUM *t2 = BN_new();
	BN_set_word(t1, 2);
	for(i=0; i<m; i++)
	{
		c[i] = OPENSSL_malloc(sizeof(BIGNUM*)*n);
		d[i] = OPENSSL_malloc(sizeof(BIGNUM*)*n);
		for(j=0; j<n; j++)
		{
			c[i][j] = BN_new();
			d[i][j] = BN_new();

			BN_mul(t2, b[i][j], t2, bnctx);
			BN_sub(t1, t1, t2);
			BN_mul(c[i][j], a[i][j], t1, bnctx);

			BN_hex2bn(&t1, "-1");
			BN_sqr(t2, a[i][j], bnctx);

			BN_mul(d[i][j], t2, t1, bnctx);
		}
	}

	EC_POINT *C = COMb(group, bnctx, c, m, n, rC);
	EC_POINT *D = COMb(group, bnctx, d, m, n, rD);

	unsigned char *Abuf;
	unsigned char *Cbuf;
	unsigned char *Dbuf;
	size_t Alen = EC_POINT_point2buf(group, A,
				POINT_CONVERSION_UNCOMPRESSED, &Abuf, bnctx);
	size_t Clen = EC_POINT_point2buf(group, C,
				POINT_CONVERSION_UNCOMPRESSED, &Cbuf, bnctx);
	size_t Dlen = EC_POINT_point2buf(group, D,
				POINT_CONVERSION_UNCOMPRESSED, &Dbuf, bnctx);
	
	unsigned char *buf = OPENSSL_malloc(Alen + Clen + Dlen);

	memcpy(buf, Abuf, Alen);
	memcpy(buf+Alen, Cbuf, Clen);
	memcpy(buf+Alen+Clen, Dbuf, Dlen);

	BIGNUM *x = BN_hash(buf, Alen + Clen + Dlen);

	BIGNUM ***f = OPENSSL_malloc(sizeof(BIGNUM**)*m);
	for(i=0; i<m; i++)
	{
		f[i] = OPENSSL_malloc(sizeof(BIGNUM*)*n);
		for(j=0; j<n; j++)
		{
			f[i][j] = BN_new();
			BN_mul(f[i][j], b[i][j], x, bnctx);
			BN_add(f[i][j], f[i][j], a[i][j]);
		}
	}

	BIGNUM ***f_trimmed = OPENSSL_malloc(sizeof(BIGNUM**)*m);
	for(i=0; i<m; i++)
	{
		f_trimmed[i] = OPENSSL_malloc(sizeof(BIGNUM*)*n);
		for(j=1; j<n; j++)
		{
			f_trimmed[i][j-1] = BN_dup(f[i][j]);
		}
	}

	BIGNUM *zA = BN_new();
	BIGNUM *zC = BN_new();

	BN_mul(zA, r, x, bnctx);
	BN_add(zA, zA, rA);
	
	BN_mul(zC, rC, x, bnctx);
	BN_add(zC, zC, rD);

	struct BOOTLE_SIGMA1 *ret = OPENSSL_malloc(sizeof(struct BOOTLE_SIGMA1));
	
	ret->curve = group;
	ret->A = A;	
	ret->C = C;	
	ret->D = D;
	ret->trimmed_challenge = f_trimmed;
	ret->za = zA;
	ret->zc = zC;
	ret->a = a;
	ret->a_n = m;
	ret->a_m = n;
	BN_clear_free(rA);
	BN_clear_free(rC);
	BN_clear_free(rD);
	BN_clear_free(x);

	for(i=0; i<m; i++)
	{
		for(j=0; j<n; j++)
		{
			BN_clear_free(c[i][j]);
			BN_clear_free(d[i][j]);
			BN_clear_free(f[i][j]);
		}
		OPENSSL_free(c[i]);
		OPENSSL_free(d[i]);
		OPENSSL_free(f[i]);
	}
	OPENSSL_free(c);
	OPENSSL_free(d);
	OPENSSL_free(f);
	BN_free(t1);
	BN_free(t2);
	OPENSSL_free(Abuf);
	OPENSSL_free(Cbuf);
	OPENSSL_free(Dbuf);
	OPENSSL_free(buf);
	return ret;
}

struct BOOTLE_SIGMA2 *BOOTLE_SIGMA2_new(EC_GROUP *group, BN_CTX *bnctx,
		EC_POINT ***co, int asterisk, BIGNUM *r, int dbase, int dexp)
{
	int i, j;
	int ring_size = (int)pow((double)dbase, (double)dexp);
	if(ring_size<0)
	{
		fprintf(stderr, "ring size overflow, try lowering decomposition params!\n");
		return 0;
	}
	BIGNUM **u = OPENSSL_malloc(sizeof(BIGNUM*)*dexp);
	for(i=0; i<dexp; i++)
	{
		u[i] = BN_new();
		BN_rand(u[i], 32, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY);
	}

	BIGNUM *rB = BN_new();
	BN_rand(rB, 32, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY);

	int *asterisk_seq = ndecompose(dbase, asterisk, dexp);

	BIGNUM ***D = OPENSSL_malloc(sizeof(BIGNUM**)*dexp);
	for(i=0; i<dexp; i++)
	{
		D[i] = OPENSSL_malloc(sizeof(BIGNUM*)*dbase);
		for(j=0; j<dbase; j++)
		{
			D[i][j] = BN_new();
			asterisk_seq[i] == j ? BN_one(D[i][j]) : BN_zero(D[i][j]);
		}
	}

	EC_POINT *B = COMb(group, bnctx, D, dexp, dbase, rB);
	struct BOOTLE_SIGMA1 *P = BOOTLE_SIGMA1_new(group, bnctx, D, dexp, dbase, rB);

	BIGNUM ***coefs = COEFS(P->a, P->a_n, P->a_m,  asterisk);

	EC_POINT ***G = OPENSSL_malloc(sizeof(EC_POINT**)*dexp);
	const EC_POINT *g = EC_GROUP_get0_generator(group);
	
	unsigned char *one = OPENSSL_malloc(BN_num_bytes(BN_value_one()));
	BIGNUM *hashone = BN_hash(one, BN_num_bytes(BN_value_one()));
	BN_bn2bin(BN_value_one(), one);
	EC_POINT *econe =	EC_POINT_new(group);
	EC_POINT_bn2point(group, hashone, econe, bnctx);
	EC_POINT *t1 = EC_POINT_new(group);
	EC_POINT *t2 = EC_POINT_new(group);
	for(i=0; i<dexp; i++)
	{
		G[i] = OPENSSL_malloc(sizeof(EC_POINT*)*2);
		EC_POINT_mul(group, t1, 0, econe, u[i], bnctx);
		EC_POINT_add(group, G[i][0], t1, g, bnctx);
		EC_POINT_mul(group, G[i][0], 0, g, u[i], bnctx);

		for(j=0; j<ring_size; j++)
		{
			EC_POINT_mul(group, t1, 0, co[j][0], coefs[j][i], bnctx);
			EC_POINT_mul(group, t2, 0, co[j][1], coefs[j][i], bnctx);
			EC_POINT_add(group, G[i][0], G[i][0], t1, bnctx);
			EC_POINT_add(group, G[i][1], G[i][1], t2, bnctx);
		}
	}
	
	unsigned char *Pa;
	unsigned char *Pc;
	unsigned char *Pd;
	int Pa_len = EC_POINT_point2buf(group, P->A, 
				POINT_CONVERSION_UNCOMPRESSED, &Pa, bnctx);
	int Pc_len = EC_POINT_point2buf(group, P->C, 
				POINT_CONVERSION_UNCOMPRESSED, &Pc, bnctx);
	int Pd_len = EC_POINT_point2buf(group, P->D, 
				POINT_CONVERSION_UNCOMPRESSED, &Pd, bnctx);
	unsigned char *bytes = OPENSSL_malloc(Pa_len + Pc_len + Pd_len);
	memcpy(bytes, Pa, Pa_len);
	memcpy(bytes+Pa_len, Pc, Pc_len);
	memcpy(bytes+Pa_len+Pc_len, Pd, Pd_len);
	OPENSSL_free(Pa);
	OPENSSL_free(Pc);
	OPENSSL_free(Pd);
	BIGNUM *x1 = BN_hash(bytes, Pa_len + Pc_len + Pd_len);
	BIGNUM *z = BN_new();
	BIGNUM *t3 = BN_new();
	BIGNUM *t4 = BN_new();
	BN_set_word(t3, dexp);
	BN_exp(t4, x1, t3, bnctx);
	BN_mul(z, r, t4, bnctx);

	for(i=0; i<dexp; i++)
	{
		BN_set_word(t4, i);
		BN_exp(t3, x1, t4, bnctx);
		BN_mul(t3, u[i], t3, bnctx);
		BN_sub(z, z, t3);
	}
	
	struct BOOTLE_SIGMA2 *ret = OPENSSL_malloc(sizeof(struct BOOTLE_SIGMA2));
	if(!ret)
	{
		perror("memory allocation error: ");
		return 0;
	}
	ret->sig1 = P;
	ret->B = B;
	ret->G = G;
	ret->z = z;

	// cleanup
	for(i=0; i<dexp; i++)
	{
		for(j=0; j<dbase; j++)
		{
			BN_clear_free(D[i][j]);
			BN_clear_free(coefs[i][j]);
		}
		OPENSSL_free(D[i]);
		OPENSSL_free(u[i]);
		OPENSSL_free(coefs[i]);
	}
	EC_POINT_clear_free(econe);
	EC_POINT_clear_free(t1);
	EC_POINT_clear_free(t2);
	BN_clear_free(rB);
	BN_clear_free(hashone);
	BN_clear_free(t3);
	BN_clear_free(t4);
	BN_clear_free(x1);
	OPENSSL_free(asterisk_seq);
	OPENSSL_free(D);
	OPENSSL_free(coefs);
	OPENSSL_free(one);
	OPENSSL_free(bytes);
	return ret;
}

size_t BOOTLE_SIGMA1_serialize(unsigned char **ret, struct BOOTLE_SIGMA1 *sig1, int dbase, int dexp)
{
	int i;
	int j;
	size_t retlen = 0;
	unsigned char *t;
	unsigned char *rt;
	size_t rtlen;

	rtlen = EC_POINT_point2buf(sig1->curve, sig1->A,
			POINT_CONVERSION_UNCOMPRESSED, &t, 0);
	retlen += rtlen;
	rt = realloc(*ret, retlen);
	if(!rt) goto reallocerr;
	memcpy(*ret, t, rtlen);
	free(t);

	rtlen = EC_POINT_point2buf(sig1->curve, sig1->C,
			POINT_CONVERSION_UNCOMPRESSED, &t, 0);
	retlen += rtlen;
	rt = realloc(*ret, retlen);
	if(!rt) goto reallocerr;
	memcpy(*ret, t, rtlen);
	free(t);

	rtlen = EC_POINT_point2buf(sig1->curve, sig1->D,
			POINT_CONVERSION_UNCOMPRESSED, &t, 0);
	retlen += rtlen;
	rt = realloc(*ret, retlen);
	if(!rt) goto reallocerr;
	memcpy(*ret, t, rtlen);
	free(t);

	for(i=0; i<dbase; i++)
	{
		for(j=0; j<dexp; j++)
		{
			BIGNUM *fij = sig1->trimmed_challenge[i][j];
			rt = realloc(*ret+retlen, retlen+BN_num_bytes(fij));
			if(!rt) goto reallocerr;
			BN_bn2bin(fij, *ret+retlen);
			retlen+=BN_num_bytes(fij);
		}
	}
	
	OPENSSL_free(t);
	rtlen = BN_num_bytes(sig1->za);
	t = OPENSSL_malloc(rtlen);
	rt = realloc(*ret, retlen+rtlen);
 	if(!rt) goto reallocerr;
	BN_bn2bin(sig1->za, t);
	memcpy(*ret+retlen, t, rtlen);
	retlen += rtlen;


	OPENSSL_free(t);
	rtlen = BN_num_bytes(sig1->zc);
	t = OPENSSL_malloc(rtlen);
	rt = realloc(*ret, retlen+rtlen);
 	if(!rt) goto reallocerr;
	BN_bn2bin(sig1->zc, t);
	memcpy(*ret+retlen, t, rtlen);
	retlen += rtlen;
	return retlen;
reallocerr:
	perror("memory allocation error");
	return 0;
}

size_t BOOTLE_SIGMA2_serialize(unsigned char **ret, struct BOOTLE_SIGMA2 *sig2, int dbase, int dexp)
{
	int i;
	size_t retlen = 0;
	unsigned char *t;
	unsigned char *rt;
	size_t rtlen = BOOTLE_SIGMA1_serialize(&t, sig2->sig1, dbase, dexp);
	if(rtlen == 0) return 0; // TODO
	*ret = OPENSSL_malloc(rtlen);
	memcpy(*ret, t, rtlen);
	OPENSSL_free(t); t=0;
	rtlen = EC_POINT_point2buf(sig2->sig1->curve, sig2->B,
			POINT_CONVERSION_UNCOMPRESSED, &t, 0);
	retlen += rtlen;
	rt = realloc(*ret, retlen);
	if(!rt) goto reallocerr;
	memcpy(*ret, t, rtlen);
	for(i=0; i<dexp; i++)
	{
		OPENSSL_free(t);
		rtlen = EC_POINT_point2buf(sig2->sig1->curve, sig2->G[i][0],
				POINT_CONVERSION_UNCOMPRESSED, &t, 0);
		retlen += rtlen;
		rt = realloc(*ret, retlen);
		if(!rt) goto reallocerr;
		memcpy(*ret, t, rtlen);

		OPENSSL_free(t);
		rtlen = EC_POINT_point2buf(sig2->sig1->curve, sig2->G[i][1],
				POINT_CONVERSION_UNCOMPRESSED, &t, 0);
		retlen += rtlen;
		rt = realloc(*ret, retlen);
		if(!rt) goto reallocerr;
		memcpy(*ret, t, rtlen);
	}
	
	OPENSSL_free(t);
	rtlen = BN_num_bytes(sig2->z);
	t = OPENSSL_malloc(rtlen);
	rt = realloc(*ret, retlen+rtlen);
 	if(!rt) goto reallocerr;
	BN_bn2bin(sig2->z, t);
	memcpy(*ret+retlen, t, rtlen);
	retlen += rtlen;
	return retlen;
reallocerr:
	perror("memory allocation error");
	return 0;
}

void
BOOTLE_SIGMA1_free(struct BOOTLE_SIGMA1 *b)
{
	EC_POINT_clear_free(b->A);
	EC_POINT_clear_free(b->C);
	EC_POINT_clear_free(b->D);
	int i;
	int j;
	for(i=0; i<b->a_n; i++)
	{
		for(j=0; j<b->a_m-1; j++)
		{
			BN_free(b->trimmed_challenge[i][j]);
			BN_free(b->a[i][j]);
		}
		BN_free(b->a[i][j]);
		OPENSSL_free(b->trimmed_challenge[i]);
		OPENSSL_free(b->a[i]);
	}
	OPENSSL_free(b->trimmed_challenge);
	OPENSSL_free(b->a);
	BN_clear_free(b->za);
	BN_clear_free(b->zc);
	free(b);
}

void
BOOTLE_SIGMA2_free(struct BOOTLE_SIGMA2 *b)
{
	int i;
	int j;
	EC_POINT_free(b->B);
	for(i=0; i<b->sig1->a_n; i++)
	{
		for(j=0; j<b->sig1->a_m; j++)
		{
			EC_POINT_free(b->G[i][j]);
		}
		OPENSSL_free(b->G[i]);
	}
	OPENSSL_free(b->G);
	BOOTLE_SIGMA1_free(b->sig1);
}
