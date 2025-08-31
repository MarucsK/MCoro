#pragma once

#include <co_async/std.hpp>
#include <co_async/awaiter/task.hpp>
#include <co_async/generic/allocator.hpp>
#include <co_async/iostream/bytes_buffer.hpp>
#include <co_async/utils/expected.hpp>

namespace Marcus {

inline constexpr std::size_t kStreamBufferSize = 8192;

inline std::error_code eofError() {
    static struct : public std::error_category {
        const char *name() const noexcept override {
            return "eof";
        }

        std::string message(int) const override {
            return "End of file";
        }
    } category;

    return std::error_code(1, category);
}

struct Stream {
    /* Set the stream's timeout. */
    virtual void raw_timeout(std::chrono::steady_clock::duration timeout) {}

    /* Asynchronously move the stream's read/write position to pos. Seeking is
     * not supported by default. */
    virtual Task<Expected<>> raw_seek(std::uint64_t pos) {
        co_return std::errc::invalid_seek;
    }

    /* Asynchronously write all buffered output data to the underlying device.
     */
    virtual Task<Expected<>> raw_flush() {
        co_return {};
    }

    /* Asynchronously close the stream. */
    virtual Task<> raw_close() {
        co_return;
    }

    /* Asynchronously read data from the stream into the buffer. Returns the
     * number of bytes read. */
    virtual Task<Expected<std::size_t>> raw_read(std::span<char> buffer) {
        co_return std::errc::not_supported;
    }

    /* Asynchronously write data from the buffer to the stream. Returns the
     * number of bytes written. */
    virtual Task<Expected<std::size_t>>
    raw_write(std::span<const char> buffer) {
        co_return std::errc::not_supported;
    }

    Stream &operator=(Stream &&) = delete;

    virtual ~Stream() = default;
};

struct BorrowedStream {
    BorrowedStream() : mRaw() {}

    explicit BorrowedStream(Stream *raw) : mRaw(raw) {}

    virtual ~BorrowedStream() = default;
    BorrowedStream(BorrowedStream &&) = default;
    BorrowedStream &operator=(BorrowedStream &&) = default;

    Task<Expected<char>> getchar() {
        if (bufempty()) {
            mInEnd = mInIndex = 0;
            co_await co_await fillbuf();
        }
        char c = mInBuffer[mInIndex];
        ++mInIndex;
        co_return c;
    }

    /* Asynchronously read a line from stream into s until eol. */
    Task<Expected<>> getline(String &s, char eol) {
        std::size_t start = mInIndex;
        while (true) {
            for (std::size_t i = start; i < mInEnd; ++i) {
                if (mInBuffer[i] == eol) {
                    s.append(mInBuffer.data() + start, i - start);
                    mInIndex = i + 1;
                    co_return {};
                }
            }
            s.append(mInBuffer.data() + start, mInEnd - start);
            mInEnd = mInIndex = 0;
            co_await co_await fillbuf();
            start = 0;
        }
    }

    /* Asynchronously discard a line from stream until eol. */
    Task<Expected<>> dropline(char eol) {
        std::size_t start = mInIndex;
        while (true) {
            for (std::size_t i = start; i < mInEnd; ++i) {
                if (mInBuffer[i] == eol) {
                    mInIndex = i + 1;
                    co_return {};
                }
            }
            mInEnd = mInIndex = 0;
            co_await co_await fillbuf();
            start = 0;
        }
    }

    /* Asynchronously read a line from stream into s until eol string. */
    Task<Expected<>> getline(String &s, std::string_view eol) {
    again:
        co_await co_await getline(s, eol.front());
        for (std::size_t i = 1; i < eol.size(); ++i) {
            if (bufempty()) {
                mInEnd = mInIndex = 0;
                co_await co_await fillbuf();
            }
            char c = mInBuffer[mInIndex];
            if (eol[i] == c) [[likely]] {
                ++mInIndex;
            } else {
                s.append(eol.data(), i);
                goto again;
            }
        }
        co_return {};
    }

    /* Asynchronously discard a line from stream until eol string. */
    Task<Expected<>> dropline(std::string_view eol) {
    again:
        co_await co_await dropline(eol.front());
        for (std::size_t i = 1; i < eol.size(); ++i) {
            if (bufempty()) {
                mInEnd = mInIndex = 0;
                co_await co_await fillbuf();
            }
            char c = mInBuffer[mInIndex];
            if (eol[i] == c) [[likely]] {
                ++mInIndex;
            } else {
                goto again;
            }
        }
        co_return {};
    }

    /* Asynchronously read a line from stream until eol char, return as
     * String. */
    Task<Expected<String>> getline(char eol) {
        String s;
        co_await co_await getline(s, eol);
        co_return s;
    }

