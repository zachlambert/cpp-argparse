#include "argparse.hpp"
#include <sstream>

namespace argparse {

bool parse_word(
    const std::string& word,
    const std::vector<std::string>& choices,
    int& value)
{
    try {
        value = std::stoi(word);
        return true;
    } catch (const std::invalid_argument&) {
        std::cout << "Invalid integer argument '" << word << "'\n";
        return false;
    }
}

bool parse_word(
    const std::string& word,
    const std::vector<std::string>& choices,
    std::optional<int>& value)
{
    value.emplace();
    return parse_word(word, choices, value.value());
}

bool parse_word(
    const std::string& word,
    const std::vector<std::string>& choices,
    double& value)
{
    try {
        value = std::stod(word);
        return true;
    } catch (const std::invalid_argument&) {
        std::cout << "Invalid integer argument '" << word << "'\n";
        return false;
    }
}

bool parse_word(
    const std::string& word,
    const std::vector<std::string>& choices,
    std::optional<double>& value)
{
    value.emplace();
    return parse_word(word, choices, value.value());
}

bool parse_word(
    const std::string& word,
    const std::vector<std::string>& choices,
    std::string& value)
{
    if (!choices.empty()) {
        auto iter = std::find(choices.begin(), choices.end(), word);
        if (iter == choices.end()) {
            std::cout << "Invalid value '" << word << "', not a valid choice\n";
            return false;
        }
    }
    value = word;
    return true;
}

bool parse_word(
    const std::string& word,
    const std::vector<std::string>& choices,
    std::optional<std::string>& value)
{
    value.emplace();
    return parse_word(word, choices, value.value());
}

bool Parser::parse(
    const std::string& program,
    const std::span<const char*>& words) const
{
    bool have_list_arg = false;
    bool have_optional_arg = false;
    for (const auto& item: items) {
        if (item.type == ItemType::Flag) continue;
        if (!item.is_optional && have_optional_arg){
            throw UsageError("Cannot have a required argument following an optional argument");
        }
        have_optional_arg |= item.is_optional;
        if (have_list_arg) {
            throw UsageError("List argument must be the final argument");
        }
        if (std::get_if<std::vector<std::string>*>(&item.output)) {
            have_list_arg = true;
            continue;
        }
    }
    if (have_optional_arg && !subcommands.empty()) {
        throw UsageError("Cannot have an optional arg and subcommands");
    }
    if (have_list_arg && !subcommands.empty()) {
        throw UsageError("Cannot have a list arg and subcommands");
    }

    std::size_t word_i = 0;
    std::size_t arg_i = 0; // Positional argument
    std::vector<Subcommand>::const_iterator subcommand = subcommands.end();

    std::vector<bool> item_has_value;
    for (const auto& item: items) {
        item_has_value.push_back(item.has_default);
    }

    while (word_i < words.size()) {
        std::string word = words[word_i];
        word_i++;

        assert(!word.empty());
        bool is_flag = (word[0] == '-');

        if (word == "-h" || word == "--help") {
            std::cout << help_message(program) << std::endl;
            return false;
        }

        std::size_t item_i;
        if (!is_flag) {
            if (arg_i == args.size()) {
                if (!subcommands.empty()) {
                    subcommand = std::find_if(
                        subcommands.begin(), subcommands.end(),
                        [&word](const Subcommand& value) {
                            return value.name == word;
                        }
                    );
                    if (subcommand == subcommands.end()) {
                        std::cout << "Invalid subcommand '" << word << "'\n";
                        return false;
                    }
                    break;
                }
                std::cout << "Extra position argument '" << word << "'\n";
                std::cout << "\n" << help_message(program) << std::endl;
                return false;
            }
            item_i = args[arg_i];
            arg_i++;
        } else {
            auto iter = flags.find(word);
            if (iter == flags.end()) {
                std::cout << "Unknown flag '" << word << "'\n";
                std::cout << "\n" << help_message(program) << std::endl;
                return false;
            }
            item_i = iter->second;
        }

        const Item& item = items[item_i];
        item_has_value[item_i] = true;

        if (auto output = std::get_if<bool*>(&item.output)) {
            assert(is_flag);
            **output = true;
            continue;
        }

        if (is_flag) {
            if (word_i == words.size()) {
                std::cout << "Expected value after flag '" << word << "'\n";
                std::cout << "\n" << help_message(program) << std::endl;
                return false;
            }
            word = words[word_i];
            word_i++;
        }

        if (auto output = std::get_if<std::vector<std::string>*>(&item.output)) {
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
                if constexpr(!std::is_same_v<T, bool> && !std::is_same_v<T, std::vector<std::string>>) {
                    return parse_word(word, item.choices, *output);
                }
                assert(false);
                return false;
            }, item.output);
            if (!valid) {
                std::cout << "\n" << help_message(program) << std::endl;
                return false;
            }
        }
    }

    for (std::size_t i = 0; i < items.size(); i++) {
        if (item_has_value[i]) continue;
        std::cout << "Missing value for '" << items[i].identifier << "'\n";
        std::cout << "\n" << help_message(program) << std::endl;
        return false;
    }

    if (subcommand != subcommands.end()) {
        if (!subcommand->callback(program, words.subspan(word_i))) {
            return false;
        }
    } else if (subcommand_required) {
        std::cout << "Missing subcommand\n";
        return false;
    }

    return true;
}

