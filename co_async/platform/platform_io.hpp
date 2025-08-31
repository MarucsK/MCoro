#pragma once

#include <co_async/std.hpp>
#include <co_async/awaiter/task.hpp>
#include <co_async/generic/generic_io.hpp>
#include <co_async/platform/error_handling.hpp>
#include <fcntl.h>
#include <liburing.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace Marcus {

template <typename Rep, typename Period>
struct __kernel_timespec
durationToKernelTimespec(std::chrono::duration<Rep, Period> dur) {
    struct __kernel_timespec ts;
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur);
    auto nsecs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(dur - secs);
    ts.tv_sec = static_cast<__kernel_time64_t>(secs.count());
    ts.tv_nsec = static_cast<long long>(nsecs.count());
    return ts;
}

template <typename Clk, typename Dur>
struct __kernel_timespec
timePointToKernelTimespec(std::chrono::time_point<Clk, Dur> tp) {
    return durationToKernelTimespec(tp.time_since_epoch());
}

struct PlatformIOContext {
    [[gnu::cold]] static void schedSetThreadAffinity(size_t cpu);

    struct IOUringProbe {
        struct io_uring_probe *mProbe;
        struct io_uring *mRing;

        [[gnu::cold]] IOUringProbe();
        IOUringProbe(IOUringProbe &&) = delete;
        [[gnu::cold]] ~IOUringProbe();
        [[gnu::cold]] bool isSupported(int op) noexcept;
        [[gnu::cold]] void dumpDiagnostics();
    };

    [[gnu::hot]] bool
    waitEventsFor(std::optional<std::chrono::steady_clock::duration> timeout);

    /* Reserve a free SQE from SQ */
    [[gnu::hot]] struct io_uring_sqe *getSqe() {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&mRing);
        while (!sqe) {
            int res = io_uring_submit(&mRing);
            if (res < 0) [[unlikely]] {
                if (res == -EINTR) {
                    continue;
                }
                throw std::system_error(-res, std::system_category());
            }
            sqe = io_uring_get_sqe(&mRing);
        }
        ++mNumSqesPending;
        return sqe;
    }

    PlatformIOContext &operator=(PlatformIOContext &&) = delete;

    [[gnu::cold]] PlatformIOContext() noexcept;
    [[gnu::cold]] void setup(std::size_t entries);
    [[gnu::cold]] ~PlatformIOContext();

    static thread_local PlatformIOContext *instance;

    void reserveBuffers(std::size_t nbufs);
    std::size_t addBuffers(std::span<const std::span<char>> bufs);
    void reserveFiles(std::size_t nfiles);
    std::size_t addFiles(std::span<const int> files);

    std::size_t hasPendingEvents() const noexcept {
        return mNumSqesPending != 0;
    }

private:
    struct io_uring mRing;
    std::size_t mNumSqesPending =
        0; // Number of uncompleted SQEs (getting an SQE represents an attempt
           // to initiate an I/O request)
    std::unique_ptr<struct iovec[]> mBuffers;
    unsigned int mNumBufs = 0;
    unsigned int mCapBufs = 0;
    std::unique_ptr<int[]> mFiles;
    unsigned int mNumFiles = 0;
    unsigned int mCapFiles = 0;
};

struct [[nodiscard]] UringOp {
    UringOp() {
        mSqe = PlatformIOContext::instance->getSqe();
        io_uring_sqe_set_data(mSqe, this);
    }

    UringOp(UringOp &&) = delete;

    struct Awaiter {
        bool await_ready() const noexcept {
            return false;
        }

        void await_suspend(std::coroutine_handle<> coroutine) noexcept {
            mOp->mPrevious = coroutine;
            mOp->mRes = -ENOSYS;
        }

        int await_resume() const noexcept {
            return mOp->mRes;
        }

        UringOp *mOp;
    };

    Awaiter operator co_await() {
        return Awaiter{this};
    }

