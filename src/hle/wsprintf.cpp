#include "re2dj/hle/wsprintf.h"

namespace re2dj::hle
{
namespace
{

enum class Size
{
    kNormal,
    kShort,
    kLong,
    kWide,
    kInt64,
};

struct Specification
{
    bool left = false;
    bool prefix = false;
    bool zero = false;
    std::size_t width = 0;
    // -1 when none was given.
    int precision = -1;
    Size size = Size::kNormal;
};

bool IsDigit(char character)
{
    return character >= '0' && character <= '9';
}

std::string Digits(std::uint64_t magnitude, unsigned base, bool upper)
{
    const char* symbols = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    std::string digits;
    do
    {
        digits.insert(digits.begin(), symbols[magnitude % base]);
        magnitude /= base;
    } while (magnitude != 0);
    return digits;
}

// Pads body to the width with spaces, on the left unless left-justified, and
// puts the 0x prefix between the spaces and the body: the width counts the
// body only.
std::string Pad(const Specification& spec, const std::string& prefix, std::string body)
{
    const std::string spaces(body.size() < spec.width ? spec.width - body.size() : 0, ' ');
    return spec.left ? prefix + body + spaces : spaces + prefix + body;
}

// d i u x X p: digits zero-padded to the precision (at least one digit),
// then to the width when '0' was given without a precision or '-'.
std::string FormatNumber(const Specification& spec, char type, std::uint64_t value, bool negative)
{
    const bool hex = type == 'x' || type == 'X' || type == 'p';
    const bool upper = type == 'X' || type == 'p';
    std::string digits = Digits(value, hex ? 16 : 10, upper);
    if (spec.precision > 0 && digits.size() < static_cast<std::size_t>(spec.precision))
    {
        digits.insert(0, static_cast<std::size_t>(spec.precision) - digits.size(), '0');
    }
    const std::string sign = negative ? "-" : "";
    if (spec.zero && !spec.left && spec.precision < 0 && sign.size() + digits.size() < spec.width)
    {
        digits.insert(0, spec.width - sign.size() - digits.size(), '0');
    }
    const std::string prefix = spec.prefix && hex ? (upper ? "0X" : "0x") : "";
    return Pad(spec, prefix, sign + digits);
}

// s and c: zero padding applies to text too, as measured.
std::string FormatText(const Specification& spec, std::string text)
{
    if (spec.zero && !spec.left && text.size() < spec.width)
    {
        text.insert(0, spec.width - text.size(), '0');
    }
    return Pad(spec, "", std::move(text));
}

}  // namespace

bool FormatWsprintf(std::string_view format,
                    const WsprintfWordReader& next_word,
                    const WsprintfStringReader& read_string,
                    std::string* output,
                    std::string* error)
{
    output->clear();
    std::size_t at = 0;
    const auto ended = [&]() { return at >= format.size(); };
    while (!ended())
    {
        const char character = format[at++];
        if (character != '%')
        {
            output->push_back(character);
            continue;
        }
        Specification spec;
        while (!ended() && (format[at] == '-' || format[at] == '#'))
        {
            (format[at] == '-' ? spec.left : spec.prefix) = true;
            ++at;
        }
        if (!ended() && format[at] == '0')
        {
            spec.zero = true;
            ++at;
        }
        while (!ended() && IsDigit(format[at]))
        {
            spec.width = spec.width * 10 + static_cast<std::size_t>(format[at++] - '0');
        }
        if (!ended() && format[at] == '.')
        {
            ++at;
            spec.precision = 0;
            while (!ended() && IsDigit(format[at]))
            {
                spec.precision = spec.precision * 10 + (format[at++] - '0');
            }
        }
        if (!ended() && format[at] == 'h')
        {
            spec.size = Size::kShort;
            ++at;
        }
        else if (!ended() && format[at] == 'l')
        {
            spec.size = Size::kLong;
            ++at;
        }
        else if (!ended() && format[at] == 'w')
        {
            spec.size = Size::kWide;
            ++at;
        }
        else if (format.substr(at, 3) == "I64")
        {
            spec.size = Size::kInt64;
            at += 3;
        }
        if (ended())
        {
            break;
        }
        const char type = format[at++];
        const bool wide = spec.size == Size::kLong || spec.size == Size::kWide;
        if (type == 'S' || type == 'C' || ((type == 's' || type == 'c') && wide) ||
            (spec.size == Size::kInt64 && (type == 's' || type == 'c' || type == 'p')))
        {
            *error = std::string("wsprintfA %") + (spec.size == Size::kInt64 ? "I64" : wide ? "l" : "") + type +
                     " is not modelled";
            return false;
        }
        std::uint32_t word = 0;
        const bool takes_argument = type == 'd' || type == 'i' || type == 'u' || type == 'x' || type == 'X' ||
                                    type == 'p' || type == 's' || type == 'c';
        if (takes_argument && !next_word(&word))
        {
            *error = "wsprintfA cannot read its arguments";
            return false;
        }
        std::uint64_t value = word;
        if (spec.size == Size::kInt64 && type != 's' && type != 'c')
        {
            std::uint32_t high = 0;
            if (!next_word(&high))
            {
                *error = "wsprintfA cannot read its arguments";
                return false;
            }
            value |= static_cast<std::uint64_t>(high) << 32;
        }
        switch (type)
        {
        case 'd':
        case 'i':
        {
            std::int64_t signed_value = spec.size == Size::kInt64 ? static_cast<std::int64_t>(value)
                                        : spec.size == Size::kShort
                                            ? static_cast<std::int16_t>(static_cast<std::uint16_t>(word))
                                            : static_cast<std::int32_t>(word);
            const bool negative = signed_value < 0;
            const std::uint64_t magnitude =
                negative ? static_cast<std::uint64_t>(0) - static_cast<std::uint64_t>(signed_value)
                         : static_cast<std::uint64_t>(signed_value);
            *output += FormatNumber(spec, type, magnitude, negative);
            break;
        }
        case 'u':
        case 'x':
        case 'X':
            *output += FormatNumber(spec, type, value, false);
            break;
        case 'p':
            if (spec.precision < 0)
            {
                spec.precision = 8;
            }
            *output += FormatNumber(spec, type, value, false);
            break;
        case 's':
        {
            std::string text;
            if (word != 0 && !read_string(word, &text))
            {
                *error = "wsprintfA cannot read a %s argument";
                return false;
            }
            if (spec.precision >= 0 && text.size() > static_cast<std::size_t>(spec.precision))
            {
                text.resize(static_cast<std::size_t>(spec.precision));
            }
            *output += FormatText(spec, std::move(text));
            break;
        }
        case 'c':
            *output += FormatText(spec, std::string(1, static_cast<char>(word & 0xFFU)));
            break;
        default:
            output->push_back(type);
            break;
        }
    }
    if (output->size() > kWsprintfMaximumOutput)
    {
        output->resize(kWsprintfMaximumOutput);
    }
    error->clear();
    return true;
}

}  // namespace re2dj::hle
