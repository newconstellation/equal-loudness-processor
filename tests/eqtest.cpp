#include "../Source/DSP.h"
#include <cstdio>
using namespace elp;
int main(){
  for (double fs : {44100.0, 48000.0, 96000.0}){
    GraphicEqDesigner d; d.prepare(fs);
    // target: loudness comp at 80 dB vs 95 ref + room-ish wiggle
    std::array<double,NB> t{};
    for(int i=0;i<NB;i++){ double P=80,Pr=95; t[i]=(iso226::phonToSpl(i,P)-P)-(iso226::phonToSpl(i,Pr)-Pr) + 3*std::sin(i*0.9); t[i]=clampd(t[i],-12,8);}
    auto g=d.solve(t); auto r=d.response(g);
    double maxErr=0, maxRip=0;
    for(int i=0;i<NB;i++) if(bandCentres()[i]<0.45*fs) maxErr=std::max(maxErr,std::abs(r[i]-t[i]));
    for(int i=0;i<NB-1;i++){ double fm=std::sqrt(bandCentres()[i]*bandCentres()[i+1]); if(fm>0.45*fs) break; double mid=d.responseAt(g,fm); double lin=(t[i]+t[i+1])/2; maxRip=std::max(maxRip,std::abs(mid-lin)); }
    // flat +6 broad
    std::array<double,NB> t2; t2.fill(6.0); auto g2=d.solve(t2); double rip2=0;
    for(int i=0;i<NB-1;i++){ double fm=std::sqrt(bandCentres()[i]*bandCentres()[i+1]); if(fm>0.45*fs) break; rip2=std::max(rip2,std::abs(d.responseAt(g2,fm)-6)); }
    double gmax=0; for(auto v:g) gmax=std::max(gmax,std::abs(v));
    printf("fs %.0f  centre err %.3f dB  midpoint dev %.2f dB  flat+6 ripple %.2f dB  max filter gain %.1f\n",fs,maxErr,maxRip,rip2,gmax);
  }
  auto z=zwicker([]{std::array<double,NB>a; a.fill(-200); a[17]=40; return a;}(),false);
  printf("1 kHz 40 dB free field: %.3f sone (expect ~1.0)\n", z.N);
}
