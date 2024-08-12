#include <argparse.hpp>

using T = argparse::Args;

struct AddCommand: public argparse::Args {
    int a;
    int b;
private:
    std::string description() const override {
        return "Add two numbers";
    }
    void build(argparse::Parser& parser) override {
        parser.add(a, "a").help("First argument");
        parser.add(b, "b").help("Second argument");
    }
};

struct NegateCommand: public argparse::Args {
    int value;
private:
    std::string description() const override {
        return "Negate a number";
    }
    void build(argparse::Parser& parser) override {
        parser.add(value, "value").help("Argument");
    }
};

struct CliArgs: public argparse::Args {
    using Command = std::variant<AddCommand, NegateCommand>;
    Command command;
private:
    void build(argparse::Parser& parser) override {
        parser.subcommand(command)
            .add<AddCommand>("add")
            .add<NegateCommand>("negate");
    }
};

int main(int argc, const char** argv) {
    CliArgs args;
    if (!argparse::parse(argc, argv, args)) {
        return 1;
    }
    if (auto command = std::get_if<AddCommand>(&args.command)) {
        std::cout << "Result: " << command->a + command->b << std::endl;
    }
    if (auto command = std::get_if<NegateCommand>(&args.command)) {
        std::cout << "Result: " << -command->value << std::endl;
    }
    return 0;
}
