#include <co_async/awaiter/task.hpp>
#include <co_async/generic/io_context.hpp>
#include <co_async/iostream/file_stream.hpp>
#include <co_async/iostream/stream_base.hpp>
#include <co_async/platform/fs.hpp>

namespace Marcus {

namespace {
struct FileStream : Stream {
    Task<Expected<std::size_t>> raw_read(std::span<char> buffer) override {
        co_return co_await fs_read(mFile, buffer, co_await co_cancel);
    }

    Task<Expected<std::size_t>>
    raw_write(std::span<const char> buffer) override {
        co_return co_await fs_write(mFile, buffer, co_await co_cancel);
    }

    Task<> raw_close() override {
        (co_await fs_close(std::move(mFile))).value_or();
    }

    FileHandle release() noexcept {
        return std::move(mFile);
    }

    FileHandle &get() noexcept {
        return mFile;
    }

    explicit FileStream(FileHandle file) : mFile(std::move(file)) {}

private:
    FileHandle mFile;
};
} // namespace

/* Asynchronously open a file and return a buffered stream (OwningStream) that
 * owns the file handle. */
Task<Expected<OwningStream>> file_open(std::filesystem::path path,
                                       OpenMode mode) {
    co_return make_stream<FileStream>(co_await co_await fs_open(path, mode));
}

/* Create an OwningStream from an existing FileHandle. */
OwningStream file_from_handle(FileHandle handle) {
    return make_stream<FileStream>(std::move(handle));
}

/* Asynchronously read the entire file content into a String. */
Task<Expected<String>> file_read(std::filesystem::path path) {
    auto file = co_await co_await file_open(path, OpenMode::Read);
    co_return co_await file.getall();
}

/* Asynchronously write content to a file, overwriting it if it exists. */
Task<Expected<>> file_write(std::filesystem::path path,
                            std::string_view content) {
    auto file = co_await co_await file_open(path, OpenMode::Write);
    co_await co_await file.puts(content);
    co_await co_await file.flush();
    co_return {};
}

/* Asynchronously append content to the end of a file. */
Task<Expected<>> file_append(std::filesystem::path path,
                             std::string_view content) {
    auto file = co_await co_await file_open(path, OpenMode::Append);
    co_await co_await file.puts(content);
    co_await co_await file.flush();
    co_return {};
}

} // namespace Marcus