std::string Parser::help_message(const std::string& program) const {
    std::stringstream ss;
    ss << program;
    if (!description.empty()) {
        ss << " - " << description;
    }
    ss << "\n";

    ss << "\033[1mUSAGE:\033[0m " << program;

    // [optional args]
    // <required args>
    // {default values}

    auto print_item = [&ss](const Item& item, bool initial_space=true) {
        if (initial_space) {
            ss << " ";
        }
        if (!item.has_default) {
            ss << "<" << item.identifier << ">";
            return;
        }
        ss << "[" << item.identifier;
        if (std::get_if<std::vector<std::string>*>(&item.output)) {
            ss << "...";
        } else {
            std::visit([&](const auto& output) {
                using T = std::decay_t<decltype(*output)>;
                if constexpr(!is_optional_t<T>) {
                    ss << " {" << *output << "}";
                }
            }, item.output);
        }
        ss << "]";
    };

    bool have_required_flag = false;
    bool have_optional_flag = false;
    bool have_arg = false;
    for (const auto& item: items) {
        have_required_flag |= (item.type == ItemType::Flag && !item.is_optional);
        have_required_flag |= (item.type == ItemType::Flag && item.is_optional);
        have_arg |= (item.type == ItemType::Arg);
    }

    bool have_flag_help = false;
    bool have_arg_help = false;

    // Required flags <--flag|-f>
    if (have_required_flag) {
        for (const auto& item: items) {
            if (item.type != ItemType::Flag) continue;
            if (item.has_default) continue;
            print_item(item);
            have_flag_help |= !item.help.empty() || !item.choices.empty();
        }
    }
    // Optional flags [--flag|-f] {default}
    if (have_optional_flag) {
        for (const auto& item: items) {
            if (item.type != ItemType::Flag) continue;
            if (!item.has_default) continue;
            print_item(item);
            have_flag_help |= !item.help.empty() || !item.choices.empty();
        }
    }
    // Args (note: guaranteed to have required args first)
    if (have_arg) {
        for (const auto& item: items) {
            if (item.type != ItemType::Arg) continue;
            print_item(item);
            have_arg_help |= !item.help.empty() || !item.choices.empty();
        }
    }
    // Subcommand
    if (!subcommands.empty()) {
        if (subcommand_required) {
            ss << "\n  <subcommand>";
        } else {
            ss << "\n  [subcommand]";
        }
    }
    ss << "\n";

    auto print_help = [&ss](const std::string& help) {
        if (help.empty()) return;
        ss << "    " << help << "\n";
    };

    auto print_choices = [&ss](const std::vector<std::string>& choices) {
        if (choices.empty()) return;
        ss << "    Choices: [";
        for (std::size_t i = 0; i < choices.size(); i++) {
            ss << choices[i];
            if (i+1 != choices.size()) {
                ss << ", ";
            }
        }
        ss << "]\n";
    };

    if (have_flag_help) {
        ss << "\n\033[1mFLAGS:\033[0m\n";
        for (const auto& item: items) {
            if (item.type != ItemType::Flag) continue;
            if (item.has_default) continue;
            if (item.help.empty() && item.choices.empty()) continue;
            ss << "  ";
            print_item(item, false);
            ss << "\n";
            print_help(item.help);
            print_choices(item.choices);
        }
        for (const auto& item: items) {
            if (item.type != ItemType::Flag) continue;
            if (!item.has_default) continue;
            if (item.help.empty() && item.choices.empty()) continue;
            ss << "  ";
            print_item(item, false);
            ss << "\n";
            print_help(item.help);
            print_choices(item.choices);
        }
    }
    if (have_arg_help) {
        ss << "\n\033[1mARGUMENTS:\033[0m\n";
        for (const auto& item: items) {
            if (item.type != ItemType::Arg) continue;
            if (item.help.empty() && item.choices.empty()) continue;
            ss << "  ";
            print_item(item, false);
            ss << "\n";
            print_help(item.help);
            print_choices(item.choices);
        }
    }
    if (!subcommands.empty()) {
        ss << "\n\033[1mSUBCOMMAND:\033[0m";
        if (!subcommand_required) {
            ss << " (optional)";
        }
        ss << "\n";
        for (const auto& subcommand: subcommands) {
            ss << "  " << subcommand.name;
            if (!subcommand.description.empty()) {
                ss << "  " << subcommand.description;
            }
            ss << "\n";
        }
    }

    return ss.str();
}

ItemType Parser::parse_identifier(const std::string& identifier) {
    auto validate_word = [](const std::string& word) -> bool {
        if (word.empty()) return false;
        if (!std::isalpha(word[0])) return false;
        for (char c: word) {
            if (!std::isalnum(c) && c != '-' && c != '_') {
                return false;
            }
        }
        return true;
    };

    assert(!identifier.empty());
    if (identifier[0] != '-') {
        if (!validate_word(identifier)) {
            throw UsageError("Invalid identifier '" + identifier + "'");
        }
        args.push_back(items.size());
        return ItemType::Arg;
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
            if (!validate_word(part.substr(2))) {
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
        flags.emplace(part, items.size());
    }

    return ItemType::Flag;
}

} // namespace argparse
