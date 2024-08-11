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
#include <functional>


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

class Parser;

class Args {
public:
    virtual std::string description() const { return ""; }
    virtual void build(Parser& parser) = 0;
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

    template <typename ArgsT, typename OutputT>
    requires std::is_base_of_v<Args, ArgsT> && std::convertible_to<ArgsT, OutputT>
    void subcommand(OutputT& output, const std::string& name, const std::string& description="") {
        Subcommand subcommand;
        subcommand.name = name;
        subcommand.description = description;
        subcommand.callback = [&output](const char* program, std::span<const char*> words) {
            ArgsT args;
            Parser parser;
            ((Args&)args).build(parser);
            if (!parser.parse(program, words)) {
                return false;
            }
            output = args;
            return true;
        };
        subcommands.push_back(subcommand);
    };

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

    struct Subcommand {
        std::string name;
        std::string description;
        std::function<bool(const char*, std::span<const char*>)> callback;
    };
    std::vector<Subcommand> subcommands;
};

[[nodiscard]] inline bool parse(int argc, const char** argv, Args& args)  {
    Parser parser(args.description());
    args.build(parser);
    return parser.parse(argc, argv);
}


} // namespace argparse
