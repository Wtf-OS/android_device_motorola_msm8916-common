#pragma once
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <mutex>
#include <poll.h>
#include <unistd.h>
#include <vector>

// QMUX/CTL/NAS layouts: linux-mobile-broadband/libqmi data/qmi-service-{ctl,nas}.json.
struct QmiReply {
    bool valid=false;
    uint16_t error=0xffff;
    std::vector<uint8_t> tlvs;
    static uint16_t u16(const uint8_t* p) { return p[0] | (uint16_t(p[1])<<8); }
    std::vector<uint8_t> field(uint8_t type) const {
        for (size_t off=0; off<tlvs.size();) {
            if (tlvs.size()-off<3) return {};
            const auto size=u16(tlvs.data()+off+1);
            if (size>tlvs.size()-off-3) return {};
            if (tlvs[off]==type) return {tlvs.begin()+off+3, tlvs.begin()+off+3+size};
            off+=3+size;
        }
        return {};
    }
    static QmiReply parse(const std::vector<uint8_t>& tlvs) {
        QmiReply reply;
        unsigned results=0;
        for (size_t off=0; off<tlvs.size();) {
            if (tlvs.size()-off<3) return reply;
            const auto size=u16(tlvs.data()+off+1);
            if (size>tlvs.size()-off-3) return reply;
            if (tlvs[off]==2 && ++results>1) return reply;
            off+=3+size;
        }
        reply.tlvs=tlvs;
        const auto result=reply.field(2);
        if (result.size()!=4 || u16(result.data())>1 ||
            (u16(result.data())==1 && u16(result.data()+2)==0)) return reply;
        reply.error=u16(result.data()) ? u16(result.data()+2) : 0;
        reply.valid=true;
        return reply;
    }
};

class QmiBackend {
    int fd=-1;
    uint8_t nasClient=0;
    uint8_t uimClient=0;
    uint8_t ctlTransaction=0;
    uint16_t transaction=0;
    std::mutex mutex;
    QmiReply request(uint8_t service, uint8_t client, uint16_t message,
                     const std::vector<uint8_t>& tlvs={}) {
        if (fd<0 || tlvs.size()>1024) return {};
        uint16_t txn;
        if (service==0) { if (++ctlTransaction==0) ++ctlTransaction; txn=ctlTransaction; }
        else { if (++transaction==0) ++transaction; txn=transaction; }
        std::vector<uint8_t> packet={1,0,0,0,service,client,0,uint8_t(txn)};
        if (service) packet.push_back(txn>>8);
        packet.insert(packet.end(),{uint8_t(message),uint8_t(message>>8),uint8_t(tlvs.size()),uint8_t(tlvs.size()>>8)});
        packet.insert(packet.end(),tlvs.begin(),tlvs.end());
        const auto size=packet.size()-1;
        packet[1]=size; packet[2]=size>>8;
        if (write(fd,packet.data(),packet.size())!=ssize_t(packet.size())) return {};
        std::vector<uint8_t> incoming;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        size_t received=0;
        while (std::chrono::steady_clock::now()<deadline && received<65536) {
            pollfd pfd{fd,POLLIN,0};
            if (poll(&pfd,1,100)<0 && errno!=EINTR) return {};
            if (pfd.revents&(POLLERR|POLLHUP|POLLNVAL)) return {};
            if (!(pfd.revents&POLLIN)) continue;
            uint8_t buffer[4096];
            const auto n=read(fd,buffer,sizeof(buffer));
            if (n<0 && (errno==EINTR || errno==EAGAIN)) continue;
            if (n<=0) return {};
            received+=n;
            incoming.insert(incoming.end(),buffer,buffer+n);
            while (incoming.size()>=3) {
                const auto length=size_t(QmiReply::u16(incoming.data()+1))+1;
                if (incoming[0]!=1 || length<12 || length>8192) return {};
                if (incoming.size()<length) break;
                const bool ctl=incoming[4]==0;
                const size_t msgOffset=ctl ? 8 : 9;
                const size_t payload=msgOffset+4;
                if (length<payload || QmiReply::u16(incoming.data()+msgOffset+2)!=length-payload) return {};
                const auto rxTxn=ctl ? incoming[7] : QmiReply::u16(incoming.data()+7);
                if (incoming[3]==0x80 && incoming[4]==service && incoming[5]==client &&
                    incoming[6]==(ctl ? 1 : 2) && rxTxn==txn &&
                    QmiReply::u16(incoming.data()+msgOffset)==message) {
                    return QmiReply::parse({incoming.begin()+payload,incoming.begin()+length});
                }
                incoming.erase(incoming.begin(),incoming.begin()+length);
            }
        }
        return {};
    }
    void resetPort() {
        if (fd>=0) close(fd);
        fd=-1; nasClient=0; uimClient=0;
    }
    bool allocate(uint8_t service, uint8_t& client) {
        if (client) return true;
        if (fd<0) fd=open("/dev/wwan0qmi0",O_RDWR|O_NONBLOCK|O_CLOEXEC);
        const auto reply=request(0,0,0x22,{1,1,0,service});
        const auto cid=reply.field(1);
        if (!reply.valid || reply.error || cid.size()!=2 || cid[0]!=service || !cid[1]) {
            if (!reply.valid) resetPort();
            return false;
        }
        client=cid[1];
        return true;
    }
    QmiReply query(uint8_t service, uint8_t& client, uint16_t message) {
        std::lock_guard lock(mutex);
        if (!allocate(service,client)) return {};
        auto reply=request(service,client,message);
        // Reopen and reallocate on the next query after a transport failure.
        if (!reply.valid || reply.error==7) resetPort(); // QMI INVALID_CLIENT_ID after a modem restart.
        return reply;
    }
public:
    QmiBackend()=default;
    QmiBackend(const QmiBackend&)=delete;
    QmiBackend& operator=(const QmiBackend&)=delete;
    ~QmiBackend() { closePort(); }
    void closePort() {
        std::lock_guard lock(mutex);
        if (nasClient) request(0,0,0x23,{1,2,0,3,nasClient});
        if (uimClient) request(0,0,0x23,{1,2,0,11,uimClient});
        resetPort();
    }
    bool openPort() {
        std::lock_guard lock(mutex);
        return allocate(3,nasClient);
    }
    QmiReply nas(uint16_t message) {
        return query(3,nasClient,message);
    }
    QmiReply uim(uint16_t message) { return query(11,uimClient,message); }
};
