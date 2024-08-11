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

    [[nodiscard]] bool parse(int argc, const char** argv) const {
        return parse(std::span<const char*>(argv+1, argc-1));
    }

    [[nodiscard]] bool parse(const std::span<const char*>& words) const {
        std::size_t word_i = 0;
        std::size_t arg_i = 0; // Positional argument

        std::vector<bool> element_has_value;
        for (const auto& element: elements) {
            element_has_value.push_back(element.has_default);
        }

        while (word_i < words.size()) {
            std::string word = words[word_i];
            word_i++;

            assert(!word.empty());
            bool is_flag = word[0] == '-';

            std::size_t element_i;
            if (!is_flag) {
                if (arg_i == args.size()) {
                    std::cout << "Extra position argument '" << word << "'\n";
                    return false;
                }
                element_i = args[arg_i];
                arg_i++;
            } else {
                auto iter = flags.find(word);
                if (iter == flags.end()) {
                    std::cout << "Unknown flag '" << word << "'\n";
                    return false;
                }
                element_i = iter->second;
            }

            const Element& element = elements[element_i];
            element_has_value[element_i] = true;

            if (auto output = std::get_if<bool*>(&element.output); is_flag && output) {
                **output = true;
                continue;
            }

            if (is_flag) {
                if (word_i == words.size()) {
                    std::cout << "Expected value after flag '" << word << "'\n";
                    return false;
                }
                word = words[word_i];
                word_i++;
            }

            if (auto output = std::get_if<std::vector<std::string>*>(&element.output)) {
                (*output)->clear();
                (*output)->push_back(word);
                while (word_i != words.size()) {
                    word = words[word_i];
                    if (is_flag && word[0] == '-') {
                        break;
                    }
                    (*output)->push_back(word);
                    word_i++;
                }
            }
            else {
                bool valid = std::visit([&](auto output) -> bool {
                    using T = std::decay_t<decltype(*output)>;
                    if constexpr(is_string_field<T>) {
                        if (!element.choices.empty()) {
                            auto iter = std::find(
                                element.choices.begin(),
                                element.choices.end(),
                                word);
                            if (iter == element.choices.end()) {
                                std::cout << "Invalid value '" << word << "', not a valid choice\n";
                                return false;
                            }
                        }
                        *output = word;
                        return true;
                    }
                    if constexpr(is_int_field<T>) {
                        try {
                            *output = std::stoi(word);
                            return true;
                        } catch (const std::invalid_argument&) {
                            std::cout << "Invalid int value '" << word << "'\n";
                            return false;
                        }
                    }
                    if constexpr(is_double_field<T>) {
                        try {
                            *output = std::stod(word);
                            return true;
                        } catch (const std::invalid_argument&) {
                            std::cout << "Invalid int value '" << word << "'\n";
                            return false;
                        }
                    }
                    if constexpr(std::is_same_v<T, bool>) {
                        if (word == "true") {
                            *output = true;
                            return true;
                        } else if (word == "false") {
                            *output = false;
                            return true;
                        } else {
                            std::cout << "Invalid bool value '" << word << "'\n";
                            return false;
                        }
                    }
                    std::cout << word << std::endl;
                    std::cout << is_string_field<T> << std::endl;
                    assert(false);
                    return false;
                }, element.output);

                if (!valid) {
                    return false;
                }
            }
        }

        for (std::size_t i = 0; i < elements.size(); i++) {
            if (element_has_value[i]) continue;
            std::cout << "Missing value for '" << elements[i].identifier << "'\n";
            return false;
        }

        return true;
    }

private:
    bool validate_label(const std::string& label) const {
        if (label.empty()) return false;
        if (!std::isalpha(label[0])) return false;
        for (char c: label) {
            if (!std::isalnum(c) && c != '-' && c != '_') {
                return false;
            }
        }
        return true;
    }

    void add_identifier(const std::string& identifier) {
        assert(!identifier.empty());
        if (identifier[0] != '-') {
            if (!validate_label(identifier)) {
                throw UsageError("Invalid identifier '" + identifier + "'");
            }
            args.push_back(elements.size());
            return;
        }

        std::size_t part_begin = 0;
        while (part_begin != std::string::npos) {
            std::size_t part_end = identifier.find('|', part_begin);
            std::string part = identifier.substr(part_begin, part_end);
            part_begin = part_end == std::string::npos ? std::string::npos : part_end + 1;

            if (part == "-h" || part == "--help") {
                throw UsageError("Cannot use flags '-h' and '--help', reserved for printing help message");
            }
            if (part.size() >= 2 && part[1] == '-') {
                if (!validate_label(part.substr(2))) {
                    throw UsageError("Invalid flag '" + part + "'");
                }
            } else {
                if (part.size() != 2 && !std::isalpha(part[1]))  {
                    throw UsageError("Invalid flag '" + part + "'");
                }
            }

            auto iter = flags.find(part);
            if (iter != flags.end()) {
                throw UsageError("Duplicate flag '" + part + "'");
            }
            flags.emplace(part, elements.size());
        }
    }

    std::vector<Element> elements;
    std::unordered_map<std::string, std::size_t> flags;
    std::vector<std::size_t> args;
    bool have_list_arg = false;
};

} // namespace argparse
