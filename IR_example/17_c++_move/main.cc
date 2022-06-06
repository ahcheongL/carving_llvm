#include <iomanip>
#include <iostream>
#include <utility>
#include <vector>
#include <string>
 
int foo (std::vector<std::string> v, std::string && rstr) {
    v.push_back(rstr);
    return 0;
}

int main(int argc, char * argv[])
{
    std::string str = "Salut";
    std::vector<std::string> vec1;
 
    vec1.push_back(str);
    vec1.push_back(std::move(str));
    int a = foo(vec1, std::string("abc"));

    return a;
}