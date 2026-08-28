#include "validation.h"

#include <arpa/inet.h>
#include <netdb.h>

namespace ip_server {

bool isValidIpFormat(const std::string& ip) {
    struct in_addr addr {};
    if (inet_pton(AF_INET, ip.c_str(), &addr) == 1) {
        return true;
    }

    if (ip.empty() || ip.find('%') != std::string::npos) {
        return false;
    }

    struct addrinfo hints {};
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
