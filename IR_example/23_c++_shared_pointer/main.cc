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
 
int foo (std::shared_ptr<Rectangle>& rect) {
    return 0;
}

int main(int argc, char * argv []){
 
    std::shared_ptr<Rectangle> P1(new Rectangle(10, 5));
 
    std::shared_ptr<Rectangle> P2;
    P2 = P1;
    //P1->area(); seg fault
    foo(P2);
    return 0;
}

