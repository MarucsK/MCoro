#include <arpa/inet.h>
#include <co_async/awaiter/task.hpp>
#include <co_async/generic/cancel.hpp>
#include <co_async/platform/error_handling.hpp>
#include <co_async/platform/fs.hpp>
#include <co_async/platform/platform_io.hpp>
#include <co_async/platform/socket.hpp>
#include <co_async/utils/finally.hpp>
#include <co_async/utils/string_utils.hpp>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

namespace Marcus {

const std::error_category &getAddrInfoCategory() {
    static struct : std::error_category {
        const char *name() const noexcept override {
            return "getaddrinfo";
        }

        std::string message(int e) const override {
            return gai_strerror(e);
        }
    } instance;

    return instance;
}

SocketAddress::SocketAddress(const struct sockaddr *addr, socklen_t addrLen,
                             sa_family_t family, int sockType, int protocol)
    : mSockType(sockType),
      mProtocol(protocol) {
    std::memcpy(&mAddr, addr, addrLen);
    mAddr.ss_family = family;
    mAddrLen = addrLen;
}

std::string SocketAddress::host() const {
    if (family() == AF_INET) {
        auto &sin =
            reinterpret_cast<const struct sockaddr_in &>(mAddr).sin_addr;
        char buf[INET_ADDRSTRLEN] = {};
        inet_ntop(family(), &sin, buf, sizeof(buf));
        return buf;
    } else if (family() == AF_INET6) {
        auto &sin6 =
            reinterpret_cast<struct sockaddr_in6 const &>(mAddr).sin6_addr;
        char buf[INET6_ADDRSTRLEN] = {};
        inet_ntop(AF_INET6, &sin6, buf, sizeof(buf));
        return buf;
    } else [[unlikely]] {
        throw std::runtime_error("address family not ipv4 or ipv6");
    }
}

int SocketAddress::port() const {
    if (family() == AF_INET) {
        auto port =
            reinterpret_cast<const struct sockaddr_in &>(mAddr).sin_port;
        return ntohs(port);
    } else if (family() == AF_INET6) {
        auto port =
            reinterpret_cast<const struct sockaddr_in6 &>(mAddr).sin6_port;
        return ntohs(port);
    } else [[unlikely]] {
        throw std::runtime_error("address family not ipv4 or ipv6");
    }
}

void SocketAddress::trySetPort(int port) {
    if (family() == AF_INET) {
        reinterpret_cast<struct sockaddr_in &>(mAddr).sin_port =
            htons(static_cast<uint16_t>(port));
    } else if (family() == AF_INET6) {
        reinterpret_cast<struct sockaddr_in6 &>(mAddr).sin6_port =
            htons(static_cast<uint16_t>(port));
    }
}

String SocketAddress::toString() const {
    return String(host()) + ':' + to_string(port());
}

auto AddressResolver::resolve_all() -> Expected<ResolveResult> {
    if (m_host.empty()) [[unlikely]] {
        return std::errc::invalid_argument;
    }

    struct addrinfo *result;
    int err = getaddrinfo(m_host.c_str(),
                          m_service.empty() ? nullptr : m_service.c_str(),
                          &m_hints, &result);
    if (err) [[unlikely]] {
        std::cerr << m_host << ": " << gai_strerror(err) << '\n';
        return std::error_code(err, getAddrInfoCategory());
    }

    Finally fin = [&] {
        freeaddrinfo(result);
    };

    ResolveResult res;
    for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
        res.addrs
            .emplace_back(rp->ai_addr, rp->ai_addrlen, rp->ai_family,
                          rp->ai_socktype, rp->ai_protocol)
            .trySetPort(m_port);
    }
    if (res.addrs.empty()) [[unlikely]] {
        std::cerr << m_host << ": no matching host address\n";
        return std::errc::bad_address;
    }
    res.service = std::move(m_service);
    return res;
}

Expected<SocketAddress> AddressResolver::resolve_one() {
    auto res = resolve_all();
    if (res.has_error()) [[unlikely]] {
        return res.error();
    }
    return res->addrs.front();
}