    /* Asynchronously read a line from stream until eol string, return as
     * String. */
    Task<Expected<String>> getline(std::string_view eol) {
        String s;
        co_await co_await getline(s, eol);
        co_return s;
    }

    /* Asynchronously read s.size() bytes from stream into s. */
    Task<Expected<>> getspan(std::span<char> s) {
        auto p = s.data();
        auto n = s.size();
        std::size_t start = mInIndex;
        while (true) {
            auto end = start + n;
            if (end <= mInEnd) {
                p = std::copy(mInBuffer.data() + start, mInBuffer.data() + end,
                              p);
                mInIndex = end;
                co_return {};
            }
            p = std::copy(mInBuffer.data() + start, mInBuffer.data() + mInEnd,
                          p);
            // n -= (mInEnd - start);
            mInEnd = mInIndex = 0;
            co_await co_await fillbuf();
            start = 0;
        }
    }

    /* Asynchronously discard n bytes from stream. */
    Task<Expected<>> dropn(std::size_t n) {
        auto start = mInIndex;
        while (true) {
            auto end = start + n;
            if (end <= mInEnd) {
                mInIndex = end;
                co_return {};
            }
            auto m = mInEnd - mInIndex;
            n -= m;
            mInEnd = mInIndex = 0;
            co_await co_await fillbuf();
            start = 0;
        }
    }

    /* Asynchronously read n bytes from stream into s. */
    Task<Expected<>> getn(String &s, std::size_t n) {
        auto start = mInIndex;
        while (true) {
            auto end = start + n;
            if (end <= mInEnd) {
                s.append(mInBuffer.data() + mInIndex, n);
                mInIndex = end;
                co_return {};
            }
            auto m = mInEnd - mInIndex;
            n -= m;
            s.append(mInBuffer.data() + mInIndex, m);
            mInEnd = mInIndex = 0;
            co_await co_await fillbuf();
            start = 0;
        }
    }

    /* Asynchronously read n bytes from stream, return as String. */
    Task<Expected<String>> getn(std::size_t n) {
        String s;
        s.reserve(n);
        co_await co_await getn(s, n);
        co_return s;
    }

    /* Asynchronously discard all remaining data from stream until EOF. */
    Task<Expected<>> dropall() {
        do {
            mInEnd = mInIndex = 0;
        } while (co_await (co_await fillbuf())
                     .transform([] { return true; })
                     .or_else(eofError(), [] { return false; }));
        co_return {};
    }

    /* Asynchronously read all remaining data from stream into s until EOF. */
    Task<Expected<>> getall(String &s) {
        std::size_t start = mInIndex;
        do {
            s.append(mInBuffer.data() + start, mInEnd - start);
            start = 0;
            mInEnd = mInIndex = 0;
        } while (co_await (co_await fillbuf())
                     .transform([] { return true; })
                     .or_else(eofError(), [] { return false; }));
        co_return {};
    }

    /* Asynchronously read all remaining data from stream, return as String. */
    Task<Expected<String>> getall() {
        String s;
        co_await co_await getall(s);
        co_return s;
    }

    template <typename T>
        requires std::is_trivial_v<T>
    Task<Expected<>> getstruct(T &ret) {
        return getspan(
            std::span<char>(reinterpret_cast<char *>(&ret), sizeof(T)));
    }

    template <class T>
        requires std::is_trivial_v<T>
    Task<Expected<T>> getstruct() {
        T ret;
        co_await co_await getstruct(ret);
        co_return ret;
    }

    /* Return std::span of available, unconsumed data in the current input
     * buffer, without consuming it. */
    std::span<const char> peekbuf() const noexcept {
        return {mInBuffer.data() + mInIndex, mInEnd - mInIndex};
    }

    void seenbuf(std::size_t n) noexcept {
        mInIndex += n;
    }

    /* Asynchronously get all available data from input buffer as String,
     * consuming it. */
    Task<Expected<String>> getchunk() noexcept {
        if (bufempty()) {
            mInEnd = mInIndex = 0;
            co_await co_await fillbuf();
        }
        auto buf = peekbuf();
        String ret(buf.data(), buf.size());
        seenbuf(buf.size());
        co_return std::move(ret);
    }

    /* Attempt to read data into buffer from input buffer; non-async,
     * non-blocking, only uses existing buffered data. */
    std::size_t tryread(std::span<char> buffer) {
        auto peekBuf = peekbuf();
        std::size_t n = std::min(buffer.size(), peekBuf.size());
        std::memcpy(buffer.data(), peekBuf.data(), n);
        seenbuf(n);
        return n;
    }

