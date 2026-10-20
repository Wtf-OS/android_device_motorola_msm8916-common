#pragma once
#include "../radio/qmi-backend.h"
#include <array>
#include <cstring>
#include <functional>

// Wire layouts: libqmi data/qmi-service-loc.json. LOC indications have no result TLV.
class QmiLoc {
    int fd = -1;
    uint8_t client = 0, ctlTxn = 0;
    uint16_t txn = 0;
    std::vector<uint8_t> input;
    uint16_t awaiting=0;
    uint32_t commandStatus=0xffffffff;
    bool lost=false;
    std::function<void(uint16_t, const QmiReply&)> report;
    bool disconnect() { lost=true; input.clear(); if(fd>=0) ::close(fd); fd=-1; return false; }
    bool receive(int timeout, std::vector<uint8_t>& packet) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
        do {
            if (input.size() >= 3) {
                const size_t length = QmiReply::u16(input.data()+1)+1;
                if (input[0]!=1 || length<12 || length>8192) return disconnect();
                if (input.size()>=length) {
                    packet.assign(input.begin(), input.begin()+length);
                    input.erase(input.begin(), input.begin()+length);
                    return true;
                }
            }
            pollfd p{fd,POLLIN,0};
            if (poll(&p,1,100)<0 && errno!=EINTR) return disconnect();
            if (p.revents&(POLLERR|POLLHUP|POLLNVAL)) return disconnect();
            if (!(p.revents&POLLIN)) continue;
            uint8_t buffer[4096];
            const auto n=read(fd,buffer,sizeof(buffer));
            if (n<0 && (errno==EINTR || errno==EAGAIN)) continue;
            if (n<=0 || input.size()+n>16384) return disconnect();
            input.insert(input.end(),buffer,buffer+n);
        } while (std::chrono::steady_clock::now()<deadline);
        return false;
    }
    bool indication(const std::vector<uint8_t>& p) {
        if (p.size()<13 || p[3]!=0x80 || p[4]!=16 || p[5]!=client || p[6]!=4) return false;
        if (QmiReply::u16(p.data()+11)!=p.size()-13) return false;
        QmiReply fields;
        fields.tlvs.assign(p.begin()+13,p.end());
        if (!validTlvs(fields.tlvs)) return false;
        fields.valid=true;
        const auto id=QmiReply::u16(p.data()+9);
        if (id==awaiting) value(fields,1,commandStatus);
        else if (report) report(id,fields);
        return true;
    }
    bool command(uint16_t message,const std::vector<uint8_t>& tlvs) {
        awaiting=message; commandStatus=0xffffffff;
        bool ok=request(16,client,message,tlvs);
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while (ok && commandStatus==0xffffffff && std::chrono::steady_clock::now()<end) pollEvents();
        awaiting=0;
        return ok && commandStatus==0;
    }
    bool request(uint8_t service,uint8_t cid,uint16_t message,const std::vector<uint8_t>& tlvs,QmiReply* result=nullptr) {
        if (fd<0 || tlvs.size()>1024) return false;
        uint16_t id;
        if (service) { if(++txn==0) ++txn; id=txn; }
        else { if(++ctlTxn==0) ++ctlTxn; id=ctlTxn; }
        std::vector<uint8_t> p={1,0,0,0,service,cid,0,uint8_t(id)};
        if (service) p.push_back(id>>8);
        p.insert(p.end(),{uint8_t(message),uint8_t(message>>8),uint8_t(tlvs.size()),uint8_t(tlvs.size()>>8)});
        p.insert(p.end(),tlvs.begin(),tlvs.end());
        p[1]=p.size()-1; p[2]=(p.size()-1)>>8;
        if (write(fd,p.data(),p.size())!=ssize_t(p.size())) return false;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while (std::chrono::steady_clock::now()<deadline) {
            if (!receive(200,p)) { if(lost) return false; continue; }
            if (indication(p)) continue;
            const size_t off=service ? 9 : 8;
            if (p.size()<off+4 || p[3]!=0x80 || p[4]!=service || p[5]!=cid || p[6]!=(service ? 2 : 1)) continue;
            if ((service ? QmiReply::u16(p.data()+7) : p[7])!=id || QmiReply::u16(p.data()+off)!=message) continue;
            if (QmiReply::u16(p.data()+off+2)!=p.size()-off-4) return false;
            auto reply=QmiReply::parse({p.begin()+off+4,p.end()});
            if (result) *result=reply;
            return reply.valid && !reply.error;
        }
        return false;
    }
public:
    static bool validTlvs(const std::vector<uint8_t>& bytes) {
        std::array<bool,256> seen{};
        for (size_t off=0;off<bytes.size();) {
            if (bytes.size()-off<3 || seen[bytes[off]]) return false;
            seen[bytes[off]]=true;
            const auto length=QmiReply::u16(bytes.data()+off+1);
            if (length>bytes.size()-off-3) return false;
            off+=3+length;
        }
        return true;
    }
    template<class T> static bool value(const QmiReply& fields,uint8_t tag,T& out) {
        const auto bytes=fields.field(tag);
        if (bytes.size()!=sizeof(T)) return false;
        memcpy(&out,bytes.data(),sizeof(T)); // Both supported Android architectures are little endian.
        return true;
    }
    ~QmiLoc() { stop(); }
    bool start(uint32_t interval,std::function<void(uint16_t,const QmiReply&)> callback) {
        stop(); report=std::move(callback);
        fd=open("/dev/wwan0qmi0",O_RDWR|O_CLOEXEC|O_NONBLOCK);
        QmiReply reply;
        if (!request(0,0,0x22,{1,1,0,16},&reply)) { stop(); return false; }
        const auto cid=reply.field(1);
        if (cid.size()!=2 || cid[0]!=16 || !cid[1]) { stop(); return false; }
        client=cid[1];
        // Register position, satellite and NMEA events; standalone needs no SIM or SUPL server.
        if (!request(16,client,0x21,{1,8,0,7,0,0,0,0,0,0,0}) ||
            !command(0x3a,{1,4,0,3,0,0,0}) || // Allow local sessions, keep network-initiated positioning locked.
            !command(0x4a,{1,4,0,4,0,0,0}) ||
            !command(0x3e,{1,4,0,0x9f,0,0,0}) ||
            !request(16,client,0x22,{1,1,0,1,0x10,4,0,1,0,0,0,0x13,4,0,
                uint8_t(interval),uint8_t(interval>>8),uint8_t(interval>>16),uint8_t(interval>>24)})) {
            stop(); return false;
        }
        return true;
    }
    bool pollEvents() { std::vector<uint8_t> packet; if (receive(200,packet)) indication(packet); return !lost; }
    void stop() {
        report={};
        if (client) {
            request(16,client,0x23,{1,1,0,1});
            command(0x3a,{1,4,0,4,0,0,0});
            request(0,0,0x23,{1,2,0,16,client});
        }
        if (fd>=0) ::close(fd);
        fd=-1; client=0; input.clear(); lost=false;
    }
};