Expected<SocketAddress> AddressResolver::resolve_one(std::string &service) {
    auto res = resolve_all();
    if (res.has_error()) [[unlikely]] {
        return res.error();
    }
    service = std::move(res->service);
    return res->addrs.front();
}

SocketAddress get_socket_address(SocketHandle &sock) {
    SocketAddress sa;
    sa.mAddrLen = sizeof(sa.mAddr);
    throwingErrorErrno(getsockname(
        sock.fileNo(), reinterpret_cast<struct sockaddr *>(&sa.mAddr),
        &sa.mAddrLen));
    return sa;
}

SocketAddress get_socket_peer_address(SocketHandle &sock) {
    SocketAddress sa;
    sa.mAddrLen = sizeof(sa.mAddr);
    throwingErrorErrno(getpeername(
        sock.fileNo(), reinterpret_cast<struct sockaddr *>(&sa.mAddr),
        &sa.mAddrLen));
    return sa;
}

Task<Expected<SocketHandle>> createSocket(int family, int type, int protocol) {
    int fd = co_await expectError(
                 co_await UringOp().prep_socket(family, type, protocol, 0))
                 .or_else(std::errc::invalid_argument,
                          [&] { return socket(family, type, protocol); });
    SocketHandle sock(fd);
    co_return sock;
}

Task<Expected<SocketHandle>> socket_connect(const SocketAddress &addr) {
    SocketHandle sock = co_await co_await createSocket(
        addr.family(), addr.socktype(), addr.protocol());
    co_await expectError(
        co_await UringOp().prep_connect(
            sock.fileNo(),
            reinterpret_cast<const struct sockaddr *>(&addr.mAddr),
            addr.mAddrLen))
        .or_else(std::errc::invalid_argument, [&] {
            return connect(
                sock.fileNo(),
                reinterpret_cast<const struct sockaddr *>(&addr.mAddr),
                addr.mAddrLen);
        });
    co_return sock;
}

Task<Expected<SocketHandle>>
socket_connect(const SocketAddress &addr,
               std::chrono::steady_clock::duration timeout) {
    SocketHandle sock = co_await co_await createSocket(
        addr.family(), addr.socktype(), addr.protocol());
    auto ts = durationToKernelTimespec(timeout);
    co_await expectError(
        co_await UringOp::link_ops(
            UringOp().prep_connect(
                sock.fileNo(),
                reinterpret_cast<const struct sockaddr *>(&addr.mAddr),
                addr.mAddrLen),
            UringOp().prep_link_timeout(&ts, IORING_TIMEOUT_BOOTTIME)))
        .or_else(std::errc::invalid_argument, [&] {
            return connect(
                sock.fileNo(),
                reinterpret_cast<const struct sockaddr *>(&addr.mAddr),
                addr.mAddrLen);
        });
}

Task<Expected<SocketHandle>> socket_connect(const SocketAddress &addr,
                                            CancelToken cancel) {
    SocketHandle sock =
        co_await co_await createSocket(addr.family(), SOCK_STREAM, 0);
    if (cancel.is_canceled()) [[unlikely]] {
        co_return std::errc::operation_canceled;
    }
    co_await expectError(
        co_await UringOp()
            .prep_connect(
                sock.fileNo(),
                reinterpret_cast<const struct sockaddr *>(&addr.mAddr),
                addr.mAddrLen)
            .cancelGuard(cancel))
        .or_else(std::errc::invalid_argument, [&] {
            return connect(
                sock.fileNo(),
                reinterpret_cast<const struct sockaddr *>(&addr.mAddr),
                addr.mAddrLen);
        });
}

Task<Expected<SocketListener>> listener_bind(const SocketAddress &addr,
                                             int backlog) {
    SocketHandle sock =
        co_await co_await createSocket(addr.family(), SOCK_STREAM, 0);
    co_await socketSetOption(sock, SOL_SOCKET, SO_REUSEADDR, 1);
    co_await socketSetOption(sock, SOL_SOCKET, SO_REUSEPORT, 1);
    SocketListener serv(sock.releaseFile());
    co_await expectError(bind(
        serv.fileNo(), reinterpret_cast<const struct sockaddr *>(&addr.mAddr),
        addr.mAddrLen));
    co_await expectError(listen(serv.fileNo(), backlog));
    co_return serv;
}

