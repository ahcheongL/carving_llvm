#include<set>

int foo(std::set<int>) {
    return 0;
}

int main(int argc, char * argv[]) {
    std::set<int> intset { 0 , 23 , 5};
    foo(intset);
    return 0;
}