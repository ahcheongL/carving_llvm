#include<iostream>
#include<cstdlib>

class Shape {
   public:   
      Shape() {width = 42; age = 3;}
      virtual void setWidth(int w) {
         std::cout << "Call Shape setwidth\n";
         width = w;
      }
      int getWidth() {
         return width;
      }
   protected:
      char age;
      int width;
};

class Rectangle: public Shape {
public:
   Rectangle() {area = 423;}
   void setWidth(int w) {
      std::cout << "Call Rect setwidth\n";
      width = w * 10;
   }
   int getWidth() {
      return width * 10;
   }
protected:
   int area;
};

int foo (Shape * shape1, Shape * shape2) {
  return shape1->getWidth() + shape2->getWidth();
}

int main(int argc, char * argv[]) {
   Shape * sha_ptr = NULL; 
   Shape * rect_ptr = NULL;
   Rectangle Rect;
   Shape shape;
   rect_ptr = &Rect;
   sha_ptr = &shape;
   Rect.setWidth(40);
   sha_ptr->setWidth(20);
   rect_ptr->setWidth(45);
   int width1 = foo (sha_ptr,rect_ptr);
   return width1;
}
