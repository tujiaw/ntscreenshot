#include "hanzi_to_pinyin.hpp"

#include "pinyin_data.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <new>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace hanzi_to_pinyin {

struct PhraseDictionary::Impl {
    struct Node {
        std::unordered_map<char32_t, std::unique_ptr<Node>> children;
        std::string pinyin;
        bool terminal = false;
    };

    Node root;
    std::size_t entry_count = 0;
};

namespace {

class IoException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class DictionaryException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct DecodedText {
    std::vector<char32_t> code_points;
};

Error make_error(ErrorCode code, const char* message) noexcept
{
    Error error;
    error.code = code;
    try {
        if (message != nullptr) {
            error.message = message;
        }
    } catch (...) {
        // Preserve the error code even if allocating the diagnostic fails.
    }
    return error;
}

[[noreturn]] void throw_invalid_utf8(std::size_t offset)
{
    throw std::invalid_argument(
        "Invalid UTF-8 sequence at byte offset " + std::to_string(offset));
}

DecodedText decode_utf8(std::string_view input)
{
    DecodedText result;
    result.code_points.reserve(input.size());

    for (std::size_t i = 0; i < input.size();) {
        const auto first = static_cast<std::uint8_t>(input[i]);
        char32_t code_point = 0;
        std::size_t length = 0;
        char32_t minimum = 0;

        if (first <= 0x7f) {
            code_point = first;
            length = 1;
        } else if ((first & 0xe0) == 0xc0) {
            code_point = first & 0x1f;
            length = 2;
            minimum = 0x80;
        } else if ((first & 0xf0) == 0xe0) {
            code_point = first & 0x0f;
            length = 3;
            minimum = 0x800;
        } else if ((first & 0xf8) == 0xf0) {
            code_point = first & 0x07;
            length = 4;
            minimum = 0x10000;
        } else {
            throw_invalid_utf8(i);
        }

        if (i + length > input.size()) {
            throw_invalid_utf8(i);
        }

        for (std::size_t j = 1; j < length; ++j) {
            const auto continuation = static_cast<std::uint8_t>(input[i + j]);
            if ((continuation & 0xc0) != 0x80) {
                throw_invalid_utf8(i + j);
            }
            code_point = (code_point << 6) | (continuation & 0x3f);
        }

        if ((length > 1 && code_point < minimum) ||
            code_point > 0x10ffff ||
            (code_point >= 0xd800 && code_point <= 0xdfff)) {
            throw_invalid_utf8(i);
        }

        result.code_points.push_back(code_point);
        i += length;
    }

    return result;
}

void validate_phrase_entry(
    std::string_view phrase,
    const DecodedText& decoded_phrase,
    std::string_view phrase_pinyin)
{
    if (phrase.empty() || decoded_phrase.code_points.empty()) {
        throw std::invalid_argument("Phrase must not be empty");
    }
    if (phrase_pinyin.empty()) {
        throw std::invalid_argument("Phrase pinyin must not be empty");
    }

    std::size_t syllable_count = 1;
    bool previous_was_separator = true;
    for (const char character : phrase_pinyin) {
        if (character >= 'a' && character <= 'z') {
            previous_was_separator = false;
        } else if (
            character == '\'' &&
            !previous_was_separator &&
            syllable_count < decoded_phrase.code_points.size()) {
            ++syllable_count;
            previous_was_separator = true;
        } else {
            throw std::invalid_argument(
                "Pinyin must contain lowercase ASCII letters separated by apostrophes");
        }
    }

    if (previous_was_separator) {
        throw std::invalid_argument("Pinyin must not end with an apostrophe");
    }
    if (syllable_count != decoded_phrase.code_points.size()) {
        throw std::invalid_argument(
            "Pinyin syllable count must match the phrase character count");
    }
}

void append_utf8(std::string& output, char32_t code_point)
{
    if (code_point <= 0x7f) {
        output.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (code_point >> 6)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
    } else if (code_point <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (code_point >> 12)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (code_point >> 18)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
    }
}

const wchar_t* lookup(char32_t code_point) noexcept
{
    using namespace detail;

    if (code_point < 0x4e00 || code_point > 0x9fa5) {
        return nullptr;
    }
    if (code_point >= 0x9a2c) return gama14[code_point - 0x9a2c];
    if (code_point >= 0x9450) return gama13[code_point - 0x9450];
    if (code_point >= 0x8e74) return gama12[code_point - 0x8e74];
    if (code_point >= 0x8898) return gama11[code_point - 0x8898];
    if (code_point >= 0x82bc) return gama10[code_point - 0x82bc];
    if (code_point >= 0x7ce0) return gama9[code_point - 0x7ce0];
    if (code_point >= 0x7704) return gama8[code_point - 0x7704];
    if (code_point >= 0x7128) return gama7[code_point - 0x7128];
    if (code_point >= 0x6b4c) return gama6[code_point - 0x6b4c];
    if (code_point >= 0x6570) return gama5[code_point - 0x6570];
    if (code_point >= 0x5f94) return gama4[code_point - 0x5f94];
    if (code_point >= 0x59b8) return gama3[code_point - 0x59b8];
    if (code_point >= 0x53dc) return gama2[code_point - 0x53dc];
    return gama1[code_point - 0x4e00];
}

std::string narrow_ascii(const wchar_t* value)
{
    std::string result;
    if (value == nullptr) {
        return result;
    }
    while (*value != L'\0') {
        result.push_back(static_cast<char>(*value++));
    }
    return result;
}

void append_mapped(
    std::string& output,
    std::string_view mapped,
    const Options& options,
    bool& previous_was_mapped)
{
    if (previous_was_mapped) {
        output += options.separator;
    }
    for (const char character : mapped) {
        if (character == '\'') {
            output += options.separator;
        } else {
            output.push_back(character);
        }
    }
    previous_was_mapped = true;
}

std::string read_utf8_file(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw IoException("Unable to open input file: " + path.string());
    }

