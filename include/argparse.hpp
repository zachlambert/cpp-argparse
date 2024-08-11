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
#include <tuple>


namespace argparse {

template <typename T, bool Optional>
using select_opt = std::conditional_t<Optional, std::optional<T>, T>;

template <typename T>
concept is_simple_t =
    std::is_same_v<T, int>
    || std::is_same_v<T, double>
    || std::is_same_v<T, std::string>;

template <typename T>
concept is_optional_t =
    std::is_same_v<T, std::optional<int>>
    || std::is_same_v<T, std::optional<double>>
    || std::is_same_v<T, std::optional<std::string>>
    || std::is_same_v<T, bool>
    || std::is_same_v<T, std::vector<std::string>>;

template <typename T>
concept is_output_t = is_simple_t<T> || is_optional_t<T>;

using output_ptr_t = std::variant<
    int*,
    std::optional<int>*,
    double*,
    std::optional<double>*,
    std::string*,
    std::optional<std::string>*,
    bool*,
    std::vector<std::string>*
>;

class UsageError: public std::runtime_error {
public:
    UsageError(const std::string& message):
        std::runtime_error(message)
    {}
};

enum class ItemType {
    Arg,
    Flag
};

struct Item {
    output_ptr_t output;
    std::string identifier;
    ItemType type;
    bool has_default;
    bool is_optional;
    std::vector<std::string> choices;
    std::string help;
};

template <typename T>
class ItemHandle {
public:
    ItemHandle(T* output, Item* item):
        output(output),
        item(item)
    {}
    template <typename S>
    requires std::is_convertible_v<S, T>
    ItemHandle& default_value(const S& value) {
        *output = value;
        item->has_default = true;
        return *this;
    }
    ItemHandle& choices(const std::vector<std::string>& choices) {
        if (choices.empty()) {
            throw UsageError("Choices cannot be empty");
        }
        item->choices = choices;
        return *this;
    }
    ItemHandle& help(const std::string& help) {
        item->help = help;
        return *this;
    }
private:
    T* output;
    Item* item;
};

class Parser {
public:
    Parser(const std::string& description = ""):
        description(description)
    {}

    template <is_output_t T>
    ItemHandle<T> add(T& output, const std::string& identifier) {
        Item item;
        item.output = &output;
        item.identifier = identifier;
        item.type = parse_identifier(item.identifier);
        item.is_optional = is_optional_t<T>;
        item.has_default = is_optional_t<T>;

        // Special cases
        if constexpr(std::is_same_v<T, bool>) {
            if (item.type != ItemType::Flag) {
                throw UsageError("Args cannot take boolean values");
            }
            output = false;
        }
        if constexpr(std::is_same_v<T, std::vector<std::string>>) {
            output.clear();
        }

        items.push_back(item);
        return ItemHandle(&output, &items.back());
    }

    [[nodiscard]] bool parse(const char* program, const std::span<const char*>& words) const;

    [[nodiscard]] bool parse(int argc, const char** argv) const {
        return parse(argv[0], std::span<const char*>(argv+1, argc-1));
    }

private:
    std::string help_message(const char* program) const;
    ItemType parse_identifier(const std::string& identifier);

    const std::string description;
    std::vector<Item> items;
    std::unordered_map<std::string, std::size_t> flags;
    std::vector<std::size_t> args;
};

} // namespace argparse
