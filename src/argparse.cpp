#include "argparse.hpp"

namespace argparse {

[[nodiscard]] bool Parser::parse(const std::span<const char*>& words) const {
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

bool Parser::validate_label(const std::string& label) const {
    if (label.empty()) return false;
    if (!std::isalpha(label[0])) return false;
    for (char c: label) {
        if (!std::isalnum(c) && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

void Parser::add_identifier(const std::string& identifier) {
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

} // namespace argparse
