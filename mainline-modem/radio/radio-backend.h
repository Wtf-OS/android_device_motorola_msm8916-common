#pragma once
#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <aidl/android/hardware/radio/RadioResponseInfo.h>
#include <aidl/android/hardware/radio/modem/RadioState.h>
#include <chrono>
#include <cerrno>
#include <fcntl.h>
#include <mutex>
#include <poll.h>
#include <thread>
#include <unistd.h>

// shortcut: single AT channel, no SIM/data/call implementation yet.
class AtBackend {
    android::base::unique_fd fd;
    std::mutex mutex;
    std::string cachedRevision;
public:
    bool openPort() {
        for (int n=0; n<60; ++n) {
            fd.reset(open("/dev/wwan0at0", O_RDWR | O_NONBLOCK | O_CLOEXEC));
            if (fd.ok() && query("AT").find("OK") != std::string::npos) {
                query("ATE0");
                query("AT+CMEE=2");
                cachedRevision=readRevision();
                if (!cachedRevision.empty()) return true;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        LOG(ERROR) << "MPSS AT endpoint unavailable";
        return false;
    }
    std::string query(const std::string& command) {
        std::lock_guard lock(mutex);
        if (!fd.ok()) return {};
        char buffer[1024];
        while (read(fd.get(), buffer, sizeof(buffer)) > 0) {}
        const auto message=command+"\r";
        // WWAN AT writes are packets; never resend a partial command.
        if (write(fd.get(), message.data(), message.size()) != static_cast<ssize_t>(message.size())) return {};
        std::string result;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while (std::chrono::steady_clock::now()<deadline && result.size()<8192) {
            pollfd pfd{fd.get(), POLLIN, 0};
            const int ready=poll(&pfd, 1, 100);
            if (ready<0 && errno!=EINTR) return {};
            if (pfd.revents & (POLLERR|POLLHUP|POLLNVAL)) return {};
            if (!(pfd.revents & POLLIN)) continue;
            const auto n=read(fd.get(), buffer, sizeof(buffer));
            if (n>0) result.append(buffer, n);
            // This MPSS firmware returns CGMR's version line without a final OK.
            const auto version=result.find("MPSS.");
            if (command=="AT+CGMR" && version!=std::string::npos && result.find_first_of("\r\n", version)!=std::string::npos) return result;
            if (result.find("\nOK\r")!=std::string::npos || result.find("\nERROR\r")!=std::string::npos || result.find("+CME ERROR:")!=std::string::npos) return result;
        }
        return {};
    }
    std::string revision() const { return cachedRevision; }
    std::string readRevision() {
        auto value=query("AT+CGMR");
        if (value.empty()) return {};
        size_t start=0;
        while (start<value.size()) {
            const auto end=value.find_first_of("\r\n", start);
            const auto line=value.substr(start, end==std::string::npos ? end : end-start);
            if (line.starts_with("MPSS.")) return line;
            if (end==std::string::npos) break;
            start=end+1;
        }
        return {};
    }
    aidl::android::hardware::radio::modem::RadioState state() {
        using aidl::android::hardware::radio::modem::RadioState;
        auto result=query("AT+CFUN?");
        if (result.find("+CFUN: 1")!=std::string::npos) return RadioState::ON;
        if (result.find("+CFUN: 0")!=std::string::npos || result.find("+CFUN: 4")!=std::string::npos) return RadioState::OFF;
        return RadioState::UNAVAILABLE;
    }
};
static AtBackend backend;
