#include "GMBluetooth_json.h"

#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>

namespace gmbluetooth::json
{
    const Value* Value::find(std::string_view key) const
    {
        if (type != Type::Object)
            return nullptr;

        for (const auto& [k, v] : object_value)
        {
            if (k == key)
                return &v;
        }
        return nullptr;
    }

    std::string Value::as_string(std::string_view fallback) const
    {
        return type == Type::String ? string_value : std::string(fallback);
    }

    double Value::as_double(double fallback) const
    {
        return type == Type::Number ? number_value : fallback;
    }

    std::int32_t Value::as_int(std::int32_t fallback) const
    {
        if (type != Type::Number || !std::isfinite(number_value) || std::trunc(number_value) != number_value)
            return fallback;
        if (number_value < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
            number_value > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
            return fallback;
        return static_cast<std::int32_t>(number_value);
    }

    std::uint64_t Value::as_uint64(std::uint64_t fallback) const
    {
        constexpr double max_exact = 9007199254740992.0; // 2^53
        if (type != Type::Number || !std::isfinite(number_value) || std::trunc(number_value) != number_value)
            return fallback;
        if (number_value < 0.0 || number_value > max_exact)
            return fallback;
        return static_cast<std::uint64_t>(number_value);
    }

    bool Value::as_bool(bool fallback) const
    {
        return type == Type::Bool ? bool_value : fallback;
    }

    namespace
    {
        // Deep enough for every payload a backend sends (three levels at most),
        // shallow enough that the recursion cannot exhaust a thread's stack.
        constexpr int k_max_depth = 64;

        bool is_digit(char c) { return c >= '0' && c <= '9'; }

        // JSON whitespace only; std::isspace also takes \v and \f.
        bool is_json_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

        void append_utf8(std::string& out, std::uint32_t code)
        {
            if (code < 0x80)
            {
                out.push_back(static_cast<char>(code));
            }
            else if (code < 0x800)
            {
                out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else if (code < 0x10000)
            {
                out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else
            {
                out.push_back(static_cast<char>(0xF0 | (code >> 18)));
                out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
        }

        class Parser
        {
        public:
            explicit Parser(std::string_view text) : text_(text) {}

            std::optional<Value> parse_document()
            {
                auto value = parse_value();
                if (!value)
                    return std::nullopt;
                skip_ws();
                if (!eof())
                    return std::nullopt;
                return value;
            }

        private:
            std::string_view text_;
            std::size_t pos_ = 0;
            int depth_ = 0;

            bool eof() const { return pos_ >= text_.size(); }
            char peek() const { return text_[pos_]; }

            void skip_ws()
            {
                while (!eof() && is_json_space(peek()))
                    ++pos_;
            }

            std::optional<Value> parse_value()
            {
                skip_ws();
                if (eof())
                    return std::nullopt;

                switch (peek())
                {
                case '{': return parse_nested(&Parser::parse_object);
                case '[': return parse_nested(&Parser::parse_array);
                case '"': return parse_string_value();
                case 't':
                case 'f': return parse_bool();
                case 'n': return parse_null();
                default:  return parse_number();
                }
            }

            std::optional<Value> parse_nested(std::optional<Value> (Parser::*parse)())
            {
                if (depth_ >= k_max_depth)
                    return std::nullopt;
                ++depth_;
                auto value = (this->*parse)();
                --depth_;
                return value;
            }

            bool consume_literal(std::string_view literal)
            {
                if (text_.substr(pos_, literal.size()) != literal)
                    return false;
                pos_ += literal.size();
                return true;
            }

            std::optional<Value> parse_bool()
            {
                Value v;
                v.type = Type::Bool;
                if (consume_literal("true")) { v.bool_value = true; return v; }
                if (consume_literal("false")) { v.bool_value = false; return v; }
                return std::nullopt;
            }

            std::optional<Value> parse_null()
            {
                if (!consume_literal("null"))
                    return std::nullopt;
                Value v;
                v.type = Type::Null;
                return v;
            }

            // The RFC 8259 number grammar, converted in the classic locale so a
            // game that set a decimal comma cannot change what a payload means.
            std::optional<Value> parse_number()
            {
                const std::size_t start = pos_;
                if (!eof() && peek() == '-')
                    ++pos_;

                if (eof() || !is_digit(peek()))
                    return std::nullopt;
                if (peek() == '0')
                    ++pos_;
                else
                    while (!eof() && is_digit(peek()))
                        ++pos_;

                if (!eof() && peek() == '.')
                {
                    ++pos_;
                    if (eof() || !is_digit(peek()))
                        return std::nullopt;
                    while (!eof() && is_digit(peek()))
                        ++pos_;
                }

                if (!eof() && (peek() == 'e' || peek() == 'E'))
                {
                    ++pos_;
                    if (!eof() && (peek() == '+' || peek() == '-'))
                        ++pos_;
                    if (eof() || !is_digit(peek()))
                        return std::nullopt;
                    while (!eof() && is_digit(peek()))
                        ++pos_;
                }

                std::istringstream stream(std::string(text_.substr(start, pos_ - start)));
                stream.imbue(std::locale::classic());
                double number = 0.0;
                stream >> number;
                if (stream.fail())
                    return std::nullopt;

                Value v;
                v.type = Type::Number;
                v.number_value = number;
                return v;
            }

            std::optional<std::uint32_t> parse_hex4()
            {
                if (pos_ + 4 > text_.size())
                    return std::nullopt;
                std::uint32_t code = 0;
                for (int i = 0; i < 4; ++i)
                {
                    const char hex = text_[pos_++];
                    code <<= 4;
                    if (hex >= '0' && hex <= '9') code |= static_cast<std::uint32_t>(hex - '0');
                    else if (hex >= 'a' && hex <= 'f') code |= static_cast<std::uint32_t>(hex - 'a' + 10);
                    else if (hex >= 'A' && hex <= 'F') code |= static_cast<std::uint32_t>(hex - 'A' + 10);
                    else return std::nullopt;
                }
                return code;
            }

            std::optional<std::string> parse_raw_string()
            {
                if (eof() || peek() != '"')
                    return std::nullopt;
                ++pos_;

                std::string out;
                while (true)
                {
                    if (eof())
                        return std::nullopt;

                    const char c = text_[pos_++];
                    if (c == '"')
                        return out;

                    // Control characters must be escaped.
                    if (static_cast<unsigned char>(c) < 0x20)
                        return std::nullopt;

                    if (c != '\\')
                    {
                        out.push_back(c);
                        continue;
                    }

                    if (eof())
                        return std::nullopt;

                    const char esc = text_[pos_++];
                    switch (esc)
                    {
                    case '"':  out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/'); break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u':
                    {
                        auto code = parse_hex4();
                        if (!code)
                            return std::nullopt;

                        // A high surrogate must be followed by an escaped low
                        // one; the pair is one code point. A lone surrogate is
                        // not valid UTF-8 in any encoding, so it fails.
                        if (*code >= 0xD800 && *code <= 0xDBFF)
                        {
                            if (!consume_literal("\\u"))
                                return std::nullopt;
                            const auto low = parse_hex4();
                            if (!low || *low < 0xDC00 || *low > 0xDFFF)
                                return std::nullopt;
                            *code = 0x10000 + ((*code - 0xD800) << 10) + (*low - 0xDC00);
                        }
                        else if (*code >= 0xDC00 && *code <= 0xDFFF)
                        {
                            return std::nullopt;
                        }

                        append_utf8(out, *code);
                        break;
                    }
                    default:
                        return std::nullopt;
                    }
                }
            }

            std::optional<Value> parse_string_value()
            {
                auto raw = parse_raw_string();
                if (!raw)
                    return std::nullopt;
                Value v;
                v.type = Type::String;
                v.string_value = std::move(*raw);
                return v;
            }

            std::optional<Value> parse_array()
            {
                if (eof() || peek() != '[')
                    return std::nullopt;
                ++pos_;

                Value v;
                v.type = Type::Array;

                skip_ws();
                if (!eof() && peek() == ']')
                {
                    ++pos_;
                    return v;
                }

                while (true)
                {
                    auto element = parse_value();
                    if (!element)
                        return std::nullopt;
                    v.array_value.push_back(std::move(*element));

                    skip_ws();
                    if (eof())
                        return std::nullopt;

                    if (peek() == ',')
                    {
                        ++pos_;
                        continue;
                    }
                    if (peek() == ']')
                    {
                        ++pos_;
                        return v;
                    }
                    return std::nullopt;
                }
            }

            std::optional<Value> parse_object()
            {
                if (eof() || peek() != '{')
                    return std::nullopt;
                ++pos_;

                Value v;
                v.type = Type::Object;

                skip_ws();
                if (!eof() && peek() == '}')
                {
                    ++pos_;
                    return v;
                }

                while (true)
                {
                    skip_ws();
                    auto key = parse_raw_string();
                    if (!key)
                        return std::nullopt;

                    skip_ws();
                    if (eof() || peek() != ':')
                        return std::nullopt;
                    ++pos_;

                    auto value = parse_value();
                    if (!value)
                        return std::nullopt;

                    v.object_value.emplace_back(std::move(*key), std::move(*value));

                    skip_ws();
                    if (eof())
                        return std::nullopt;

                    if (peek() == ',')
                    {
                        ++pos_;
                        continue;
                    }
                    if (peek() == '}')
                    {
                        ++pos_;
                        return v;
                    }
                    return std::nullopt;
                }
            }
        };
    }

    std::optional<Value> parse(std::string_view text)
    {
        Parser parser(text);
        return parser.parse_document();
    }

    namespace
    {
        constexpr char kBase64Alphabet[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::array<int, 256> make_decode_table()
        {
            std::array<int, 256> table{};
            table.fill(-1);
            for (int i = 0; i < 64; ++i)
                table[static_cast<unsigned char>(kBase64Alphabet[i])] = i;
            return table;
        }
    }

    std::optional<std::vector<std::uint8_t>> base64_decode(std::string_view text)
    {
        static const std::array<int, 256> decode_table = make_decode_table();

        if (text.size() % 4 != 0)
            return std::nullopt;

        std::size_t padding = 0;
        if (!text.empty() && text.back() == '=')
            padding = (text.size() >= 2 && text[text.size() - 2] == '=') ? 2 : 1;

        std::vector<std::uint8_t> out;
        out.reserve((text.size() / 4) * 3);

        const std::size_t data_size = text.size() - padding;
        std::uint32_t buffer = 0;
        int bits_collected = 0;

        for (std::size_t i = 0; i < data_size; ++i)
        {
            const int value = decode_table[static_cast<unsigned char>(text[i])];
            if (value < 0)
                return std::nullopt; // '=' inside the data lands here too

            buffer = ((buffer << 6) | static_cast<std::uint32_t>(value)) & 0xFFFFFFu;
            bits_collected += 6;
            if (bits_collected >= 8)
            {
                bits_collected -= 8;
                out.push_back(static_cast<std::uint8_t>((buffer >> bits_collected) & 0xFF));
            }
        }

        return out;
    }

    std::string base64_encode(const std::uint8_t* data, std::size_t size)
    {
        std::string out;
        out.reserve(((size + 2) / 3) * 4);

        std::size_t i = 0;
        for (; i + 3 <= size; i += 3)
        {
            const std::uint32_t chunk = (static_cast<std::uint32_t>(data[i]) << 16) |
                                         (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                                         static_cast<std::uint32_t>(data[i + 2]);
            out.push_back(kBase64Alphabet[(chunk >> 18) & 0x3F]);
            out.push_back(kBase64Alphabet[(chunk >> 12) & 0x3F]);
            out.push_back(kBase64Alphabet[(chunk >> 6) & 0x3F]);
            out.push_back(kBase64Alphabet[chunk & 0x3F]);
        }

        const std::size_t remaining = size - i;
        if (remaining == 1)
        {
            const std::uint32_t chunk = static_cast<std::uint32_t>(data[i]) << 16;
            out.push_back(kBase64Alphabet[(chunk >> 18) & 0x3F]);
            out.push_back(kBase64Alphabet[(chunk >> 12) & 0x3F]);
            out.push_back('=');
            out.push_back('=');
        }
        else if (remaining == 2)
        {
            const std::uint32_t chunk = (static_cast<std::uint32_t>(data[i]) << 16) |
                                         (static_cast<std::uint32_t>(data[i + 1]) << 8);
            out.push_back(kBase64Alphabet[(chunk >> 18) & 0x3F]);
            out.push_back(kBase64Alphabet[(chunk >> 12) & 0x3F]);
            out.push_back(kBase64Alphabet[(chunk >> 6) & 0x3F]);
            out.push_back('=');
        }

        return out;
    }
}