    std::string content{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
    if (!input.eof() && input.fail()) {
        throw IoException("Unable to read input file: " + path.string());
    }
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xef &&
        static_cast<unsigned char>(content[1]) == 0xbb &&
        static_cast<unsigned char>(content[2]) == 0xbf) {
        content.erase(0, 3);
    }
    return content;
}

void write_file(const std::filesystem::path& path, std::string_view content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw IoException("Unable to open output file: " + path.string());
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!output) {
        throw IoException("Unable to write output file: " + path.string());
    }
}

} // namespace

PhraseDictionary::PhraseDictionary() noexcept = default;

PhraseDictionary::~PhraseDictionary() noexcept = default;
PhraseDictionary::PhraseDictionary(PhraseDictionary&&) noexcept = default;
PhraseDictionary& PhraseDictionary::operator=(PhraseDictionary&&) noexcept = default;

Error PhraseDictionary::add(
    std::string_view utf8_phrase,
    std::string_view phrase_pinyin) noexcept
{
    try {
        add_or_throw(utf8_phrase, phrase_pinyin);
        return {};
    } catch (const std::bad_alloc&) {
        return make_error(ErrorCode::out_of_memory, "Out of memory");
    } catch (const std::invalid_argument& error) {
        return make_error(ErrorCode::invalid_dictionary, error.what());
    } catch (const std::exception& error) {
        return make_error(ErrorCode::internal_error, error.what());
    } catch (...) {
        return make_error(ErrorCode::internal_error, "Unknown internal error");
    }
}

void PhraseDictionary::add_or_throw(
    std::string_view utf8_phrase,
    std::string_view phrase_pinyin)
{
    const DecodedText decoded = decode_utf8(utf8_phrase);
    validate_phrase_entry(utf8_phrase, decoded, phrase_pinyin);
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }

    Impl::Node* node = &impl_->root;
    for (const char32_t code_point : decoded.code_points) {
        auto& child = node->children[code_point];
        if (!child) {
            child = std::make_unique<Impl::Node>();
        }
        node = child.get();
    }

    if (!node->terminal) {
        ++impl_->entry_count;
    }
    node->terminal = true;
    node->pinyin.assign(phrase_pinyin);
}

