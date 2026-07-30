#pragma once

#include <string>

namespace ip_server {

/// Validate IPv4 address format (dotted decimal, e.g. "192.168.1.1").
bool is_valid_ipv4(const std::string& ip);

/// Validate IPv6 address format using getaddrinfo() for OS-authoritative parsing.
/// Handles all RFC 4291 formats including IPv4-mapped IPv6 (::ffff:x.x.x.x).
bool is_valid_ipv6(const std::string& ip);

/// Returns true if the string is a valid IPv4 or IPv6 address.
bool is_valid_ip_format(const std::string& ip);

/// Validate MAC address format — accepts "XX:XX:XX:XX:XX:XX" (17 chars),
/// "XX-XX-XX-XX-XX-XX" (17 chars), or "XXXXXXXXXXXX" (12 chars, hex only).
bool is_valid_mac_format(const std::string& mac);

}  // namespace ip_server