Task<Expected<SocketHandle>> listener_accept(SocketListener &listener) {
    int fd = co_await expectError(
        co_await UringOp().prep_accept(listener.fileNo(), nullptr, nullptr, 0));
    SocketHandle sock(fd);
    co_return sock;
}

Task<Expected<SocketHandle>> listener_accept(SocketListener &listener,
                                             CancelToken cancel) {
    int fd = co_await expectError(
        co_await UringOp()
            .prep_accept(listener.fileNo(), nullptr, nullptr, 0)
            .cancelGuard(cancel));
    SocketHandle sock(fd);
    co_return sock;
}

Task<Expected<SocketHandle>> listener_accept(SocketListener &listener,
                                             SocketAddress &peerAddr) {
    int fd = co_await expectError(
                 co_await UringOp().prep_accept(
                     listener.fileNo(),
                     reinterpret_cast<struct sockaddr *>(&peerAddr.mAddr),
                     &peerAddr.mAddrLen, 0))
                 .or_else(std::errc::invalid_argument, [&] {
                     return expectError(accept4(
                         listener.fileNo(),
                         reinterpret_cast<struct sockaddr *>(&peerAddr.mAddr),
                         &peerAddr.mAddrLen, 0));
                 });
    SocketHandle sock(fd);
    co_return sock;
}

Task<Expected<SocketHandle>> listener_accept(SocketListener &listener,
                                             SocketAddress &peerAddr,
                                             CancelToken cancel) {
    int fd = co_await expectError(
                 co_await UringOp()
                     .prep_accept(
                         listener.fileNo(),
                         reinterpret_cast<struct sockaddr *>(&peerAddr.mAddr),
                         &peerAddr.mAddrLen, 0)
                     .cancelGuard(cancel))
                 .or_else(std::errc::invalid_argument, [&] {
                     return expectError(accept4(
                         listener.fileNo(),
                         reinterpret_cast<struct sockaddr *>(&peerAddr.mAddr),
                         &peerAddr.mAddrLen, 0));
                 });
    SocketHandle sock(fd);
    co_return sock;
}

Task<Expected<std::size_t>> socket_write(SocketHandle &sock,
                                         std::span<const char> buf) {
    co_return static_cast<std::size_t>(
        co_await expectError(
            co_await UringOp().prep_send(sock.fileNo(), buf, 0))
            .or_else(std::errc::invalid_argument, [&] {
                return expectError(
                    send(sock.fileNo(), buf.data(), buf.size(), 0));
            }));
}

Task<Expected<std::size_t>> socket_write(SocketHandle &sock,
                                         std::span<const char> buf,
                                         CancelToken cancel) {
    co_return static_cast<std::size_t>(
        co_await expectError(co_await UringOp()
                                 .prep_send(sock.fileNo(), buf, 0)
                                 .cancelGuard(cancel))
            .or_else(std::errc::invalid_argument, [&] {
                return expectError(
                    send(sock.fileNo(), buf.data(), buf.size(), 0));
            }));
}

Task<Expected<std::size_t>>
socket_write(SocketHandle &sock, std::span<const char> buf,
             std::chrono::steady_clock::duration timeout) {
    auto ts = durationToKernelTimespec(timeout);
    co_return static_cast<std::size_t>(
        co_await expectError(
            co_await UringOp::link_ops(
                UringOp().prep_send(sock.fileNo(), buf, 0),
                UringOp().prep_link_timeout(&ts, IORING_TIMEOUT_BOOTTIME)))
            .or_else(std::errc::invalid_argument, [&] {
                return expectError(
                    send(sock.fileNo(), buf.data(), buf.size(), 0));
            }));
}

