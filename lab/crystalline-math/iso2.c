#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define B 65536u
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void p(const char*n,CrystallineAbacus*x){uint64_t v=0;MathError e=abacus_to_uint64(x,&v);printf("  %s: err=%d val=%016llx beads=%zu exp[%d..%d] neg=%d\n",n,e,(unsigned long long)v,x->num_beads,x->min_exponent,x->max_exponent,x->negative);}
static CrystallineAbacus* two64(){CrystallineAbacus*t=abacus_from_uint64(1ULL<<32,B),*r=abacus_new(B);abacus_mul(r,t,t);abacus_free(t);return r;}
int main(int argc,char**argv){ setvbuf(stdout,NULL,_IONBF,0); const char*t=argv[1];
  uint64_t a=0xDEADBEEFCAFEBABEULL,b=0x0123456789ABCDEFULL; __uint128_t pp=(__uint128_t)a*b;
  CrystallineAbacus *A=abacus_from_uint64(a,B),*Bb=abacus_from_uint64(b,B),*R=abacus_new(B),*Q=abacus_new(B),*P=abacus_new(B);
  printf("[%s]\n",t);
  if(!strcmp(t,"two64")){ CrystallineAbacus*x=two64(); p("2^64",x); }
  if(!strcmp(t,"div_by_two64_mul")){ abacus_mul(P,A,Bb); CrystallineAbacus*x=two64(); MathError e=abacus_div(Q,R,P,x); printf("  err=%d\n",e); p("q",Q); p("r",R); printf("  native hi=%016llx lo=%016llx\n",(unsigned long long)(pp>>64),(unsigned long long)pp); }
  if(!strcmp(t,"mod_by_two64_mul")){ abacus_mul(P,A,Bb); CrystallineAbacus*x=two64(); MathError e=abacus_mod(R,P,x); printf("  err=%d\n",e); p("r",R); }
  if(!strcmp(t,"shl_then_normalize")){ CrystallineAbacus*one=abacus_from_uint64(1,B),*x=abacus_new(B); abacus_shift_left(x,one,4); abacus_normalize(x); p("2^64 normalized",x); CrystallineAbacus*y=two64(); printf("  compare(shl,mul)=%d\n",abacus_compare(x,y)); }
  if(!strcmp(t,"add_overflow_reduce")){ CrystallineAbacus*x=two64(); abacus_add(R,A,A); p("A+A",R); int c=abacus_compare(R,x); printf("  cmp(A+A,2^64)=%d\n",c); if(c>=0){abacus_sub(Q,R,x); p("reduced",Q);} printf("  native=%016llx\n",(unsigned long long)(a+a)); }
  if(!strcmp(t,"sub_neg_reduce")){ CrystallineAbacus*x=two64(); abacus_sub(R,Bb,A); p("B-A",R); abacus_add(Q,R,x); p("B-A+2^64",Q); printf("  native=%016llx\n",(unsigned long long)(b-a)); }
  if(!strcmp(t,"base2_shifts")){ CrystallineAbacus*x=abacus_from_uint64(a,2),*l=abacus_new(2),*r=abacus_new(2); MathError e1=abacus_shift_right(r,x,13); abacus_normalize(r); MathError e2=abacus_shift_left(l,x,51); abacus_normalize(l); p("a>>13",r); printf("  native=%016llx\n",(unsigned long long)(a>>13)); printf("  a<<51: err=%d beads=%zu exp[%d..%d]\n",e2,l->num_beads,l->min_exponent,l->max_exponent); 
     /* low 64 of (a<<51): drop beads with exponent>=64 */ uint64_t lo=0; for(size_t i=0;i<l->num_beads;i++){int32_t w=l->beads[i].weight_exponent; if(w>=0&&w<64&&l->beads[i].value) lo|=1ULL<<w;} printf("  low64(a<<51)=%016llx native=%016llx\n",(unsigned long long)lo,(unsigned long long)(a<<51)); }
  if(!strcmp(t,"base2_layout")){ CrystallineAbacus*x=abacus_from_uint64(0x8000000000000005ULL,2); printf("  beads=%zu exp[%d..%d]\n",x->num_beads,x->min_exponent,x->max_exponent); for(size_t i=0;i<x->num_beads;i++){ if(x->beads[i].value) printf("  bead[%zu] v=%u w=%d\n",i,x->beads[i].value,x->beads[i].weight_exponent);} CrystallineAbacus*y=abacus_from_uint64(5,2); printf("  small: beads=%zu exp[%d..%d]\n",y->num_beads,y->min_exponent,y->max_exponent); }
  if(!strcmp(t,"frac_shift")){ CrystallineAbacus*x=abacus_from_uint64(0x1921FB54442D18ULL,2),*y=abacus_new(2); MathError e=abacus_shift_right(y,x,52); printf("  err=%d beads=%zu exp[%d..%d] neg=%d\n",e,y->num_beads,y->min_exponent,y->max_exponent,y->negative); double d=0; abacus_to_double(y,&d); printf("  to_double=%.17g (pi=3.141592653589793)\n",d); CrystallineAbacus*z=abacus_new(2); abacus_mul(z,y,y); double d2=0; abacus_to_double(z,&d2); printf("  pi^2 approx=%.17g beads=%zu exp[%d..%d]\n",d2,z->num_beads,z->min_exponent,z->max_exponent); }
  if(!strcmp(t,"sqrt_big")){ CrystallineAbacus*one=abacus_from_uint64(1,B),*big=abacus_new(B),*x=two64(),*s=abacus_new(B); abacus_mul(big,x,x); /*2^128*/ MathError e=abacus_sqrt(s,big); printf("  err=%d\n",e); printf("  cmp(sqrt(2^128),2^64)=%d\n",abacus_compare(s,x)); }
  if(!strcmp(t,"perf")){ int N=100000; double t0=now(); CrystallineAbacus*x=two64(); uint64_t acc=0;
     for(int i=0;i<N;i++){ CrystallineAbacus*u=abacus_from_uint64(a+i,B),*v=abacus_from_uint64(b^i,B),*w=abacus_new(B); abacus_add(w,u,v); if(abacus_compare(w,x)>=0){abacus_sub(u,w,x); uint64_t z;abacus_to_uint64(u,&z);acc^=z;} else {uint64_t z;abacus_to_uint64(w,&z);acc^=z;} abacus_free(u);abacus_free(v);abacus_free(w);} 
     double dt=now()-t0; printf("  add64 mod 2^64: %.0f ops/s\n",N/dt);
     t0=now(); for(int i=0;i<N;i++){ CrystallineAbacus*u=abacus_from_uint64(a+i,B),*v=abacus_from_uint64(b^i,B),*w=abacus_new(B),*h=abacus_new(B); abacus_mul(w,u,v); abacus_shift_right(h,w,4); abacus_normalize(h); uint64_t z;abacus_to_uint64(h,&z);acc^=z; abacus_free(u);abacus_free(v);abacus_free(w);abacus_free(h);} 
     dt=now()-t0; printf("  mulh64: %.0f ops/s\n",N/dt);
     t0=now(); for(int i=0;i<N;i++){ CrystallineAbacus*u=abacus_from_uint64(a+i,2),*v=abacus_from_uint64(b^i,2); uint64_t z=0; /* xor digit-wise on base-2 beads */ uint8_t bits[64]={0}; for(size_t k=0;k<u->num_beads;k++){int32_t w=u->beads[k].weight_exponent; if(w>=0&&w<64) bits[w]^=u->beads[k].value&1;} for(size_t k=0;k<v->num_beads;k++){int32_t w=v->beads[k].weight_exponent; if(w>=0&&w<64) bits[w]^=v->beads[k].value&1;} for(int k=0;k<64;k++) z|=(uint64_t)bits[k]<<k; acc^=z; abacus_free(u);abacus_free(v);} 
     dt=now()-t0; printf("  xor64 via base-2 beads: %.0f ops/s\n",N/dt);
     t0=now(); for(int i=0;i<N/10;i++){ CrystallineAbacus*u=abacus_from_uint64(a+i,2),*r=abacus_new(2),*l=abacus_new(2),*s=abacus_new(2); abacus_shift_right(r,u,13); abacus_shift_left(l,u,51); abacus_normalize(l); abacus_add(s,r,l); uint64_t z=0; for(size_t k=0;k<s->num_beads;k++){int32_t w=s->beads[k].weight_exponent; if(w>=0&&w<64&&s->beads[k].value) z|=1ULL<<w;} acc^=z; abacus_free(u);abacus_free(r);abacus_free(l);abacus_free(s);} 
     dt=now()-t0; printf("  ror64 via base-2 shifts+add: %.0f ops/s  (acc=%llx)\n",(N/10)/dt,(unsigned long long)acc); }
  printf("  done\n"); return 0; }
