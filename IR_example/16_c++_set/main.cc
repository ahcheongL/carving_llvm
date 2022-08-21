#include<set>

int foo(std::set<int> aset) {
    return aset.size();
}

int main(int argc, char * argv[]) {
    std::set<int> intset { 0 , 23 , 5};
    foo(intset);
    return 0;
}
