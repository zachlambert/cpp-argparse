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
    parser.add(foo, "--foo")
        .help("Foo flag");
    parser.add(bar, "-b|--bar")
        .default_value("asdf");
    parser.add(fruit, "--fruit")
        .choices({"apple", "banana", "pear"})
        .help("Fruit flag");
    parser.add(color1, "--color1")
        .choices({"red", "green", "blue"});
    parser.add(color2, "--color2")
        .default_value("red")
        .choices({"red", "green", "blue"});
    parser.add(a, "a")
        .default_value(2)
        .help("First number to add");
    parser.add(b, "b")
        .help("Second number to add");
    parser.add(other, "other")
        .help("Other words to print out");

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
