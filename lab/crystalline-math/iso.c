#include "math/abacus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define B 65536u
static void p(const char*n,CrystallineAbacus*x){uint64_t v=0;MathError e=abacus_to_uint64(x,&v);printf("  %s: err=%d val=%016llx beads=%zu sparse=%d exp[%d..%d]\n",n,e,(unsigned long long)v,x->num_beads,x->is_sparse,x->min_exponent,x->max_exponent);}
int main(int argc,char**argv){ setvbuf(stdout,NULL,_IONBF,0); const char*t=argv[1];
  uint64_t a=0xDEADBEEFCAFEBABEULL,b=0x0123456789ABCDEFULL;
  CrystallineAbacus *A=abacus_from_uint64(a,B),*Bb=abacus_from_uint64(b,B),*one=abacus_from_uint64(1,B),*R=abacus_new(B),*Q=abacus_new(B),*P=abacus_new(B);
  printf("[%s]\n",t);
  if(!strcmp(t,"new")){ p("new",R); }
  if(!strcmp(t,"shl_one")){ MathError e=abacus_shift_left(R,one,4); printf("  err=%d\n",e); p("2^64",R); }
  if(!strcmp(t,"mul")){ MathError e=abacus_mul(P,A,Bb); printf("  err=%d\n",e); p("P",P); }
  if(!strcmp(t,"div_small")){ CrystallineAbacus*three=abacus_from_uint64(3,B); MathError e=abacus_div(Q,R,A,three); printf("  err=%d\n",e); p("q",Q); p("r",R); printf("  native q=%016llx r=%llu\n",(unsigned long long)(a/3),(unsigned long long)(a%3)); }
  if(!strcmp(t,"div_by_2e64")){ abacus_mul(P,A,Bb); CrystallineAbacus*two64=abacus_new(B); abacus_shift_left(two64,one,4); MathError e=abacus_div(Q,R,P,two64); printf("  err=%d\n",e); p("q",Q); p("r",R); }
  if(!strcmp(t,"mod_by_2e64")){ abacus_mul(P,A,Bb); CrystallineAbacus*two64=abacus_new(B); abacus_shift_left(two64,one,4); MathError e=abacus_mod(R,P,two64); printf("  err=%d\n",e); p("r",R); }
  if(!strcmp(t,"mod_small")){ CrystallineAbacus*m=abacus_from_uint64(1000003,B); MathError e=abacus_mod(R,A,m); printf("  err=%d\n",e); p("r",R); printf("  native=%llu\n",(unsigned long long)(a%1000003)); }
  if(!strcmp(t,"convert2")){ CrystallineAbacus*A2=NULL; MathError e=abacus_convert_base(&A2,A,2); printf("  err=%d\n",e); if(A2)p("A2",A2); }
  if(!strcmp(t,"from_u64_base2")){ CrystallineAbacus*x=abacus_from_uint64(a,2); p("x",x); }
  if(!strcmp(t,"add")){ MathError e=abacus_add(R,A,Bb); printf("  err=%d\n",e); p("r",R); printf("  native=%016llx\n",(unsigned long long)(a+b)); }
  if(!strcmp(t,"sub")){ MathError e=abacus_sub(R,A,Bb); printf("  err=%d\n",e); p("r",R); printf("  native=%016llx\n",(unsigned long long)(a-b)); }
  if(!strcmp(t,"sub_neg")){ MathError e=abacus_sub(R,Bb,A); printf("  err=%d neg=%d\n",e,abacus_is_negative(R)); p("r",R); }
  if(!strcmp(t,"sqrt")){ CrystallineAbacus*n=abacus_from_uint64(1000000007ULL*1000000007ULL,B); MathError e=abacus_sqrt(R,n); printf("  err=%d\n",e); p("r",R); }
  if(!strcmp(t,"cmp")){ printf("  cmp(A,B)=%d cmp(B,A)=%d cmp(A,A)=%d\n",abacus_compare(A,Bb),abacus_compare(Bb,A),abacus_compare(A,A)); }
  if(!strcmp(t,"big_shl")){ CrystallineAbacus*big=abacus_new(B); MathError e=abacus_shift_left(big,one,70); printf("  err=%d beads=%zu max=%d\n",e,big->num_beads,big->max_exponent); }
  if(!strcmp(t,"from_double")){ CrystallineAbacus*x=abacus_from_double(157.25,2,8); printf("  beads=%zu exp[%d..%d]\n",x->num_beads,x->min_exponent,x->max_exponent); double d=0; abacus_to_double(x,&d); printf("  back=%.17g\n",d); }
  printf("  done\n"); return 0; }
