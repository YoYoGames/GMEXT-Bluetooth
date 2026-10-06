#pragma once

// Minimal hand-rolled JSON reader + base64 codec.
//
// Scope is intentionally narrow: the core only ever *parses* JSON that the
// backends already emit (Apple's proven event payloads, mirrored by Windows
// and Android), and only ever *builds* JSON for the one outbound call that
// needs it (bluetooth_le_server_add_service). No writer/serializer for
// arbitrary values is provided because nothing else needs one.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gmbluetooth::json
{
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object,
    };

    class Value
    {
    public:
        Type type = Type::Null;

        bool bool_value = false;
        double number_value = 0.0;
        std::string string_value;
        std::vector<Value> array_value;
        std::vector<std::pair<std::string, Value>> object_value;

        bool is_object() const { return type == Type::Object; }
        bool is_array() const { return type == Type::Array; }
        bool is_string() const { return type == Type::String; }
        bool is_number() const { return type == Type::Number; }
        bool is_bool() const { return type == Type::Bool; }
        bool is_null() const { return type == Type::Null; }

        // Object member lookup. Returns nullptr if this isn't an object or
        // the key is absent.
        const Value* find(std::string_view key) const;

        std::string as_string(std::string_view fallback = {}) const;
        double as_double(double fallback = 0.0) const;
        // The number as an integer, or fallback when it is not a number, not
        // finite, has a fraction, or falls outside the target's range
        // (as_uint64: 0 to 2^53, the integers a double holds exactly).
        std::int32_t as_int(std::int32_t fallback = 0) const;
        std::uint64_t as_uint64(std::uint64_t fallback = 0) const;
        bool as_bool(bool fallback = false) const;
    };

    // Parses `text` as exactly one JSON value (RFC 8259) with nothing but
    // whitespace around it, nested no deeper than 64. Returns std::nullopt on
    // any malformed input rather than throwing.
    std::optional<Value> parse(std::string_view text);

    // Standard base64 (RFC 4648 section 4) with padding and nothing else: no
    // whitespace, no base64url, no missing padding. std::nullopt otherwise, so
    // a bad payload is reported rather than decoded into wrong bytes.
    std::optional<std::vector<std::uint8_t>> base64_decode(std::string_view text);
    std::string base64_encode(const std::uint8_t* data, std::size_t size);
}
