#pragma once
#include <cstdlib>
#include <string>

namespace rr::test {
inline std::string assets_root() {
    if (const char* root = std::getenv("RR_ASSETS"); root && *root) return root;
#ifdef RR_TEST_ASSETS_DIR
    return RR_TEST_ASSETS_DIR;
#else
    return RR_SOURCE_DIR "/../cdrom/ROADRASH";
#endif
}
inline std::string media_root() {
    if (const char* root = std::getenv("RR_TEST_MEDIA_ASSETS"); root && *root) return root;
    return assets_root();
}
inline std::string moving_fixture(const std::string& historical) {
    const char* configured = std::getenv("RR_TEST_MOVING_FIXTURE");
    return configured && *configured ? configured : historical;
}
inline std::string frontend_reference(const std::string& historical) {
    const char* configured = std::getenv("RR_TEST_FRONTEND_REFERENCE");
    std::string root = configured && *configured ? configured : historical;
    if (!root.empty() && root.back() != '/' && root.back() != '\\') root += '/';
    return root;
}
inline bool require_frontend_reference() {
    const char* configured = std::getenv("RR_TEST_FRONTEND_REFERENCE");
    const char* required = std::getenv("RR_REQUIRE_FRONTEND_REFERENCE");
    return (configured && *configured) || (required && std::string(required) == "1");
}
} // namespace rr::test
