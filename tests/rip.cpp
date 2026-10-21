#include "../Source/DSP.h"
#include <cstdio>
using namespace elp;
int main(){ double fs=48000; GraphicEqDesigner d; d.prepare(fs); std::array<double,NB> t; t.fill(6.0); auto g=d.solve(t);
 for(int i=0;i<NB-1;i++){ double fm=std::sqrt(bandCentres()[i]*bandCentres()[i+1]); printf("%s-%s mid %.2f  g=%.2f\n",bandLabel(i),bandLabel(i+1),d.responseAt(g,fm),g[i]); } }
