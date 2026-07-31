#pragma once

#include <string>

namespace ip_server {

/// Validate IPv4 address format (dotted decimal, e.g. "192.168.1.1").
bool isValidIpv4(const std::string& ip);

/// Validate IPv6 address format using getaddrinfo() for OS-authoritative parsing.
/// Handles all RFC 4291 formats including IPv4-mapped IPv6 (::ffff:x.x.x.x).
bool isValidIpv6(const std::string& ip);

/// Returns true if the string is a valid IPv4 or IPv6 address.
bool isValidIpFormat(const std::string& ip);

/// Validate MAC address format — accepts "XX:XX:XX:XX:XX:XX" (17 chars),
/// "XX-XX-XX-XX-XX-XX" (17 chars), or "XXXXXXXXXXXX" (12 chars, hex only).
bool isValidMacFormat(const std::string& mac);

}  // namespace ip_server
