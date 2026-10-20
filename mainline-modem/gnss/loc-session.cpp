#include "qmi-loc.h"
#include <cassert>
#include <cstdio>
int main() {
    assert(!QmiLoc::validTlvs({1,4,0,0}));
    assert(!QmiLoc::validTlvs({1,0,0,1,0,0}));
    assert(QmiLoc::validTlvs({1,1,0,7}));
    QmiLoc loc;
    unsigned positions=0,nmea=0;
    if (!loc.start(1000,[&](uint16_t id,const QmiReply& fields){
        if (id==0x24) { uint32_t status=~0u; QmiLoc::value(fields,1,status); ++positions; printf("position report status=%u\n",status); }
        else if (id==0x26) { ++nmea; const auto b=fields.field(1); std::string line(b.begin(),b.end()); auto off=line.find("GSV,"); unsigned total,part,count; if(off!=std::string::npos && sscanf(line.c_str()+off+4,"%u,%u,%u",&total,&part,&count)==3 && part==1) printf("satellites_in_view=%u\n",count); }
        else printf("indication=%04x bytes=%zu\n",id,fields.tlvs.size());
        fflush(stdout);
    })) { puts("LOC session rejected"); return 1; }
    puts("LOC session started"); fflush(stdout);
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(180);
    while(std::chrono::steady_clock::now()<end) loc.pollEvents();
    printf("reports=%u nmea=%u\n",positions,nmea);
}