    /* Asynchronously peek next character, without consuming it. */
    Task<Expected<char>> peekchar() {
        if (bufempty()) {
            mInEnd = mInIndex = 0;
            co_await co_await fillbuf();
        }
        co_return mInBuffer[mInIndex];
    }

    /* Asynchronously peek n bytes into s, without consuming them. */
    Task<Expected<>> peekn(String &s, std::size_t n) {
        if (mInBuffer.size() - mInIndex < n) {
            if (mInBuffer.size() < n) [[unlikely]] {
                co_return std::errc::value_too_large;
            }
            std::memmove(mInBuffer.data(), mInBuffer.data() + mInIndex,
                         mInEnd - mInIndex);
            mInEnd -= mInIndex;
            mInIndex = 0;
        }
        while (mInEnd - mInIndex < n) {
            co_await co_await fillbuf();
        }
        s.append(mInBuffer.data() + mInIndex, n);
        co_return {};
    }

    Task<Expected<String>> peekn(std::size_t n) {
        String s;
        co_await co_await peekn(s, n);
        co_return s;
    }

    void allocinbuf(std::size_t size) {
        if (!mInBuffer) [[likely]] {
            mInBuffer.allocate(size);
            mInIndex = 0;
            mInEnd = 0;
        }
    }

    Task<Expected<>> fillbuf() {
        if (!mInBuffer) {
            allocinbuf(kStreamBufferSize);
        }
        auto n = co_await co_await mRaw->raw_read(std::span(
            mInBuffer.data() + mInIndex, mInBuffer.size() - mInIndex));
        if (n == 0) [[unlikely]] {
            co_return eofError();
        }
        mInEnd = mInIndex + n;
        co_return {};
    }

    bool bufempty() const noexcept {
        return mInIndex == mInEnd;
    }

    /* Asynchronously write a character to the output buffer. */
    Task<Expected<>> putchar(char c) {
        if (buffull()) {
            co_await co_await flush();
        }
        mOutBuffer[mOutIndex] = c;
        ++mOutIndex;
        co_return {};
    }

    /* Asynchronously write s to the output buffer. */
    Task<Expected<>> putspan(std::span<const char> s) {
        auto p = s.data();
        const auto pe = s.data() + s.size();
    again:
        if (std::size_t(pe - p) <= mOutBuffer.size() - mOutIndex) {
            auto b = mOutBuffer.data() + mOutIndex;
            mOutIndex += std::size_t(pe - p);
            while (p < pe) {
                *b++ = *p++;
            }
        } else {
            auto b = mOutBuffer.data() + mOutIndex;
            const auto be = mOutBuffer.data() + mOutBuffer.size();
            mOutIndex = mOutBuffer.size();
            while (b < be) {
                *b++ = *p++;
            }
            co_await co_await flush();
            mOutIndex = 0;
            goto again;
        }
        co_return {};
    }

    /* Attempt to write data to output buffer; non-async, non-blocking, only
     * writes to remaining buffer space. */
    std::size_t trywrite(std::span<const char> s) {
        if (!mOutBuffer) {
            allocoutbuf(kStreamBufferSize);
        }
        auto p = s.data();
        const auto pe = s.data() + s.size();
        auto nMax = mOutBuffer.size() - mOutIndex;
        auto n = std::size_t(pe - p);
        if (n <= nMax) {
            auto b = mOutBuffer.data() + mOutIndex;
            mOutIndex += std::size_t(pe - p);
            while (p < pe) {
                *b++ = *p++;
            }
            return n;
        } else {
            auto b = mOutBuffer.data() + mOutIndex;
            const auto be = mOutBuffer.data() + mOutBuffer.size();
            mOutIndex = mOutBuffer.size();
            while (b < be) {
                *b++ = *p++;
            }
            return nMax;
        }
    }

    /* Asynchronously write a std::string_view to the output buffer. */
    Task<Expected<>> puts(std::string_view s) {
        return putspan(std::span<const char>(s.data(), s.size()));
    }

    /* Asynchronously write a T-struct to the output buffer. */
    template <typename T>
    Task<Expected<>> putstruct(const T &s) {
        return putspan(std::span<const char>(
            reinterpret_cast<const char *>(std::addressof(s)), sizeof(T)));
    }

    /* Asynchronously write a std::string_view to the output buffer, then
     * immediately flush. */
    Task<Expected<>> putchunk(std::string_view s) {
        co_await co_await puts(s);
        co_return co_await flush();
    }

    /* Asynchronously write a std::string_view to the output buffer, then a
     * \n, and finally flush. */
    Task<Expected<>> putline(std::string_view s) {
        co_await co_await puts(s);
        co_await co_await putchar('\n');
        co_return co_await flush();
    }

    void allocoutbuf(std::size_t size) {
        if (!mOutBuffer) [[likely]] {
            mOutBuffer.allocate(size);
            mOutIndex = 0;
        }
    }

