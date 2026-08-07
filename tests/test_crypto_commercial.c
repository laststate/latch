#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
ls_result_t ls_crypto_xchacha20_poly1305_encrypt(const uint8_t key[32], const uint8_t nonce[24], const uint8_t *aad, size_t aad_length, const uint8_t *plaintext, uint8_t *ciphertext, size_t length, uint8_t tag[16]);
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"commercial crypto failed: %s:%d\n",#x,__LINE__); return 1; } } while(0)
static ls_result_t hk(void *c,const uint8_t*s,size_t sl,const uint8_t*i,size_t il,const uint8_t*n,size_t nl,uint8_t*o,size_t ol){(void)c;return ls_hkdf_sha256(s,sl,i,il,n,nl,o,ol);}
static ls_result_t enc(void*c,const uint8_t k[32],const uint8_t n[24],const uint8_t*a,size_t al,const uint8_t*p,uint8_t*x,size_t l,uint8_t t[16]){(void)c;return ls_xchacha20_poly1305_encrypt(k,n,a,al,p,x,l,t);}
static ls_result_t dec(void*c,const uint8_t k[32],const uint8_t n[24],const uint8_t*a,size_t al,const uint8_t*x,uint8_t*p,size_t l,const uint8_t t[16]){(void)c;return ls_xchacha20_poly1305_decrypt(k,n,a,al,x,p,l,t);}

static ls_result_t bad_hk(void *c,const uint8_t*s,size_t sl,const uint8_t*i,size_t il,const uint8_t*n,size_t nl,uint8_t*o,size_t ol){(void)c;(void)s;(void)sl;(void)i;(void)il;(void)n;(void)nl;memset(o,0,ol);return LS_OK;}
static ls_result_t bad_enc(void*c,const uint8_t k[32],const uint8_t n[24],const uint8_t*a,size_t al,const uint8_t*p,uint8_t*x,size_t l,uint8_t t[16]){ls_result_t r=enc(c,k,n,a,al,p,x,l,t); if(r==LS_OK && l) x[0]^=1u; return r;}
static ls_result_t bad_dec(void*c,const uint8_t k[32],const uint8_t n[24],const uint8_t*a,size_t al,const uint8_t*x,uint8_t*p,size_t l,const uint8_t t[16]){uint8_t tmp[32]; if(l>sizeof tmp) return LS_EOVERFLOW; memcpy(tmp,x,l); if(l) tmp[0]^=1u; return dec(c,k,n,a,al,tmp,p,l,t);}

int main(void){
  uint8_t key[32]={0},nonce[24]={0},out[1],tag[16]; const uint8_t in[1]={7};
  CHECK(!ls_security_crypto_provider_ready());
  ls_security_policy_t legacy={.algorithm=LS_SECURITY_HMAC_SHA256,.key_id=1u,.reject_plaintext=true,.allow_legacy_hmac=true};
  CHECK(ls_security_set_policy(&legacy)==LS_ENOTSUP);
  CHECK(ls_crypto_xchacha20_poly1305_encrypt(key,nonce,NULL,0,in,out,1,tag)==LS_EAUTH);
  ls_crypto_provider_t bad={.name="bad-kat",.version="1",.audit_reference="external-audit-ref",.assurance=LS_CRYPTO_ASSURANCE_EXTERNAL_AUDITED,.hkdf_sha256=bad_hk,.xchacha20_poly1305_encrypt=enc,.xchacha20_poly1305_decrypt=dec};
  CHECK(ls_security_set_crypto_provider(&bad)==LS_EAUTH);
  bad.hkdf_sha256=hk; bad.xchacha20_poly1305_encrypt=bad_enc; bad.xchacha20_poly1305_decrypt=bad_dec; CHECK(ls_security_set_crypto_provider(&bad)==LS_EAUTH);
  ls_crypto_provider_t weak={.name="weak",.version="1",.audit_reference="none",.assurance=LS_CRYPTO_ASSURANCE_TEST_ONLY,.hkdf_sha256=hk,.xchacha20_poly1305_encrypt=enc,.xchacha20_poly1305_decrypt=dec};
  CHECK(ls_security_set_crypto_provider(&weak)==LS_EAUTH);
  ls_crypto_provider_t audited=weak; audited.name="audited-fixture"; audited.audit_reference="external-audit-ref"; audited.assurance=LS_CRYPTO_ASSURANCE_EXTERNAL_AUDITED;
  CHECK(ls_security_set_crypto_provider(&audited)==LS_OK);
  CHECK(ls_security_crypto_provider_ready());
  CHECK(ls_crypto_xchacha20_poly1305_encrypt(key,nonce,NULL,0,in,out,1,tag)==LS_OK);
  ls_security_clear_crypto_provider(); CHECK(!ls_security_crypto_provider_ready());
  return 0;
}