    /* Link io_uring operations, execute sequentially */
    static UringOp &&link_ops(UringOp &&lhs, UringOp &&rhs) {
        lhs.mSqe->flags |= IOSQE_IO_LINK;
        rhs.mPrevious = std::noop_coroutine();
        return std::move(lhs);
    }

    struct io_uring_sqe *getSqe() const noexcept {
        return mSqe;
    }

private:
    std::coroutine_handle<> mPrevious;

    union {
        int mRes;
        struct io_uring_sqe *mSqe;
    };

    friend PlatformIOContext;

    struct DoNotConstruct {};

    explicit UringOp(DoNotConstruct) {}

public:
    /* Bind mSqe to detachedOp.
    Start a detached io_uring operation whose completion will not resume any
    coroutine.*/
    void startDetach() {
        static thread_local UringOp detachedOp{DoNotConstruct{}};
        detachedOp.mPrevious = std::noop_coroutine();
        io_uring_sqe_set_data(mSqe, &detachedOp);
    }

    /* No-op request. */
    UringOp &&prep_nop() && {
        io_uring_prep_nop(mSqe);
        return std::move(*this);
    }

    /* Asynchronously open a file or directory. */
    UringOp &&prep_openat(int dirfd, const char *path, int flags,
                          mode_t mode) && {
        io_uring_prep_openat(mSqe, dirfd, path, flags, mode);
        return std::move(*this);
    }

    /* Asynchronously open a file and directly store its file descriptor into
     * the io_uring registered file descriptor array at the specified index
     * file_index. */
    UringOp &&prep_openat_direct(int dirfd, const char *path, int flags,
                                 mode_t mode, unsigned int file_index) && {
        io_uring_prep_openat_direct(mSqe, dirfd, path, flags, mode, file_index);
        return std::move(*this);
    }

    /* Asynchronously create a socket. Similar to the synchronous
     * socket() system call. */
    UringOp &&prep_socket(int domain, int type, int protocol,
                          unsigned int flags) && {
        io_uring_prep_socket(mSqe, domain, type, protocol, flags);
        return std::move(*this);
    }

    /* Asynchronously create a socket and directly store its file descriptor
     * into the io_uring registered file descriptor array at the specified index
     * file_index.*/
    UringOp &&prep_socket_direct(int domain, int type, int protocol,
                                 unsigned int flags,
                                 unsigned int file_index) && {
        io_uring_prep_socket_direct(mSqe, domain, type, protocol, flags,
                                    file_index);
        return std::move(*this);
    }

    /* Asynchronously accept an incoming connection. Similar to the
     * synchronous accept4() system call. */
    UringOp &&prep_accept(int fd, struct sockaddr *addr, socklen_t *addrlen,
                          int flags) && {
        io_uring_prep_accept(mSqe, fd, addr, addrlen, flags);
        return std::move(*this);
    }

    /* Asynchronously accept an incoming connection and directly store its newly
     * created socket file descriptor into the io_uring registered file
     * descriptor array at the specified index file_index. */
    UringOp &&prep_accept_direct(int fd, struct sockaddr *addr,
                                 socklen_t *addrlen, int flags,
                                 unsigned int file_index) && {
        io_uring_prep_accept_direct(mSqe, fd, addr, addrlen, flags, file_index);
        return std::move(*this);
    }

    /* Asynchronously connect to a remote socket. Similar to the
     * synchronous connect() system call. */
    UringOp &&prep_connect(int fd, const struct sockaddr *addr,
                           socklen_t addrlen) && {
        io_uring_prep_connect(mSqe, fd, addr, addrlen);
        return std::move(*this);
    }

    /* Asynchronously create a directory. Similar to the synchronous
     * mkdirat() system call. */
    UringOp &&prep_mkdirat(int dirfd, const char *path, mode_t mode) && {
        io_uring_prep_mkdirat(mSqe, dirfd, path, mode);
        return std::move(*this);
    }

