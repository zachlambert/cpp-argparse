#pragma once

#include <string>
#include <optional>
#include <vector>
#include <variant>
#include <type_traits>
#include <stdexcept>
#include <span>
#include <unordered_map>
#include <iostream>
#include <assert.h>


namespace argparse {

struct Flag {
    char short_flag;
    Flag(char short_flag = '\0'): short_flag(short_flag) {}
};
struct Arg {};
using Type = std::variant<Flag, Arg>;

struct String {
    std::string* output;
    std::optional<std::string> default_value;
    std::vector<std::string> choices;
    String(
        std::string& output,
        const std::optional<std::string>& default_value = std::nullopt,
        const std::vector<std::string>& choices = {}
    ):
        output(&output),
        default_value(default_value),
        choices(choices)
    {}
};
struct StringOpt {
    std::optional<std::string>* output;
    std::vector<std::string> choices;
    StringOpt(
        std::optional<std::string>& output,
        const std::vector<std::string>& choices = {}
    ):
        output(&output),
        choices(choices)
    {}
};
struct StringList {
    std::vector<std::string>* output;
    std::vector<std::string> choices;
    StringList(
        std::vector<std::string>& output,
        const std::vector<std::string>& choices = {}
    ):
        output(&output),
        choices(choices)
    {}
};

struct Int {
    int* output;
    std::optional<int> default_value;
    Int(int& output, const std::optional<int>& default_value = std::nullopt):
        output(&output), default_value(default_value)
    {}
};
struct IntOpt {
    std::optional<int>* output;
    IntOpt(std::optional<int>& output): output(&output) {}
};

struct Boolean {
    bool* output;
    bool require_value;
    Boolean(bool& output, bool require_value):
        output(&output), require_value(require_value)
    {}
};

using Field = std::variant<
    String,
    StringOpt,
    StringList,
    Int,
    IntOpt,
    Boolean
>;

class Parser {
    struct Element {
        mutable bool is_set;
        std::string label;
        Type type;
        Field field;
        Element(const std::string& label, const Type& type, const Field& field):
            is_set(false), label(label), type(type), field(field)
        {}
    };
public:
    class UsageError: public std::runtime_error {
    public:
        UsageError(const std::string& message):
            std::runtime_error(message)
        {}
    };

    Parser():
        have_multi_arg(false)
    {}

    bool validate_choice(
        const std::string& value,
        const std::vector<std::string>& choices)
    {
        if (choices.empty()) return true;
        auto iter = std::find(choices.begin(), choices.end(), value);
        return iter != choices.end();
    }

    void add(
        const std::string& label,
        const Type& type,
        const Field& field)
    {
        if (auto field_v = std::get_if<String>(&field)) {
            if (field_v->default_value.has_value()
                && !validate_choice(
                        field_v->default_value.value(),
                        field_v->choices))
            {
                throw UsageError(
                    "Default value '" + field_v->default_value.value() + " is not "
                    + "' for " + label + " is not one of the given choices");
            }
        }
        if (have_multi_arg && std::get_if<Arg>(&type)) {
            throw UsageError("If using a StringList, this must be the last arg");
        }
        if (std::get_if<StringList>(&field)) {
            if (!subcommands.empty()) {
                throw UsageError("Cannot have a StringList and subcommand");
            }
            if (!std::get_if<Arg>(&type)) {
                throw UsageError("Can only use StringList for arg, not flag");
            }
            have_multi_arg = true;
        }

        if (std::get_if<Arg>(&type)) {
            args.push_back(elements.size());
        }
        else {
            const auto& flag = std::get<Flag>(type);
            long_flags.emplace(label, elements.size());
            if (flag.short_flag != '\0') {
                short_flags.emplace(flag.short_flag, elements.size());
            }
        }

        elements.push_back({label, type, field});
    }

    void add_subcommand(const std::string& label) {
        if (have_multi_arg) {
            throw UsageError("Cannot have a StringList and subcommand");
        }
        subcommands.push_back(label);
    }

    bool parse(int argc, const char** argv) {
        return parse(std::span(argv + 1, argc - 1));
    }

