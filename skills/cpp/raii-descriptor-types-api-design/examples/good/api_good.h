// GOOD: a resource API designed so misuse is hard.
#include <cassert>
#include <cstddef>
#include <memory>

// G1: typed descriptor with RAII, non-copyable, movable.
class File {
public:
    explicit File(int fd) noexcept : fd_(fd) { assert(fd_ >= 0); }
    ~File() { if (fd_ >= 0) close_fd(fd_); }

    File(const File &) = delete;
    File &operator=(const File &) = delete;
    File(File &&other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    File &operator=(File &&other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) close_fd(fd_);
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int fd() const noexcept { return fd_; }

private:
    static void close_fd(int fd) noexcept;
    int fd_;
};

// G2: allocation result handed to a manager immediately (R.12).
std::unique_ptr<char[]> make_buf(std::size_t n) {
    return std::make_unique<char[]>(n);
}

// G3: typed error enum, no errno.
enum class Status { Ok, NotFound, IoError };
Status read_file(const File &f, char *dst, std::size_t n);

// G4: builder for complex construction (invalid states unrepresentable until build()).
class ReaderBuilder {
public:
    ReaderBuilder &buf_size(std::size_t n) { buf_size_ = n; return *this; }
    Status build(std::unique_ptr<File> &out) const {
        if (buf_size_ == 0) return Status::IoError; // validate before producing
        out = std::make_unique<File>(0);            // simplified: real open here
        return Status::Ok;
    }

private:
    std::size_t buf_size_ = 4096;
};
