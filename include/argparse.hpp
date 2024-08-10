#pragma once

#include <vector>
#include <string>
#include <variant>
#include <unordered_map>
#include <optional>
#include <stdexcept>
#include <memory>
#include <unordered_set>
#include <iostream>
#include <assert.h>


namespace argparse {

struct Int {
    std::optional<int> default_value;
    Int() {}
    Int(int default_value): default_value(default_value) {}
};

struct Double {
    std::optional<double> default_value;
    Double() {}
    Double(double default_value): default_value(default_value) {}
};

struct NoValue {};

struct String {
    std::string default_value;
    std::vector<std::string> choices;
    String() {}
    String(const std::string& default_value): default_value(default_value) {}
    String(const std::vector<std::string>& choices): choices(choices) {}
    String(const std::string& default_value, const std::vector<std::string>& choices):
        default_value(default_value), choices(choices)
    {}
};

using FieldType = std::variant<
    Int,
    Double,
    NoValue,
    String
>;


struct Field {
    std::string label;
    FieldType type;
    char short_flag;
    bool optional;
    std::string help;
    Field():
        short_flag(0),
        optional(false)
    {}
};

using Value = std::variant<
    int,
    double,
    bool,
    std::string
>;

struct ResultValue {
    std::optional<Value> value;
    bool optional;
    ResultValue(const std::optional<Value>& value, bool optional):
        value(value), optional(optional)
    {}
};

class Result {
public:
    class LookupError: public std::runtime_error {
    public:
        LookupError(const std::string& message):
            std::runtime_error(message)
        {}
    };

    template <typename T>
    const T& get(const std::string& label) const {
        auto iter = values.find(label);
        if (iter == values.end()) {
            throw LookupError("No field with label '" + label + "'");
        }
        if (iter->second.optional) {
            throw LookupError("operator[] should only be used for non-optional fields");
        }
        auto ptr = std::get_if<T>(&iter->second.value.value());
        if (!ptr) {
            throw LookupError("Field '" + label + "' doesn't match requested type");
        }
        return *ptr;
    };

#if 0
    template <typename T>
    const std::optional<T>& operator[](const std::string& label) const {
        auto iter = values.find(label);
        if (iter == values.end()) {
            throw LookupError("No field with label '" + label + "'");
        }
        if (!iter->second.optional) {
            throw LookupError("operator[] should only be used for non-optional fields");
        }
        auto ptr = std::get_if<T>(&iter->second.value);
        if (!ptr) {
            throw LookupError("Field '" + label + "' doesn't match requested type");
        }
        return *ptr;
    };
#endif

    const std::string& subcommand() const {
        if (!subcommand_.has_value()) {
            throw LookupError("Parser has no subcommand");
        }
        return subcommand_.value().first;
    }

    const Result& subcommand_result(const std::string& name) const {
        if (!subcommand_.has_value()) {
            throw LookupError("Parser has no subcommand");
        }
        return *subcommand_.value().second;
    }

private:
    std::unordered_map<std::string, ResultValue> values;
    std::optional<std::pair<std::string, std::unique_ptr<Result>>> subcommand_;

    friend class Parser;
};

class Parser {
public:
    class UsageError: public std::runtime_error {
    public:
        UsageError(const std::string& message):
            std::runtime_error(message)
        {}
    };

    void arg(
            const std::string& label,
            const FieldType& type,
            bool optional = false,
            const std::string& help = "")
    {
        if (label.empty()) {
            throw UsageError("Label cannot be empty");
        }
        if (labels.count(label) != 0) {
            throw UsageError("Duplicate label");
        }
        if (!subparsers.empty() && optional) {
            throw UsageError("Cannot have optional arguments and a subparser");
        }
        if (!optional && !args.empty() && args.back().optional) {
            throw UsageError("A required argument cannot follow an optional argument");
        }
        if (std::get_if<NoValue>(&type)) {
            throw UsageError("Cannot have an argument with no value");
        }

        Field field;
        field.label = label;
        field.optional = optional;
        field.help = help;

        args.push_back(field);
        labels.insert(label);
    }

    void flag(
        const std::string& label,
        const FieldType& type,
        bool required = true,
        const std::string& help = "")
    {
        if (label.empty()) {
            throw UsageError("Label cannot be empty");
        }
        if (labels.count(label) != 0) {
            throw UsageError("Duplicate label");
        }

        Field field;
        field.label = label;
        field.type = type;
        field.help = help;

        flags.push_back(field);
        labels.insert(label);
    }

    void flag(
        const std::string& label,
        char short_flag,
        const FieldType& type,
        bool required = true,
        const std::string& help = "")
    {
        if (label.empty()) {
            throw UsageError("Label cannot be empty");
        }
        if (labels.count(label) != 0) {
            throw UsageError("Duplicate label");
        }

        Field field;
        field.label = label;
        field.type = type;
        field.short_flag = short_flag;
        field.help = help;

        flags.push_back(field);
        labels.insert(label);
        short_flags.insert(short_flag);
    }

    Parser& subparser(const std::string& name) {
        if (!args.empty() && args.back().optional) {
            throw UsageError("Cannot have optional arguments and a subparser");
        }
        auto iter = subparsers.emplace(name, std::make_unique<Parser>());
        if (!iter.second) {
            throw UsageError("Duplicate subparser name");
        }
        return *iter.first->second;
    }

    std::string help_message() const;
    std::optional<Result> parse(int argc, char** argv) const;

private:
    const Field* lookup_flag(const std::string& label) const {
        for (const auto& field: flags) {
            if (field.label == label) {
                return &field;
            }
        }
        return nullptr;
    }

    const Field* lookup_flag(char short_flag) const {
        for (const auto& field: flags) {
            if (field.short_flag == short_flag) {
                return &field;
            }
        }
        return nullptr;
    }

    std::optional<Value> parse_value(const std::string_view& word, const FieldType& field_type) const {
        if (auto field = std::get_if<String>(&field_type)) {
            if (field->choices.empty()) {
                return std::string(word);
            }
            if (std::find(field->choices.begin(), field->choices.end(), std::string(word)) == field->choices.end()) {
                std::cout << "Value '" << word << "' not a valid choice, options are: [";
                for (std::size_t i = 0; i < field->choices.size(); i++) {
                    std::cout << field->choices[i];
                    if (i+1 != field->choices.size()) {
                        std::cout << ", ";
                    }
                }
                std::cout << "]\n";
                return std::nullopt;
            }
            return std::string(word);
        }
        if (auto field = std::get_if<Int>(&field_type)) {
            try {
                int result = std::stoi(std::string(word));
                return result;
            } catch (const std::invalid_argument&){
                std::cout << "Invalid integer value '" << word << "'";
                return std::nullopt;
            }
        }
        if (auto field = std::get_if<Int>(&field_type)) {
            try {
                int result = std::stoi(std::string(word));
                return result;
            } catch (const std::invalid_argument&){
                std::cout << "Invalid integer value '" << word << "'";
                return std::nullopt;
            }
        }
        if (auto field = std::get_if<Double>(&field_type)) {
            try {
                int result = std::stoi(std::string(word));
                return result;
            } catch (const std::invalid_argument&){
                std::cout << "Invalid integer value '" << word << "'";
                return std::nullopt;
            }
        }
        assert(false);
        return std::nullopt;
    }

    std::unordered_set<std::string> labels;
    std::unordered_set<char> short_flags;
    std::vector<Field> args;
    std::vector<Field> flags;
    std::unordered_map<std::string, std::unique_ptr<Parser>> subparsers;
};

} // namespace argparse
