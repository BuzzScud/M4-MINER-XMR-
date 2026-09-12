#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define B 65536u
/* pad: make beads contiguous from exponent 0..max, ascending, zero-filled */
static void pad(CrystallineAbacus*x){ if(x->is_sparse) abacus_densify(x); int32_t mx=0; for(size_t i=0;i<x->num_beads;i++){int32_t w=x->beads[i].weight_exponent; if(w>mx)mx=w;} size_t n=(size_t)mx+1; AbacusBead*nb=calloc(n,sizeof(AbacusBead)); for(size_t i=0;i<n;i++){nb[i].weight_exponent=(int32_t)i; nb[i].value=0;} for(size_t i=0;i<x->num_beads;i++){int32_t w=x->beads[i].weight_exponent; if(w>=0) nb[w]=x->beads[i];} free(x->beads); x->beads=nb; x->num_beads=n; x->capacity=n; x->min_exponent=0; x->max_exponent=mx; }
static void p(const char*n,CrystallineAbacus*x){uint64_t v=0;MathError e=abacus_to_uint64(x,&v);printf("  %s: err=%d val=%016llx beads=%zu exp[%d..%d] neg=%d |",n,e,(unsigned long long)v,x->num_beads,x->min_exponent,x->max_exponent,x->negative); for(size_t i=0;i<x->num_beads&&i<10;i++) printf(" %u@%d",x->beads[i].value,x->beads[i].weight_exponent); printf("\n");}
static CrystallineAbacus* two64(){CrystallineAbacus*t=abacus_from_uint64(1ULL<<32,B),*r=abacus_new(B);abacus_mul(r,t,t);pad(r);return r;}
int main(int argc,char**argv){ setvbuf(stdout,NULL,_IONBF,0); const char*t=argv[1];
  uint64_t a=0xDEADBEEFCAFEBABEULL,b=0x0123456789ABCDEFULL; __uint128_t pp=(__uint128_t)a*b;
  CrystallineAbacus *A=abacus_from_uint64(a,B),*Bb=abacus_from_uint64(b,B),*R=abacus_new(B),*Q=abacus_new(B),*P=abacus_new(B);
  printf("[%s]\n",t);
  if(!strcmp(t,"cmp_pad")){ CrystallineAbacus*x=two64(); p("2^64 padded",x); abacus_add(R,A,A); pad(R); p("A+A",R); printf("  cmp(A+A,2^64)=%d (expect 1)  cmp(2^64,A+A)=%d\n",abacus_compare(R,x),abacus_compare(x,R)); abacus_sub(Q,R,x); pad(Q); p("A+A-2^64",Q); printf("  native=%016llx\n",(unsigned long long)(a+a)); }
  if(!strcmp(t,"div_pad")){ abacus_mul(P,A,Bb); pad(P); p("P",P); CrystallineAbacus*x=two64(); MathError e=abacus_div(Q,R,P,x); printf("  err=%d\n",e); p("q",Q); p("r",R); printf("  native hi=%016llx lo=%016llx\n",(unsigned long long)(pp>>64),(unsigned long long)pp); }
  if(!strcmp(t,"div_pad_small_divisor")){ abacus_mul(P,A,Bb); pad(P); CrystallineAbacus*d=abacus_from_uint64(0xFFFFFFFFULL,B); MathError e=abacus_div(Q,R,P,d); printf("  err=%d\n",e); p("q",Q); p("r",R); printf("  native q=%016llx%016llx r=%llx\n",(unsigned long long)((pp/0xFFFFFFFFULL)>>64),(unsigned long long)(pp/0xFFFFFFFFULL),(unsigned long long)(pp%0xFFFFFFFFULL)); }
  if(!strcmp(t,"sqrt_pad")){ CrystallineAbacus*n=abacus_from_uint64(0xFFFFFFFFFFFFFFFFULL,B),*sq=abacus_new(B),*r2=abacus_new(B); abacus_mul(sq,n,n); pad(sq); MathError e=abacus_sqrt(r2,sq); printf("  err=%d\n",e); p("sqrt((2^64-1)^2)",r2); }
  if(!strcmp(t,"trunc")){ abacus_mul(P,A,Bb); pad(P); CrystallineAbacus*H=abacus_new(B),*T=abacus_new(B); abacus_shift_right(H,P,4); MathError e=abacus_truncate(T,H,0); printf("  trunc err=%d\n",e); p("hi trunc",T); printf("  native hi=%016llx\n",(unsigned long long)(pp>>64)); }
  printf("  done\n"); return 0; }