Error PhraseDictionary::load_file(
    const std::filesystem::path& dictionary_path,
    bool clear_existing) noexcept
{
    try {
        load_file_or_throw(dictionary_path, clear_existing);
        return {};
    } catch (const std::bad_alloc&) {
        return make_error(ErrorCode::out_of_memory, "Out of memory");
    } catch (const IoException& error) {
        return make_error(ErrorCode::io_error, error.what());
    } catch (const DictionaryException& error) {
        return make_error(ErrorCode::invalid_dictionary, error.what());
    } catch (const std::exception& error) {
        return make_error(ErrorCode::internal_error, error.what());
    } catch (...) {
        return make_error(ErrorCode::internal_error, "Unknown internal error");
    }
}

void PhraseDictionary::load_file_or_throw(
    const std::filesystem::path& dictionary_path,
    bool clear_existing)
{
    const std::string content = read_utf8_file(dictionary_path);
    try {
        static_cast<void>(decode_utf8(content));
    } catch (const std::invalid_argument& error) {
        throw DictionaryException(
            "Dictionary is not valid UTF-8: " + std::string{error.what()});
    }

    std::vector<std::pair<std::string, std::string>> entries;
    std::unordered_set<std::string> seen_phrases;

    std::size_t line_number = 0;
    std::size_t line_start = 0;
    while (line_start <= content.size()) {
        ++line_number;
        const std::size_t line_end = content.find('\n', line_start);
        std::string line = content.substr(line_start, line_end - line_start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (!line.empty() && line.front() != '#') {
            const std::size_t tab = line.find('\t');
            if (tab == std::string::npos ||
                tab == 0 ||
                tab + 1 == line.size() ||
                line.find('\t', tab + 1) != std::string::npos) {
                throw DictionaryException(
                    "Invalid dictionary row at line " + std::to_string(line_number) +
                    ": expected <phrase><TAB><pinyin>");
            }

            std::string phrase = line.substr(0, tab);
            std::string phrase_pinyin = line.substr(tab + 1);
            if (!seen_phrases.emplace(phrase).second) {
                throw DictionaryException(
                    "Duplicate phrase at dictionary line " +
                    std::to_string(line_number) + ": " + phrase);
            }

            try {
                const DecodedText decoded = decode_utf8(phrase);
                validate_phrase_entry(phrase, decoded, phrase_pinyin);
            } catch (const std::invalid_argument& error) {
                throw DictionaryException(
                    "Invalid dictionary entry at line " +
                    std::to_string(line_number) + ": " + error.what());
            }
            entries.emplace_back(std::move(phrase), std::move(phrase_pinyin));
        }

        if (line_end == std::string::npos) {
            break;
        }
        line_start = line_end + 1;
    }

    if (clear_existing) {
        clear();
    }
    for (const auto& [phrase, phrase_pinyin] : entries) {
        add_or_throw(phrase, phrase_pinyin);
    }
}

void PhraseDictionary::clear() noexcept
{
    impl_.reset();
}

bool PhraseDictionary::empty() const noexcept
{
    return size() == 0;
}

std::size_t PhraseDictionary::size() const noexcept
{
    return impl_ ? impl_->entry_count : 0;
}

std::pair<std::size_t, std::string_view> PhraseDictionary::longest_match(
    const char32_t* code_points,
    std::size_t count) const noexcept
{
    if (!impl_ || code_points == nullptr) {
        return {};
    }

    const Impl::Node* node = &impl_->root;
    std::pair<std::size_t, std::string_view> result;
    for (std::size_t i = 0; i < count; ++i) {
        const auto child = node->children.find(code_points[i]);
        if (child == node->children.end()) {
            break;
        }
        node = child->second.get();
        if (node->terminal) {
            result = {i + 1, node->pinyin};
        }
    }
    return result;
}

static std::vector<std::string> pronunciations_or_throw(char32_t code_point)
{
    const std::string all = narrow_ascii(lookup(code_point));
    std::vector<std::string> result;
    if (all.empty()) {
        return result;
    }

    std::size_t start = 0;
    while (start <= all.size()) {
        const std::size_t end = all.find('\'', start);
        result.emplace_back(all.substr(start, end - start));
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return result;
}

static std::string pinyin_or_throw(char32_t code_point)
{
    std::string result = narrow_ascii(lookup(code_point));
    const auto separator = result.find('\'');
    if (separator != std::string::npos) {
        result.erase(separator);
    }
    return result;
}

Result<std::vector<std::string>> pronunciations(char32_t code_point) noexcept
{
    try {
        return {pronunciations_or_throw(code_point), {}};
    } catch (const std::bad_alloc&) {
        return {{}, make_error(ErrorCode::out_of_memory, "Out of memory")};
    } catch (const std::exception& error) {
        return {{}, make_error(ErrorCode::internal_error, error.what())};
    } catch (...) {
        return {{}, make_error(ErrorCode::internal_error, "Unknown internal error")};
    }
}

Result<std::string> pinyin(char32_t code_point) noexcept
{
    try {
        return {pinyin_or_throw(code_point), {}};
    } catch (const std::bad_alloc&) {
        return {{}, make_error(ErrorCode::out_of_memory, "Out of memory")};
    } catch (const std::exception& error) {
        return {{}, make_error(ErrorCode::internal_error, error.what())};
    } catch (...) {
        return {{}, make_error(ErrorCode::internal_error, "Unknown internal error")};
    }
}

Result<std::string> convert(
    std::string_view utf8_text,
    const Options& options) noexcept
{
    static const PhraseDictionary empty_dictionary;
    return convert(utf8_text, empty_dictionary, options);
}

Result<std::string> convert(
    std::string_view utf8_text,
    const PhraseDictionary& dictionary,
    const Options& options) noexcept
{
    try {
        const auto decoded = decode_utf8(utf8_text);
        std::string output;
        output.reserve(utf8_text.size() * 2);
        bool previous_was_mapped = false;

        for (std::size_t i = 0; i < decoded.code_points.size();) {
            std::pair<std::size_t, std::string_view> matched_phrase;
            if (options.phrase_matching) {
                matched_phrase = dictionary.longest_match(
                    decoded.code_points.data() + i,
                    decoded.code_points.size() - i);
            }

            if (matched_phrase.first != 0) {
                append_mapped(
                    output, matched_phrase.second, options, previous_was_mapped);
                i += matched_phrase.first;
                continue;
            }

            const std::string mapped = pinyin_or_throw(decoded.code_points[i]);
            if (!mapped.empty()) {
                append_mapped(output, mapped, options, previous_was_mapped);
            } else {
                if (options.preserve_unmapped) {
                    append_utf8(output, decoded.code_points[i]);
                }
                previous_was_mapped = false;
            }
            ++i;
        }

        return {std::move(output), {}};
    } catch (const std::bad_alloc&) {
        return {{}, make_error(ErrorCode::out_of_memory, "Out of memory")};
    } catch (const std::invalid_argument& error) {
        return {{}, make_error(ErrorCode::invalid_utf8, error.what())};
    } catch (const std::exception& error) {
        return {{}, make_error(ErrorCode::internal_error, error.what())};
    } catch (...) {
        return {{}, make_error(ErrorCode::internal_error, "Unknown internal error")};
    }
}

Error convert_file(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path,
    const Options& options) noexcept
{
    try {
        const auto converted = convert(read_utf8_file(input_path), options);
        if (!converted) {
            return converted.error;
        }
        write_file(output_path, converted.value);
        return {};
    } catch (const std::bad_alloc&) {
        return make_error(ErrorCode::out_of_memory, "Out of memory");
    } catch (const IoException& error) {
        return make_error(ErrorCode::io_error, error.what());
    } catch (const std::exception& error) {
        return make_error(ErrorCode::internal_error, error.what());
    } catch (...) {
        return make_error(ErrorCode::internal_error, "Unknown internal error");
    }
}

Error convert_file(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path,
    const PhraseDictionary& dictionary,
    const Options& options) noexcept
{
    try {
        const auto converted =
            convert(read_utf8_file(input_path), dictionary, options);
        if (!converted) {
            return converted.error;
        }
        write_file(output_path, converted.value);
        return {};
    } catch (const std::bad_alloc&) {
        return make_error(ErrorCode::out_of_memory, "Out of memory");
    } catch (const IoException& error) {
        return make_error(ErrorCode::io_error, error.what());
    } catch (const std::exception& error) {
        return make_error(ErrorCode::internal_error, error.what());
    } catch (...) {
        return make_error(ErrorCode::internal_error, "Unknown internal error");
    }
}

} // namespace hanzi_to_pinyin