    /* Asynchronously create a hard link. Similar to the synchronous
     * linkat() system call. */
    UringOp &&prep_linkat(int olddirfd, const char *oldpath, int newdirfd,
                          const char *newpath, int flags) && {
        io_uring_prep_linkat(mSqe, olddirfd, oldpath, newdirfd, newpath, flags);
        return std::move(*this);
    }

    /* Asynchronously rename or move a file. Similar to the synchronous
     * renameat() system call. */
    UringOp &&prep_renameat(int olddirfd, const char *oldpath, int newdirfd,
                            const char *newpath, unsigned int flags) && {
        io_uring_prep_renameat(mSqe, olddirfd, oldpath, newdirfd, newpath,
                               flags);
        return std::move(*this);
    }

    /* Asynchronously delete a file or directory. Similar to the
     * synchronous unlinkat() system call. */
    UringOp &&prep_unlinkat(int dirfd, const char *path, int flags = 0) && {
        io_uring_prep_unlinkat(mSqe, dirfd, path, flags);
        return std::move(*this);
    }

    /* Asynchronously create a symbolic link. Similar to the synchronous
     * symlinkat() system call. */
    UringOp &&prep_symlinkat(const char *target, int newdirfd,
                             const char *linkpath) && {
        io_uring_prep_symlinkat(mSqe, target, newdirfd, linkpath);
        return std::move(*this);
    }

    /* Asynchronously retrieve file status information. Similar to the
     * synchronous statx() system call. */
    UringOp &&prep_statx(int dirfd, const char *path, int flags,
                         unsigned int mask, struct statx *statxbuf) && {
        io_uring_prep_statx(mSqe, dirfd, path, flags, mask, statxbuf);
        return std::move(*this);
    }

    /* Asynchronously read data from file descriptor fd into buf. */
    UringOp &&prep_read(int fd, std::span<char> buf, std::uint64_t offset) && {
        io_uring_prep_read(mSqe, fd, buf.data(),
                           static_cast<unsigned int>(buf.size()), offset);
        return std::move(*this);
    }

    /* Asynchronously write data from buf to file descriptor fd. */
    UringOp &&prep_write(int fd, std::span<const char> buf,
                         std::uint64_t offset) && {
        io_uring_prep_write(mSqe, fd, buf.data(),
                            static_cast<unsigned int>(buf.size()), offset);
        return std::move(*this);
    }

    /* Asynchronously read data from file descriptor fd into the buffer
     * corresponding to buf_index in the registered fixed buffers. */
    UringOp &&prep_read_fixed(int fd, std::span<char> buf, std::uint64_t offset,
                              int buf_index) && {
        io_uring_prep_read_fixed(mSqe, fd, buf.data(),
                                 static_cast<unsigned int>(buf.size()), offset,
                                 buf_index);
        return std::move(*this);
    }

    /* Asynchronously write data from the buffer corresponding to buf_index in
     * the registered fixed buffers to file descriptor fd. */
    UringOp &&prep_write_fixed(int fd, std::span<const char> buf,
                               std::uint64_t offset, int buf_index) && {
        io_uring_prep_write_fixed(mSqe, fd, buf.data(),
                                  static_cast<unsigned int>(buf.size()), offset,
                                  buf_index);
        return std::move(*this);
    }

    /* Asynchronously perform a scatter-read operation. Read data from file
     * descriptor fd into multiple discontinuous buffers (iovec array). Similar
     * to the synchronous preadv2() system call. */
    UringOp &&prep_readv(int fd, std::span<struct iovec const> buf,
                         std::uint64_t offset, int flags) && {
        io_uring_prep_readv2(mSqe, fd, buf.data(),
                             static_cast<unsigned int>(buf.size()), offset,
                             flags);
        return std::move(*this);
    }

