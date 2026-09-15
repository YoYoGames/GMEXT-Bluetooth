#include "GMBluetooth_json.h"

#include <array>
#include <cctype>
#include <cstdlib>

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
        return type == Type::Number ? static_cast<std::int32_t>(number_value) : fallback;
    }

    bool Value::as_bool(bool fallback) const
    {
        return type == Type::Bool ? bool_value : fallback;
    }

    namespace
    {
        class Parser
        {
        public:
            explicit Parser(std::string_view text) : text_(text) {}

            std::optional<Value> parse_document()
            {
                skip_ws();
                auto value = parse_value();
                if (!value)
                    return std::nullopt;
                skip_ws();
                // Trailing garbage is tolerated: backends may embed a JSON
                // value inside a larger string we don't otherwise care about.
                return value;
            }

        private:
            std::string_view text_;
            std::size_t pos_ = 0;

            bool eof() const { return pos_ >= text_.size(); }
            char peek() const { return text_[pos_]; }

            void skip_ws()
            {
                while (!eof() && std::isspace(static_cast<unsigned char>(peek())))
                    ++pos_;
            }

            std::optional<Value> parse_value()
            {
                skip_ws();
                if (eof())
                    return std::nullopt;

                switch (peek())
                {
                case '{': return parse_object();
                case '[': return parse_array();
                case '"': return parse_string_value();
                case 't':
                case 'f': return parse_bool();
                case 'n': return parse_null();
                default:  return parse_number();
                }
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

            std::optional<Value> parse_number()
            {
                const std::size_t start = pos_;
                if (!eof() && (peek() == '-' || peek() == '+'))
                    ++pos_;
                while (!eof() && (std::isdigit(static_cast<unsigned char>(peek())) ||
                                   peek() == '.' || peek() == 'e' || peek() == 'E' ||
                                   peek() == '+' || peek() == '-'))
                    ++pos_;

                if (pos_ == start)
                    return std::nullopt;

                const std::string token(text_.substr(start, pos_ - start));
                char* end = nullptr;
                const double number = std::strtod(token.c_str(), &end);
                if (end != token.c_str() + token.size())
                    return std::nullopt;

                Value v;
                v.type = Type::Number;
                v.number_value = number;
                return v;
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
                        if (pos_ + 4 > text_.size())
                            return std::nullopt;
                        unsigned int code = 0;
                        for (int i = 0; i < 4; ++i)
                        {
                            const char hex = text_[pos_++];
                            code <<= 4;
                            if (hex >= '0' && hex <= '9') code |= static_cast<unsigned int>(hex - '0');
                            else if (hex >= 'a' && hex <= 'f') code |= static_cast<unsigned int>(hex - 'a' + 10);
                            else if (hex >= 'A' && hex <= 'F') code |= static_cast<unsigned int>(hex - 'A' + 10);
                            else return std::nullopt;
                        }
                        // Encode as UTF-8. Surrogate pairs are not handled -
                        // none of the fields we parse (UUIDs, base64) need them.
                        if (code < 0x80)
                        {
                            out.push_back(static_cast<char>(code));
                        }
                        else if (code < 0x800)
                        {
                            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        }
                        else
                        {
                            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        }
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

    std::vector<std::uint8_t> base64_decode(std::string_view text)
    {
        static const std::array<int, 256> decode_table = make_decode_table();

        std::vector<std::uint8_t> out;
        out.reserve((text.size() / 4) * 3);

        int buffer = 0;
        int bits_collected = 0;

        for (const char c : text)
        {
            if (c == '=' || std::isspace(static_cast<unsigned char>(c)))
                continue;

            const int value = decode_table[static_cast<unsigned char>(c)];
            if (value < 0)
                continue; // skip unrecognized characters rather than fail outright

            buffer = (buffer << 6) | value;
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
