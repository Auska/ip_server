#pragma once

#include <string>

namespace ip_server {

/// Returns true if the string is a valid IPv4 (inet_pton) or IPv6 address
/// (getaddrinfo, OS-authoritative; all RFC 4291 formats incl. IPv4-mapped).
bool isValidIpFormat(const std::string& ip);

/// Validate MAC address format — accepts "XX:XX:XX:XX:XX:XX" (17 chars),
/// "XX-XX-XX-XX-XX-XX" (17 chars), or "XXXXXXXXXXXX" (12 chars, hex only).
bool isValidMacFormat(const std::string& mac);

}  // namespace ip_server
