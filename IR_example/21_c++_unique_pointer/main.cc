#include <memory>

class Rectangle {
    int height;
    int width;
 
public:
    Rectangle(int h, int w){
        height = h;
        width = w;
    }
 
    int area(){
        return height * width;
    }
};
 
int foo (std::unique_ptr<Rectangle>& rect) {
    return 0;
}

int main(int argc, char * argv []){
 
    std::unique_ptr<Rectangle> P1(new Rectangle(10, 5));
 
    std::unique_ptr<Rectangle> P2;
    P2 = std::move(P1);
    //P1->area(); seg fault
    foo(P2);
    return 0;
}

