#include "argparse2.hpp"
#include <iostream>

int main(int argc, const char** argv) {
    std::string foo;
    std::string bar;

    argparse::Parser parser;
    parser.add("foo", argparse::Flag(), argparse::String(foo));
    parser.add("bar", argparse::Flag(), argparse::String(bar, "asdf"));
    parser.add_subcommand("add");

    if (!parser.parse(argc, argv)) {
        return 1;
    }
    std::cout << "foo: " << foo << std::endl;
    std::cout << "bar: " << bar << std::endl;

    if (parser.subcommand() == "add") {
        argparse::Parser subparser;
        int a;
        int b;
        subparser.add("a", argparse::Arg(), argparse::Int(a));
        subparser.add("b", argparse::Arg(), argparse::Int(b));
        subparser.parse(parser.subargs());

        std::cout << "sum: " << a + b << std::endl;
    }

    return 0;
}
