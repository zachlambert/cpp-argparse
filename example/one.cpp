#include "argparse.hpp"
#include <iostream>

int main(int argc, char** argv) {
    argparse::Parser parser;

    parser.flag("message", argparse::String(), true);

    auto result_opt = parser.parse(argc, argv);
    if (!result_opt.has_value()) {
        return 1;
    }
    const argparse::Result& result = result_opt.value();

    std::cout << result.get<std::string>("message") << std::endl;

    return 0;
}