    bool parse(const std::span<const char*>& words) {
        std::size_t word_i = 0;
        std::size_t arg_index = 0;

        while (word_i < words.size()) {
            std::string word = words[word_i];
            word_i++;

            std::size_t element_index;
            if (word.size() >= 2 && word.substr(0, 2) == "--") {
                auto iter = long_flags.find(std::string(word.substr(2)));
                if (iter == long_flags.end()) {
                    std::cout << "Unknown long flag '" << word << "'\n";
                    return false;
                }
                element_index = iter->second;
            }
            else if (word[0] == '-') {
                if (word.size() != 2) {
                    std::cout << "Invalid short flag '" << word << "', must be one character\n";
                    return false;
                }
                auto iter = long_flags.find(std::string(word.substr(0, 2)));
                if (iter == long_flags.end()) {
                    std::cout << "Unknown long flag " << word << "\n";
                    return false;
                }
                element_index = iter->second;
            }
            else if (arg_index < args.size()) {
                element_index = args[arg_index];
                arg_index++;
            }
            else if (arg_index == args.size() && !subcommands.empty()) {
                subcommand_ = word;
                subargs_ = std::span(&words[word_i], words.size() - word_i);
                break;
            }
            else {
                std::cout << "Unexpected extra argument '" << word << "'\n";
                return false;
            }

            const Element& element = elements[element_index];

            if (std::get_if<Flag>(&element.type)) {
                auto bool_field = std::get_if<Boolean>(&element.field);
                if (!bool_field || bool_field->require_value) {
                    if (word_i == words.size()) {
                        std::cout << "Expected a value after flag '" << word << "'\n";
                        return false;
                    }
                    word = words[word_i];
                    word_i++;
                } else {
                    *bool_field->output = true;
                    continue;
                }
            }

            if (auto field = std::get_if<String>(&element.field)) {
                if (!validate_choice(word, field->choices)) {
                    std::cout << "Invalid value '" << word << "' for '" << element.label << "'\n";
                    return false;
                }
                element.is_set = true;
                *field->output = word;
            }
            else if (auto field = std::get_if<StringOpt>(&element.field)) {
                if (!validate_choice(word, field->choices)) {
                    std::cout << "Invalid value '" << word << "' for '" << element.label << "'\n";
                    return false;
                }
                element.is_set = true;
                *field->output = word;
            }
            else if (auto field = std::get_if<StringList>(&element.field)) {
                element.is_set = true;
                field->output->clear();
                while (word_i != words.size()) {
                    word = words[word_i];
                    word_i++;
                    if (!validate_choice(word, field->choices)) {
                        std::cout << "Invalid value '" << word << "' for '" << element.label << " " << field->output->size() << "'\n";
                        return false;
                    }
                    field->output->push_back(word);
                }
            }
            else if (auto field = std::get_if<Int>(&element.field)) {
                try {
                    int result = std::stoi(word);
                    *field->output = result;
                } catch (const std::invalid_argument&) {
                    std::cout << "Invalid integer value '" << word << "'\n";
                    return false;
                }
                element.is_set = true;
            }
            else if (auto field = std::get_if<IntOpt>(&element.field)) {
                try {
                    int result = std::stoi(word);
                    *field->output = result;
                } catch (const std::invalid_argument&) {
                    std::cout << "Invalid integer value '" << word << "'\n";
                    return false;
                }
                element.is_set = true;
            }
            else if (auto field = std::get_if<Boolean>(&element.field)) {
                assert(field->require_value);
            }
        }

        for (const auto& element: elements) {
            if (element.is_set) continue;
            if (std::get_if<StringOpt>(&element.field)) continue;
            if (auto value = std::get_if<StringList>(&element.field)) {
                value->output->clear();
                continue;
            }
            if (std::get_if<IntOpt>(&element.field)) continue;
            if (auto field = std::get_if<Boolean>(&element.field)) {
                if (!field->require_value) {
                    *field->output = false;
                    continue;
                }
            }
            if (auto field = std::get_if<String>(&element.field)) {
                if (field->default_value.has_value()) {
                    *field->output = field->default_value.value();
                    continue;
                }
            }
            if (auto field = std::get_if<Int>(&element.field)) {
                if (field->default_value.has_value()) {
                    *field->output = field->default_value.value();
                    continue;
                }
            }
            std::cout << "Missing value for '" << element.label << "'\n";
            return false;
        }

        return true;
    }

    const std::string& subcommand() const {
        return subcommand_;
    }

    const std::span<const char*>& subargs() const {
        return subargs_;
    }

private:
    std::vector<Element> elements;
    bool have_multi_arg;

    std::unordered_map<std::string, std::size_t> long_flags;
    std::unordered_map<char, std::size_t> short_flags;
    std::vector<std::size_t> args;
    std::vector<std::string> subcommands;

    std::string subcommand_;
    std::span<const char*> subargs_;
};

} // namespace argparse
