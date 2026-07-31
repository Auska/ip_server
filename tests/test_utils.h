#pragma once

#include <filesystem>

namespace ip_server::test {

/// Resolve the project root directory from the test binary's working directory.
/// Handles common CWD values:
///   build/bin/   → build/bin/../../  (xmake test runner)
///   build/tests/ → build/tests/../../ (alternative config)
///   build/       → build/../         (some IDE runners)
///   else         → use as-is         (direct invocation from project root)
inline std::filesystem::path find_project_root() {
    std::filesystem::path cwd = std::filesystem::current_path();
    auto const parent = cwd.parent_path();

    if (cwd.filename() == "tests" && parent.filename() == "build")
        return parent.parent_path();
    if (cwd.filename() == "build")
        return parent;
    if (cwd.filename() == "bin" && parent.filename() == "build")
        return parent.parent_path();
    return cwd;
}

}  // namespace ip_server::test
