#include "sha256.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

typedef struct { uint32_t h[8]; uint64_t bits; unsigned char block[64]; size_t used; } Sha256;
static const uint32_t k[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
#define R(x,n) (((x) >> (n)) | ((x) << (32-(n))))
static void block(Sha256 *s, const unsigned char b[64]) {
  uint32_t w[64], a,bv,c,d,e,f,g,h,t1,t2; int i;
  for(i=0;i<16;i++) w[i]=((uint32_t)b[i*4]<<24)|((uint32_t)b[i*4+1]<<16)|((uint32_t)b[i*4+2]<<8)|b[i*4+3];
  for(i=16;i<64;i++) w[i]=(R(w[i-15],7)^R(w[i-15],18)^(w[i-15]>>3))+w[i-16]+(R(w[i-2],17)^R(w[i-2],19)^(w[i-2]>>10))+w[i-7];
  a=s->h[0];bv=s->h[1];c=s->h[2];d=s->h[3];e=s->h[4];f=s->h[5];g=s->h[6];h=s->h[7];
  for(i=0;i<64;i++) { t1=h+(R(e,6)^R(e,11)^R(e,25))+((e&f)^((~e)&g))+k[i]+w[i]; t2=(R(a,2)^R(a,13)^R(a,22))+((a&bv)^(a&c)^(bv&c)); h=g;g=f;f=e;e=d+t1;d=c;c=bv;bv=a;a=t1+t2; }
  s->h[0]+=a;s->h[1]+=bv;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void init(Sha256 *s) { uint32_t x[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}; memcpy(s->h,x,sizeof x);s->bits=0;s->used=0; }
static void add(Sha256 *s,const unsigned char *p,size_t n){ while(n--){s->block[s->used++]=*p++;if(s->used==64){block(s,s->block);s->bits+=512;s->used=0;}}}
static void finish(Sha256 *s,unsigned char out[32]) { int i; uint64_t bits=s->bits+s->used*8; s->block[s->used++]=0x80; if(s->used>56){while(s->used<64)s->block[s->used++]=0;block(s,s->block);s->used=0;} while(s->used<56)s->block[s->used++]=0; for(i=7;i>=0;i--)s->block[s->used++]=(unsigned char)(bits>>(i*8)); block(s,s->block); for(i=0;i<8;i++){out[i*4]=s->h[i]>>24;out[i*4+1]=s->h[i]>>16;out[i*4+2]=s->h[i]>>8;out[i*4+3]=s->h[i];}}
void password_digest(const char *password,const char *salt,char out[65]) { Sha256 s; unsigned char digest[32]; size_t i; init(&s);add(&s,(const unsigned char*)salt,strlen(salt));add(&s,(const unsigned char*)":",1);add(&s,(const unsigned char*)password,strlen(password));finish(&s,digest);for(i=0;i<32;i++)sprintf(out+i*2,"%02x",digest[i]);out[64]=0; }
