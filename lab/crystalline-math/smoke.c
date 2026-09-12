#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(){ setvbuf(stdout,NULL,_IONBF,0);
  /* 1) 64x64 -> 128 mul, take high 64 via base 65536 shift */
  uint64_t a=0xDEADBEEFCAFEBABEULL,b=0x0123456789ABCDEFULL;
  __uint128_t p=(__uint128_t)a*b; uint64_t hi_native=(uint64_t)(p>>64), lo_native=(uint64_t)p;
  CrystallineAbacus *A=abacus_from_uint64(a,65536),*B=abacus_from_uint64(b,65536),*P=abacus_new(65536),*H=abacus_new(65536);
  MathError e=abacus_mul(P,A,B); printf("mul err=%d\n",e);
  e=abacus_shift_right(H,P,4); printf("shr err=%d\n",e);
  uint64_t hi=0; e=abacus_to_uint64(H,&hi); printf("to_u64 err=%d hi=%016llx native=%016llx %s\n",e,(unsigned long long)hi,(unsigned long long)hi_native,hi==hi_native?"OK":"MISMATCH");
  /* low 64: P mod 2^64 */
  CrystallineAbacus *M=abacus_new(65536),*L=abacus_new(65536),*one=abacus_from_uint64(1,65536),*two64=abacus_new(65536);
  abacus_shift_left(two64,one,4); abacus_mod(L,P,two64); uint64_t lo=0; e=abacus_to_uint64(L,&lo);
  printf("lo=%016llx native=%016llx %s (err=%d)\n",(unsigned long long)lo,(unsigned long long)lo_native,lo==lo_native?"OK":"MISMATCH",e);
  /* 2) base-2 conversion for bitwise */
  CrystallineAbacus *A2=NULL; e=abacus_convert_base(&A2,A,2); printf("convert err=%d beads=%zu sparse=%d min=%d max=%d\n",e,A2?A2->num_beads:0,A2?A2->is_sparse:-1,A2?A2->min_exponent:0,A2?A2->max_exponent:0);
  uint64_t back=0; abacus_to_uint64(A2,&back); printf("base2 roundtrip %s\n",back==a?"OK":"MISMATCH");
  /* 3) large exact value: 2^1100 / 3 via div */
  CrystallineAbacus *big=abacus_new(65536),*three=abacus_from_uint64(3,65536),*q=abacus_new(65536),*r=abacus_new(65536);
  abacus_shift_left(big,one,1100/16); e=abacus_div(q,r,big,three); uint64_t rr=0; abacus_to_uint64(r,&rr); printf("div err=%d rem=%llu (expect %d)\n",e,(unsigned long long)rr, (1100%2==0)?1:2);
  /* 4) throughput */
  double t=now(); int N=200000; uint64_t acc=0;
  for(int i=0;i<N;i++){ CrystallineAbacus *x=abacus_from_uint64(a+i,65536),*y=abacus_from_uint64(b^i,65536),*z=abacus_new(65536); abacus_mul(z,x,y); uint64_t v; abacus_mod(L,z,two64); abacus_to_uint64(L,&v); acc^=v; abacus_free(x);abacus_free(y);abacus_free(z);} 
  double dt=now()-t; printf("mul64 (alloc+mul+mod+free): %.0f ops/s  acc=%llx\n",N/dt,(unsigned long long)acc);
  t=now(); for(int i=0;i<N;i++){ CrystallineAbacus *x=abacus_from_uint64(a+i,65536),*y=abacus_from_uint64(b^i,65536),*z=abacus_new(65536); abacus_add(z,x,y); uint64_t v; abacus_to_uint64(z,&v); acc^=v; abacus_free(x);abacus_free(y);abacus_free(z);} 
  dt=now()-t; printf("add64: %.0f ops/s\n",N/dt);
  t=now(); for(int i=0;i<20000;i++){ CrystallineAbacus *x=abacus_from_uint64(a+i,2); uint64_t v; abacus_to_uint64(x,&v); acc^=v; abacus_free(x);} dt=now()-t; printf("from_uint64 base2 + back: %.0f ops/s\n",20000/dt);
  return 0; }