    /* Asynchronously write all data from output buffer to underlying Stream. */
    Task<Expected<>> flush() {
        if (!mOutBuffer) {
            allocoutbuf(kStreamBufferSize);
            co_return {};
        }
        if (mOutIndex) [[likely]] {
            auto buf = std::span(mOutBuffer.data(), mOutIndex);
            auto len = co_await mRaw->raw_write(buf);
            while (len.has_value() && *len > 0 && *len != buf.size()) {
                buf = buf.subspan(*len);
                len = co_await mRaw->raw_write(buf);
            }
            if (len.has_error()) [[unlikely]] {
#if CO_ASYNC_DEBUG
                co_return {len.error(), len.mErrorLocation};
#else
                co_return len.error();
#endif
            }
            if (*len == 0) [[unlikely]] {
                co_return eofError();
            }
            mOutIndex = 0;
            co_await co_await mRaw->raw_flush();
        }
        co_return {};
    }

    bool buffull() const noexcept {
        return mOutIndex == mOutBuffer.size();
    }

    Stream &raw() const noexcept {
        return *mRaw;
    }

    template <std::derived_from<Stream> Derived>
    Derived &raw() const {
        return dynamic_cast<Derived &>(*mRaw);
    }

    Task<> close() {
#if CO_ASYNC_DEBUG
        if (mOutIndex) [[unlikely]] {
            std::cerr << "WARNING: stream closed with buffer not flushed\n";
        }
#endif
        return mRaw->raw_close();
    }

    /* Asynchronously read data into buffer from the input buffer if available,
     * otherwise from the underlying stream. */
    Task<Expected<std::size_t>> read(std::span<char> buffer) {
        if (!bufempty()) {
            auto n = std::min(mInEnd - mInIndex, buffer.size());
            std::memcpy(buffer.data(), mInBuffer.data() + mInIndex, n);
            mInIndex += n;
            co_return n;
        }
        co_return co_await mRaw->raw_read(buffer);
    }

    Task<Expected<std::size_t>> read(void *buffer, std::size_t len) {
        return read(std::span<char>(static_cast<char *>(buffer), len));
    }

    std::size_t tryread(void *buffer, std::size_t len) {
        return tryread(std::span<char>(static_cast<char *>(buffer), len));
    }

    /* Asynchronously write buffer to stream; prioritize writing to the output
     * buffer if not full, otherwise write to the underlying stream. */
    Task<Expected<std::size_t>> write(std::span<const char> buffer) {
        if (!buffull()) {
            auto n = std::min(mOutBuffer.size() - mOutIndex, buffer.size());
            co_await co_await putspan(buffer.subspan(0, n));
            co_return n;
        }
        co_return co_await mRaw->raw_write(buffer);
    }

    Task<Expected<std::size_t>> write(const void *buffer, std::size_t len) {
        return write(
            std::span<const char>(static_cast<const char *>(buffer), len));
    }

    Task<Expected<>> putspan(const void *buffer, std::size_t len) {
        return putspan(
            std::span<const char>(static_cast<const char *>(buffer), len));
    }

    std::size_t trywrite(const void *buffer, std::size_t len) {
        return trywrite(
            std::span<const char>(static_cast<const char *>(buffer), len));
    }

    void timeout(std::chrono::steady_clock::duration timeout) {
        mRaw->raw_timeout(timeout);
    }

    Task<Expected<>> seek(std::uint64_t pos) {
        co_await co_await mRaw->raw_seek(pos);
        mInIndex = 0;
        mInEnd = 0;
        mOutIndex = 0;
        co_return {};
    }

private:
    BytesBuffer mInBuffer;
    std::size_t mInIndex =
        0; /* Read pointer, index of next char to read in inbuffer. */
    std::size_t mInEnd =
        0; /* Write pointer, next pos after end of filled data in inbuffer. */

    BytesBuffer mOutBuffer;
    std::size_t mOutIndex = 0; /* Index of next char to write in outbuffer. */

    Stream *mRaw;
};

struct OwningStream : BorrowedStream {
    explicit OwningStream() : BorrowedStream(), mRawUnique() {}

    explicit OwningStream(std::unique_ptr<Stream> raw)
        : BorrowedStream(raw.get()),
          mRawUnique(std::move(raw)) {}

    std::unique_ptr<Stream> releaseraw() noexcept {
        return std::move(mRawUnique);
    }

private:
    std::unique_ptr<Stream> mRawUnique;
};

template <std::derived_from<Stream> Stream, class... Args>
OwningStream make_stream(Args &&...args) {
    return OwningStream(std::make_unique<Stream>(std::forward<Args>(args)...));
}

} // namespace Marcus
