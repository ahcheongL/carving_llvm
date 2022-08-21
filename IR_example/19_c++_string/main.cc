#include<iostream>
#include<string>

int foo (std::string astr) {
    int a = astr.size();

    std::cout << "astr : " << astr << "\n";
    return 0;
}

int main (int argc, char * argv[]) {
    std::string inputstr = "abcd";

    foo(inputstr);
    return 0;    
}
