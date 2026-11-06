// VINKELSLIP level calibration: prints the kLevelDb table for src/Vinkelslip.h
//   g++ -std=c++17 -O2 -Isrc tests/calibrate_slip.cpp -o cal && ./cal
// (run it with kLevelDb set to all zeros to get a fresh table)
#include "Vinkelslip.h"
#include <cstdio>
using namespace motstand;
int main(){ printf("{\n");
 for(int m=0;m<5;++m){ printf("  {"); for(int k=0;k<=8;++k){ float d=k/8.f; Vinkelslip v; v.prepare(48000); v.mode=m; v.drive=d<1e-3f?1e-3f:d; v.matning=0; v.onTrigger(0.6f);
   double si=0,so=0; for(int i=0;i<96000;++i){ float x=0.5f*std::sin(i*0.012f)*std::exp(-(i%6000)/1500.f); float y=v.process(x); if(i>24000){si+=x*x; so+=y*y;} }
   printf("%s%.2ff", k?", ":"", -10*std::log10(so/si)); } printf(" },\n"); }
 // ROM mode separately (base-rate)
 printf("  {"); for(int k=0;k<=8;++k){ float d=k/8.f; Vinkelslip v; v.prepare(48000); v.mode=ROMKROSS; v.drive=d<1e-3f?1e-3f:d; v.matning=0; v.onTrigger(0.6f);
   double si=0,so=0; for(int i=0;i<96000;++i){ float x=0.5f*std::sin(i*0.012f)*std::exp(-(i%6000)/1500.f); float y=v.process(x); if(i>24000){si+=x*x; so+=y*y;} }
   printf("%s%.2ff", k?", ":"", -10*std::log10(so/si)); } printf(" },\n}\n"); }
