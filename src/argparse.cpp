#include "argparse.hpp"
#include <cstring>
#include <iostream>
#include <unordered_set>
#include <assert.h>


namespace argparse {

std::string Parser::help_message() const {
    std::string message = "TODO";
    return message;
}

std::optional<Result> Parser::parse(int argc, char** argv) const {
    Result result;

    std::unordered_set<std::string> set_flags;
    int arg_index = 0;

    int pos = 1;
    while (pos < argc) {
        std::string word = argv[pos];
        pos++;

        if (word.size() >= 1 && word[0] == '-') {
            const Field* field = nullptr;
            if (word.size() >= 2 && word[1] == '-'){
                // Full flag
                std::string label(word.substr(2));
                field = lookup_flag(label);
            } else {
                // Short flag
                field = lookup_flag(word[1]);
            }

            if (!field) {
                std::cout << "Encountered unknown flag " << word << "\n";
                return std::nullopt;
            }
            set_flags.insert(field->label);

            if (std::get_if<NoValue>(&field->type)) {
                result.values.emplace(field->label, ResultValue(true, false));
                continue;
            }

            if (pos == argc) {
                std::cout << "Expected a value after flag '" << word << "'\n";
                return std::nullopt;
            }
            auto value = parse_value(argv[pos], field->type);
            pos++;

            if (!value.has_value()) {
                std::cout << "Invalid value after flag '" << word << "'\n";
                return std::nullopt;
            }
            result.values.emplace(field->label, ResultValue(value.value(), field->optional));
            continue;
        }

        if (arg_index < args.size()) {
            const Field& field = args[arg_index];
            auto value = parse_value(argv[pos], field.type);
            if (!value.has_value()) {
                std::cout << "Invalid value for arg '" << args[arg_index].label << "'\n";
                return std::nullopt;
            }
            result.values.emplace(field.label, ResultValue(value.value(), field.optional));
            arg_index++;
            continue;

        } else if (arg_index == args.size() && !subparsers.empty()){
            const std::string subparser_name(word);
            auto iter = subparsers.find(subparser_name);
            if (iter == subparsers.end()) {
                std::cout << "Invalid subparser '" << word << "'\n";
                return std::nullopt;
            }
            auto subparser_result = iter->second->parse(argc - pos, argv + pos);
            if (!subparser_result.has_value()) {
                return std::nullopt;
            }
            result.subcommand_.emplace(
                subparser_name,
                std::make_unique<Result>(std::move(subparser_result.value()))
            );

        } else {
            std::cout << "Unexpected extra positional argument '" << word << "'\n";
            return std::nullopt;
        }
    }

    while (arg_index < args.size()) {
        const Field& field = args[arg_index];
        if (!field.optional) {
            std::cout << "Missing value for arg " << field.label << "\n";
            return std::nullopt;
        } else {
            assert(!std::get_if<NoValue>(&field.type));
            std::visit([&](const auto& type) {
                using T = std::decay_t<decltype(type)>;
                if constexpr(!std::is_same_v<T, NoValue>) {
                    result.values.emplace(field.label, ResultValue(type.default_value, field.optional));
                }
            }, field.type);
        }
    }

    for (const auto& field: flags) {
        if (set_flags.contains(field.label)) {
            continue;
        }
        if (!field.optional) {
            std::cout << "Missing value for flag " << field.label << "\n";
            return std::nullopt;
        } else {
            std::visit([&](const auto& type) {
                using T = std::decay_t<decltype(type)>;
                if constexpr(std::is_same_v<T, NoValue>) {
                    result.values.emplace(field.label, ResultValue(false, false));
                }
                if constexpr(!std::is_same_v<T, NoValue>) {
                    result.values.emplace(field.label, ResultValue(type.default_value, field.optional));
                }
            }, field.type);
        }
    }

    return result;
}

} // namespace argparse