    /* Asynchronously perform a gather-write operation. Write data from multiple
     * discontinuous buffers (iovec array) to file descriptor fd. Similar to the
     * synchronous pwritev2() system call. */
    UringOp &&prep_writev(int fd, std::span<struct iovec const> buf,
                          std::uint64_t offset, int flags) && {
        io_uring_prep_writev2(mSqe, fd, buf.data(),
                              static_cast<unsigned int>(buf.size()), offset,
                              flags);
        return std::move(*this);
    }

    /* Asynchronously receive data from socket fd into buf. Similar to
     * the synchronous recv() system call. */
    UringOp &&prep_recv(int fd, std::span<char> buf, int flags) && {
        io_uring_prep_recv(mSqe, fd, buf.data(), buf.size(), flags);
        return std::move(*this);
    }

    /* Asynchronously send data from buf to socket fd. Similar to the
     * synchronous send() system call. */
    UringOp &&prep_send(int fd, std::span<const char> buf, int flags) && {
        io_uring_prep_send(mSqe, fd, buf.data(), buf.size(), flags);
        return std::move(*this);
    }

    /* Asynchronously perform a "zero-copy" send operation. Attempt to send data
     * directly from the user buffer, avoiding data copying between kernel and
     * user space. */
    UringOp &&prep_send_zc(int fd, std::span<const char> buf, int flags,
                           unsigned int zc_flags) && {
        io_uring_prep_send_zc(mSqe, fd, buf.data(), buf.size(), flags,
                              zc_flags);
        return std::move(*this);
    }

    /* Asynchronously perform a "zero-copy" send operation using registered
     * fixed buffers. */
    UringOp &&prep_send_zc_fixed(int fd, std::span<const char> buf, int flags,
                                 unsigned int zc_flags,
                                 unsigned int buf_index) && {
        io_uring_prep_send_zc_fixed(mSqe, fd, buf.data(), buf.size(), flags,
                                    zc_flags, buf_index);
        return std::move(*this);
    }

    /* Asynchronously receive messages from socket fd. Similar to the
     * synchronous recvmsg() system call. */
    UringOp &&prep_recvmsg(int fd, struct msghdr *msg, unsigned int flags) && {
        io_uring_prep_recvmsg(mSqe, fd, msg, flags);
        return std::move(*this);
    }

    /* Asynchronously send messages to socket fd. Similar to the
     * synchronous sendmsg() system call. */
    UringOp &&prep_sendmsg(int fd, struct msghdr *msg, unsigned int flags) && {
        io_uring_prep_sendmsg(mSqe, fd, msg, flags);
        return std::move(*this);
    }

    /* Asynchronously close a file descriptor. Similar to the
     * synchronous close() system call. */
    UringOp &&prep_close(int fd) && {
        io_uring_prep_close(mSqe, fd);
        return std::move(*this);
    }

    /* Asynchronously shut down the read/write of a socket. Similar to
     * the synchronous shutdown() system call. */
    UringOp &&prep_shutdown(int fd, int how) && {
        io_uring_prep_shutdown(mSqe, fd, how);
        return std::move(*this);
    }

    /* Asynchronously synchronize file data and metadata to disk. Similar to the
     * synchronous fsync() or fdatasync() system call. */
    /* If flags are set to IORING_FSYNC_DATASYNC, it's equivalent to
     * fdatasync; otherwise, it's fsync. */
    UringOp &&prep_fsync(int fd, unsigned int flags) && {
        io_uring_prep_fsync(mSqe, fd, flags);
        return std::move(*this);
    }

    /* Asynchronously truncate or extend a file to a specified length. Similar
     * to the synchronous ftruncate() system call. */
    UringOp &&prep_ftruncate(int fd, loff_t len) && {
        io_uring_prep_ftruncate(mSqe, fd, len);
        return std::move(*this);
    }

    /* Asynchronously cancel a previously submitted io_uring operation. */
    UringOp &&prep_cancel(UringOp *op, int flags) && {
        io_uring_prep_cancel(mSqe, op, flags);
        return std::move(*this);
    }

