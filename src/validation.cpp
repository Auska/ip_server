#include "validation.h"

#include <netdb.h>
#include <sys/socket.h>

#include <cstring>

namespace ip_server {

bool isValidIpv4(const std::string& ip) {
    int octets = 0, val = 0, digits = 0;
    for (char c : ip) {
        if (c == '.') {
            if (++octets > 3 || digits == 0) return false;
            val    = 0;
            digits = 0;
        } else if (c >= '0' && c <= '9') {
            val = val * 10 + (c - '0');
            if (val > 255) return false;
            digits++;
        } else {
            return false;
        }
    }
    return octets == 3 && digits > 0;
}

bool isValidIpv6(const std::string& ip) {
    if (ip.empty() || ip.find('%') != std::string::npos) {
        return false;
    }

    struct addrinfo hints{};
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_flags  = AI_NUMERICHOST;

    struct addrinfo* result = nullptr;
    int const ret           = getaddrinfo(ip.c_str(), nullptr, &hints, &result);
    if (ret != 0) {
        return false;
    }

    freeaddrinfo(result);
    return true;
}

bool isValidIpFormat(const std::string& ip) {
    return isValidIpv4(ip) || isValidIpv6(ip);
}

bool isValidMacFormat(const std::string& mac) {
    if (mac.size() == 17) {
        for (size_t i = 0; i < 17; ++i) {
            if (i % 3 == 2) {
                if (mac[i] != ':' && mac[i] != '-') return false;
            } else {
                if (!((mac[i] >= '0' && mac[i] <= '9') || (mac[i] >= 'a' && mac[i] <= 'f')
                      || (mac[i] >= 'A' && mac[i] <= 'F')))
                    return false;
            }
        }
        return true;
    }
    if (mac.size() == 12) {
        for (char c : mac) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                return false;
        }
        return true;
    }
    return false;
}

}  // namespace ip_server