Task<Expected<std::size_t>>
socket_write(SocketHandle &sock, std::span<const char> buf,
             std::chrono::steady_clock::duration timeout, CancelToken cancel) {
    auto ts = durationToKernelTimespec(timeout);
    co_return static_cast<std::size_t>(
        co_await expectError(
            co_await UringOp::link_ops(
                UringOp().prep_send(sock.fileNo(), buf, 0),
                UringOp().prep_link_timeout(&ts, IORING_TIMEOUT_BOOTTIME))
                .cancelGuard(cancel))
            .or_else(std::errc::invalid_argument, [&] {
                return expectError(
                    send(sock.fileNo(), buf.data(), buf.size(), 0));
            }));
}

Task<Expected<std::size_t>> socket_write_zc(SocketHandle &sock,
                                            std::span<const char> buf) {
    co_return static_cast<std::size_t>(
        co_await expectError(
            co_await UringOp().prep_send_zc(sock.fileNo(), buf, 0, 0))
            .or_else(std::errc::invalid_argument, [&] {
                return expectError(
                    send(sock.fileNo(), buf.data(), buf.size(), 0));
            }));
}

Task<Expected<std::size_t>> socket_write_zc(SocketHandle &sock,
                                            std::span<const char> buf,
                                            CancelToken cancel) {
    co_return static_cast<std::size_t>(
        co_await expectError(co_await UringOp()
                                 .prep_send_zc(sock.fileNo(), buf, 0, 0)
                                 .cancelGuard(cancel))
            .or_else(std::errc::invalid_argument, [&] {
                return expectError(
                    send(sock.fileNo(), buf.data(), buf.size(), 0));
            }));
}

Task<Expected<std::size_t>> socket_read(SocketHandle &sock,
                                        std::span<char> buf) {
    co_return static_cast<std::size_t>(
        co_await expectError(
            co_await UringOp().prep_recv(sock.fileNo(), buf, 0))
            .or_else(std::errc::invalid_argument, [&] {
                return expectError(
                    recv(sock.fileNo(), buf.data(), buf.size(), 0));
            }));
}

Task<Expected<std::size_t>> socket_read(SocketHandle &sock, std::span<char> buf,
                                        CancelToken cancel) {
    co_return static_cast<std::size_t>(
        co_await expectError(co_await UringOp()
                                 .prep_recv(sock.fileNo(), buf, 0)
                                 .cancelGuard(cancel))
            .or_else(std::errc::invalid_argument, [&] {
                return expectError(
                    recv(sock.fileNo(), buf.data(), buf.size(), 0));
            }));
}

Task<Expected<std::size_t>>
socket_read(SocketHandle &sock, std::span<char> buf,
            std::chrono::steady_clock::duration timeout) {
    auto ts = durationToKernelTimespec(timeout);
    co_return static_cast<std::size_t>(
        co_await expectError(
            co_await UringOp::link_ops(
                UringOp().prep_recv(sock.fileNo(), buf, 0),
                UringOp().prep_link_timeout(&ts, IORING_TIMEOUT_BOOTTIME)))
            .or_else(std::errc::invalid_argument, [&] {
                return expectError(
                    recv(sock.fileNo(), buf.data(), buf.size(), 0));
            }));
}

Task<Expected<std::size_t>>
socket_read(SocketHandle &sock, std::span<char> buf,
            std::chrono::steady_clock::duration timeout, CancelToken cancel) {
    auto ts = durationToKernelTimespec(timeout);
    co_return static_cast<std::size_t>(
        co_await expectError(
            co_await UringOp::link_ops(
                UringOp().prep_recv(sock.fileNo(), buf, 0),
                UringOp().prep_link_timeout(&ts, IORING_TIMEOUT_BOOTTIME))
                .cancelGuard(cancel))
            .or_else(std::errc::invalid_argument, [&] {
                return expectError(
                    recv(sock.fileNo(), buf.data(), buf.size(), 0));
            }));
}

Task<Expected<>> socket_shutdown(SocketHandle &sock, int how) {
    co_return expectError(co_await UringOp().prep_shutdown(sock.fileNo(), how));
}

} // namespace Marcus