    /* Asynchronously cancel all io_uring operations associated with a specific
     * file descriptor fd. */
    UringOp &&prep_cancel_fd(int fd, unsigned int flags) && {
        io_uring_prep_cancel_fd(mSqe, fd, flags);
        return std::move(*this);
    }

    /* Asynchronously wait for child process state changes. Similar to
     * the synchronous waitid() system call. */
    UringOp &&prep_waitid(idtype_t idtype, id_t id, siginfo_t *infop,
                          int options, unsigned int flags) && {
        io_uring_prep_waitid(mSqe, idtype, id, infop, options, flags);
        return std::move(*this);
    }

    /* Asynchronously set a timeout timer. When the timer expires, a CQE will be
     * generated. */
    UringOp &&prep_timeout(struct __kernel_timespec *ts, unsigned int count,
                           unsigned int flags) && {
        io_uring_prep_timeout(mSqe, ts, count, flags);
        return std::move(*this);
    }

    /* Set a timeout for a linked io_uring operation chain. If any operation in
     * the chain does not complete within the specified time, the entire chain
     * will be cancelled.*/
    UringOp &&prep_link_timeout(struct __kernel_timespec *ts,
                                unsigned int flags) && {
        io_uring_prep_link_timeout(mSqe, ts, flags);
        return std::move(*this);
    }

    /* Asynchronously update the timeout for an existing io_uring timeout
     * operation. */
    UringOp &&prep_timeout_update(UringOp *op, struct __kernel_timespec *ts,
                                  unsigned int flags) && {
        io_uring_prep_timeout_update(
            mSqe, ts, reinterpret_cast<std::uintptr_t>(op), flags);
        return std::move(*this);
    }

    /* Asynchronously remove an existing io_uring timeout operation. */
    UringOp &&prep_timeout_remove(UringOp *op, unsigned int flags) && {
        io_uring_prep_timeout_remove(mSqe, reinterpret_cast<std::uintptr_t>(op),
                                     flags);
        return std::move(*this);
    }

    /* Asynchronously move data between two file descriptors with zero-copy,
     * similar to the synchronous splice() system call. */
    UringOp &&prep_splice(int fd_in, std::int64_t off_in, int fd_out,
                          std::int64_t off_out, std::size_t nbytes,
                          unsigned int flags) && {
        io_uring_prep_splice(mSqe, fd_in, off_in, fd_out, off_out,
                             static_cast<unsigned int>(nbytes), flags);
        return std::move(*this);
    }

    /* Asynchronously wait for a futex variable. */
    UringOp &&prep_futex_wait(uint32_t *futex, uint64_t val, uint64_t mask,
                              uint32_t futex_flags, unsigned int flags) && {
        io_uring_prep_futex_wait(mSqe, futex, val, mask, futex_flags, flags);
        return std::move(*this);
    }

    /* Asynchronously wait for multiple futex variables. */
    UringOp &&prep_futex_waitv(std::span<struct futex_waitv> futex,
                               unsigned int flags) && {
        io_uring_prep_futex_waitv(mSqe, futex.data(),
                                  static_cast<uint32_t>(futex.size()), flags);
        return std::move(*this);
    }

    /* Asynchronously wake up threads waiting on a futex variable. */
    UringOp &&prep_futex_wake(uint32_t *futex, uint64_t val, uint64_t mask,
                              uint32_t futex_flags, unsigned int flags) && {
        io_uring_prep_futex_wake(mSqe, futex, val, mask, futex_flags, flags);
        return std::move(*this);
    }

    Task<int> cancelGuard(CancelToken cancel) && {
        CancelCallback _(cancel, [this]() -> Task<> {
            co_await UringOp().prep_cancel(this, IORING_ASYNC_CANCEL_ALL);
        });
        co_return co_await std::move(*this);
    }
};

} // namespace Marcus
