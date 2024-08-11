#pragma once

#include <concepts>
#include <variant>
#include <string>
#include <optional>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <assert.h>
#include <span>
#include <iostream>


namespace argparse {

using ValuePtr = std::variant<
>;

using OptValuePtr = std::variant<
>;


using FlagPtr = bool*;

using FieldPtr = std::variant<
    int*,
    double*,
    std::string*,
    std::optional<int>*,
    std::optional<double>*,
    std::optional<std::string>*,
    std::vector<std::string>*,
    bool*
>;

template <typename T>
concept field_t = requires(T t) {
    { &t } -> std::convertible_to<FieldPtr>;
};

template <typename T>
concept is_opt_field =
    std::is_same_v<T, std::optional<int>>
    || std::is_same_v<T, std::optional<double>>
    || std::is_same_v<T, std::optional<std::string>>
    || std::is_same_v<T, std::vector<std::string>>
    || std::is_same_v<T, bool>;

template <typename T>
concept is_string_field =
    std::is_same_v<T, std::string>
    || std::is_same_v<T, std::optional<std::string>>;

template <typename T>
concept is_int_field =
    std::is_same_v<T, int>
    || std::is_same_v<T, std::optional<int>>;

template <typename T>
concept is_double_field =
    std::is_same_v<T, double>
    || std::is_same_v<T, std::optional<double>>;

struct Element {
    std::string identifier;
    FieldPtr output;
    std::vector<std::string> choices;
    bool has_default;
    bool is_optional;
    Element(const std::string& identifier, const FieldPtr& output):
        identifier(identifier), output(output), has_default(false), is_optional(false)
    {}
};

// TODO:
// - Store default values in order to print them in help message
// - Throw usage error if any flag/arg follows a string list positional arg

class Parser {
public:
    class UsageError: public std::runtime_error {
    public:
        UsageError(const std::string& message):
            std::runtime_error(message)
        {}
    };

    template <field_t T>
    void add(
        T& output,
        const std::string& identifier)
    {
        Element element(identifier, &output);
        element.is_optional = is_opt_field<T>;
        element.has_default = is_opt_field<T>;
        if constexpr(std::is_same_v<bool, T>) {
            output = false;
        }
        add_identifier(identifier);

        elements.push_back(element);
    }

    template <field_t T, typename DefaultT>
    requires std::is_convertible_v<DefaultT, T>
    void add(
        T& output,
        const std::string& identifier,
        const DefaultT& default_value)
    {
        static_assert(!is_opt_field<T>);

        Element element(identifier, &output);
        element.has_default = true;
        output = default_value;

        add_identifier(identifier);
        elements.push_back(element);
    }

    template <field_t T, typename DefaultT>
    requires (std::is_same_v<DefaultT, std::nullopt_t> || std::is_convertible_v<DefaultT, T>)
    void add(
        T& output,
        const std::string& identifier,
        const DefaultT& default_value,
        const std::vector<std::string>& choices)
    {
        static_assert(
            std::is_same_v<T, std::string>
            || std::is_same_v<T, std::optional<std::string>>
            || std::is_same_v<T, std::vector<std::string>>);

        Element element(identifier, &output);
        element.choices = choices;
        if constexpr(!std::is_same_v<DefaultT, std::nullopt_t>) {
            auto iter = std::find(choices.begin(), choices.end(), default_value);
            if (iter == choices.end()) {
                throw UsageError("Invalid default '" + std::string(default_value) + "', not one of the given choices");
            }
            output = default_value;
            element.has_default = true;
        }
        if constexpr(std::is_same_v<DefaultT, std::nullopt_t>) {
            element.has_default = is_opt_field<T>;
        }
        element.is_optional = is_opt_field<T>;

        add_identifier(identifier);
        elements.push_back(element);
    }

    [[nodiscard]] bool parse(const std::span<const char*>& words) const;

    [[nodiscard]] bool parse(int argc, const char** argv) const {
        return parse(std::span<const char*>(argv+1, argc-1));
    }

private:
    bool validate_label(const std::string& label) const;
    void add_identifier(const std::string& identifier);

    std::vector<Element> elements;
    std::unordered_map<std::string, std::size_t> flags;
    std::vector<std::size_t> args;
    bool have_list_arg = false;
};

} // namespace argparse
