// #pragma once
// #include <co_async/std.hpp>
// #include <arpa/inet.h>
// #include <co_async/awaiter/task.hpp>
// #include <co_async/generic/cancel.hpp>
// #include <co_async/platform/error_handling.hpp>
// #include <co_async/platform/fs.hpp>
// #include <co_async/platform/platform_io.hpp>
// #include <co_async/utils/finally.hpp>
// #include <co_async/utils/string_utils.hpp>
// #include <netdb.h>
// #include <netinet/in.h>
// #include <netinet/tcp.h>
// #include <sys/socket.h>
// #include <sys/types.h>
// #include <sys/un.h>
// #include <unistd.h>

// namespace Marcus {

// const std::error_category &getAddrInfoCategory();

// struct SocketAddress {
//     SocketAddress() = default;

//     explicit SocketAddress(const struct sockaddr *addr, socklen_t addrLen,
//                            sa_family_t family, int sockType, int protocol);

//     struct sockaddr_storage mAddr;
//     socklen_t mAddrLen;
//     int mSockType; // SOCK_STREAM  SOCK_DGRAM
//     int mProtocol; // IPPROTO_TCP  IPPROTO_UDP

//     sa_family_t family() const noexcept {
//         return mAddr.ss_family; // AF_INET  AF_INET6
//     }

//     int socktype() const noexcept {
//         return mSockType;
//     }

//     int protocol() const noexcept {
//         return mProtocol;
//     }

//     std::string host() const;

//     int port() const;

//     void trySetPort(int port);

//     String toString() const;

//     auto repr() const {
//         return toString();
//     }
// };

// // 解析主机名和端口/服务名，转为SocketAddress对象
// struct AddressResolver {
// private:
//     std::string m_host;
//     int m_port = -1;
//     std::string m_service;
//     struct addrinfo m_hints{};

// public:
//     /*
//     https://www.google.com:443
//         host: www.google.com
//         service: https
//         port: 443
//     */
//     AddressResolver &host(std::string_view host) {
//         if (auto i = host.find("://"); i != host.npos) {
//             if (auto service = host.substr(0, i); !service.empty()) {
//                 m_service = service;
//             }
//             host.remove_prefix(i + 3);
//         }
//         if (auto i = host.rfind(':'); i != host.npos) {
//             if (auto portOpt = from_string<int>(host.substr(i + 1)))
//                 [[likely]] {
//                 m_port = *portOpt;
//                 host.remove_suffix(host.size() - i);
//             }
//         }
//         m_host = host;
//         return *this;
//     }

//     AddressResolver &port(int port) {
//         m_port = port;
//         return *this;
//     }

//     AddressResolver &service(std::string_view service) {
//         m_service = service;
//         return *this;
//     }

//     AddressResolver &family(int family) {
//         m_hints.ai_family = family;
//         return *this;
//     }

//     AddressResolver &socktype(int socktype) {
//         m_hints.ai_socktype = socktype;
//         return *this;
//     }

//     struct ResolveResult {
//         std::vector<SocketAddress> addrs;
//         std::string service;
//     };

//     Expected<ResolveResult> resolve_all();
//     Expected<SocketAddress> resolve_one();
//     Expected<SocketAddress> resolve_one(std::string &service);
// };

// struct [[nodiscard]] SocketHandle : FileHandle {
//     using FileHandle::FileHandle;
// };

// struct [[nodiscard]] SocketListener : SocketHandle {
//     using SocketHandle::SocketHandle;
// };

// } // namespace Marcus
