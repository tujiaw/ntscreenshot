#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hanzi_to_pinyin {

enum class ErrorCode {
    none = 0,
    invalid_utf8,
    invalid_dictionary,
    io_error,
    out_of_memory,
    internal_error,
};

struct Error {
    ErrorCode code = ErrorCode::none;
    std::string message;

    // Go-style convention: true means an error is present.
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return code != ErrorCode::none;
    }
};

template <typename T>
struct Result {
    T value{};
    Error error{};

    // A Result is true when it contains a usable value.
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return !static_cast<bool>(error);
    }
};

struct Options {
    // Separator inserted between adjacent pinyin syllables.
    std::string_view separator = "'";

    // Keep characters that do not have an entry in the pinyin table.
    bool preserve_unmapped = true;

    // Enable phrase matching when a PhraseDictionary is supplied.
    bool phrase_matching = true;
};

class PhraseDictionary {
public:
    PhraseDictionary() noexcept;
    ~PhraseDictionary() noexcept;

    PhraseDictionary(PhraseDictionary&&) noexcept;
    PhraseDictionary& operator=(PhraseDictionary&&) noexcept;

    PhraseDictionary(const PhraseDictionary&) = delete;
    PhraseDictionary& operator=(const PhraseDictionary&) = delete;

    // Adds or replaces one phrase. Both arguments must be valid UTF-8.
    // Pinyin uses apostrophes between syllables, for example "chong'qing".
    [[nodiscard]] Error add(
        std::string_view utf8_phrase,
        std::string_view phrase_pinyin) noexcept;

    // Loads a UTF-8 tab-separated dictionary. By default existing entries are
    // cleared; pass false to merge and let the loaded entries take precedence.
    [[nodiscard]] Error load_file(
        const std::filesystem::path& dictionary_path,
        bool clear_existing = true) noexcept;

    void clear() noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void add_or_throw(
        std::string_view utf8_phrase,
        std::string_view phrase_pinyin);
    void load_file_or_throw(
        const std::filesystem::path& dictionary_path,
        bool clear_existing);

    [[nodiscard]] std::pair<std::size_t, std::string_view> longest_match(
        const char32_t* code_points,
        std::size_t count) const noexcept;

    friend Result<std::string> convert(
        std::string_view,
        const PhraseDictionary&,
        const Options&) noexcept;
};

// Returns every known pronunciation for one Unicode code point.
// An empty vector means that the character is not present in the table.
[[nodiscard]] Result<std::vector<std::string>> pronunciations(
    char32_t code_point) noexcept;

// Returns the first known pronunciation, or an empty string if none is known.
[[nodiscard]] Result<std::string> pinyin(char32_t code_point) noexcept;

// Converts UTF-8 text to pinyin. Failures are returned in Result::error.
[[nodiscard]] Result<std::string> convert(
    std::string_view utf8_text,
    const Options& options = {}) noexcept;

// Converts text using longest-match phrase pronunciations before falling back
// to the single-character table.
[[nodiscard]] Result<std::string> convert(
    std::string_view utf8_text,
    const PhraseDictionary& dictionary,
    const Options& options = {}) noexcept;

// Reads and writes UTF-8 files in binary mode. A UTF-8 BOM in the input is
// accepted and omitted from the output.
[[nodiscard]] Error convert_file(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path,
    const Options& options = {}) noexcept;

[[nodiscard]] Error convert_file(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path,
    const PhraseDictionary& dictionary,
    const Options& options = {}) noexcept;

} // namespace hanzi_to_pinyin
