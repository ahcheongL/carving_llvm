#include<string>

int foo (std::string astr) {
    int a = astr.size();
    return 0;
}

int main (int argc, char * argv[]) {
    std::string inputstr = "abcd";

    foo(inputstr);
    return 0;    
}