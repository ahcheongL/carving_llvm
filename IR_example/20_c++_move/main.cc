#include <utility>

class Shape {
public:
    Shape(int area, char width) : area(area), width(width) {}
    int area;
    char width;
};

int foo (class Shape && sha) {
    return 0;
}

int main(int argc, char * argv[])
{
    class Shape shape1(10, 20);
    foo(std::move(shape1));

    return 0;
}