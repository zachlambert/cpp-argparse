#include <argparse.hpp>

using T = argparse::Args;

struct CliArgs: public argparse::Args {
    int a;
    int b;
    std::string op;
private:
    void build(argparse::Parser& parser) override {
        parser.add(a, "a").help("First argument").default_value(0);
        parser.add(b, "b").help("Second argument").default_value(0);
        parser.add(op, "--op").help("Operation").choices({"add", "multiply"});
    }
};

int main(int argc, const char** argv) {
    CliArgs args;
    if (!argparse::parse(argc, argv, args, "Args test")) {
        return 1;
    }
    if (args.op == "add") {
        std::cout << "Result: " << args.a + args.b << std::endl;
    }
    else if (args.op == "multiply") {
        std::cout << "Result: " << args.a * args.b << std::endl;
    }
    else {
        assert(false);
        return 1;
    }
    return 0;
}
