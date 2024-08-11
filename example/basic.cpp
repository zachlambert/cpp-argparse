#include <argparse.hpp>

int main(int argc, const char** argv) {
    std::string foo;
    std::string bar;
    std::optional<std::string> fruit;
    std::string color1;
    std::string color2;
    int a;
    int b;
    std::vector<std::string> other;

    argparse::Parser parser;
    parser.add(foo, "--foo");
    parser.add(bar, "-b|--bar", "default");
    parser.add(fruit, "--fruit", std::nullopt, {"apple", "banana", "pear"});
    parser.add(color1, "--color1", std::nullopt, {"red", "green", "blue"});
    parser.add(color2, "--color2", "red", {"red", "green", "blue"});
    parser.add(a, "a");
    parser.add(b, "b");
    parser.add(other, "other");

    if (!parser.parse(argc, argv)) {
        return 1;
    }

    std::cout << "foo: " << foo << std::endl;
    std::cout << "bar: " << bar << std::endl;
    std::cout << "fruit: " << (fruit.has_value() ? fruit.value() : "<none>") << std::endl;
    std::cout << "color1: " << color1 << std::endl;
    std::cout << "color2: " << color2 << std::endl;
    std::cout << "a + b: " << a + b << std::endl;
    std::cout << "other: [";
    for (std::size_t i = 0; i < other.size(); i++) {
        std::cout << other[i];
        if (i+1 != other.size()) {
            std::cout << ", ";
        }
    }
    std::cout << "]" << std::endl;

    return 0;
}
