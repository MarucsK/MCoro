// #include <arpa/inet.h>
// #include <co_async/awaiter/task.hpp>
// #include <co_async/generic/cancel.hpp>
// #include <co_async/platform/error_handling.hpp>
// #include <co_async/platform/fs.hpp>
// #include <co_async/platform/platform_io.hpp>
// #include <co_async/platform/socket.hpp>
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

// const std::error_category &getAddrInfoCategory() {
//     static struct : std::error_category {
//         const char *name() const noexcept override {
//             return "getaddrinfo";
//         }

//         std::string message(int e) const override {
//             return gai_strerror(e);
//         }
//     } instance;

//     return instance;
// }

// SocketAddress::SocketAddress(const struct sockaddr *addr, socklen_t addrLen,
//                              sa_family_t family, int sockType, int protocol)
//     : mSockType(sockType),
//       mProtocol(protocol) {
//     std::memcpy(&mAddr, addr, addrLen);
//     mAddr.ss_family = family;
//     mAddrLen = addrLen;
// }

// std::string SocketAddress::host() const {
//     if (family() == AF_INET) {
//         auto &sin =
//             reinterpret_cast<struct sockaddr_in const &>(mAddr).sin_addr;
//         char buf[INET_ADDRSTRLEN] = {};
//         inet_ntop(family(), &sin, buf, sizeof(buf));
//         return buf;
//     } else if (family() == AF_INET6) {
//         auto &sin6 =
//             reinterpret_cast<struct sockaddr_in6 const &>(mAddr).sin6_addr;
//         char buf[INET6_ADDRSTRLEN] = {};
//         inet_ntop(AF_INET6, &sin6, buf, sizeof(buf));
//         return buf;
//     } else [[unlikely]] {
//         throw std::runtime_error("address family not ipv4 or ipv6");
//     }
// }

// int SocketAddress::port() const {
//     if (family() == AF_INET) {
//         auto port =
//             reinterpret_cast<const struct sockaddr_in &>(mAddr).sin_port;
//         return ntohs(port);
//     } else if (family() == AF_INET6) {
//         auto port =
//             reinterpret_cast<const struct sockaddr_in6 &>(mAddr).sin6_port;
//         return ntohs(port);
//     } else [[unlikely]] {
//         throw std::runtime_error("address family not ipv4 or ipv6");
//     }
// }

// void SocketAddress::trySetPort(int port) {
//     if (family() == AF_INET) {
//         reinterpret_cast<struct sockaddr_in &>(mAddr).sin_port =
//             htons(static_cast<uint16_t>(port));
//     } else if (family() == AF_INET6) {
//         reinterpret_cast<struct sockaddr_in6 &>(mAddr).sin6_port =
//             htons(static_cast<uint16_t>(port));
//     }
// }

// String SocketAddress::toString() const {
//     return String(host()) + ':' + to_string(port());
// }

// auto AddressResolver::resolve_all() -> Expected<ResolveResult> {
//     if (m_host.empty()) [[unlikely]] {
//         return std::errc::invalid_argument;
//     }

//     struct addrinfo *result;
//     int err = getaddrinfo(m_host.c_str(),
//                           m_service.empty() ? nullptr : m_service.c_str(),
//                           &m_hints, &result);
//     if (err) [[unlikely]] {
//         std::cerr << m_host << ": " << gai_strerror(err) << '\n';
//         return std::error_code(err, getAddrInfoCategory());
//     }

//     Finally fin = [&] {
//         freeaddrinfo(result);
//     };

//     ResolveResult res;
//     for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
//         res.addrs
//             .emplace_back(rp->ai_addr, rp->ai_addrlen, rp->ai_family,
//                           rp->ai_socktype, rp->ai_protocol)
//             .trySetPort(m_port);
//     }
//     if (res.addrs.empty()) [[unlikely]] {
//         std::cerr << m_host << ": no matching host address\n";
//         return std::errc::bad_address;
//     }
//     res.service = std::move(m_service);
//     return res;
// }

// Expected<SocketAddress> AddressResolver::resolve_one() {
//     auto res = resolve_all();
//     if (res.has_error()) [[unlikely]] {
//         return res.error();
//     }
//     return res->addrs.front();
// }

// Expected<SocketAddress> AddressResolver::resolve_one(std::string &service) {
//     auto res = resolve_all();
//     if (res.has_error()) [[unlikely]] {
//         return res.error();
//     }
//     service = std::move(res->service);
//     return res->addrs.front();
// }

// } // namespace Marcus
