#include<stdio.h>
#include<iostream>
#include<cstdlib>

class Shape {
   public:   
      Shape() {width = 42; age = 3;}
      virtual void setWidth(int w) {
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
      width = w * 10;
   }
   int getWidth() {
      return width * 10;
   }
   int area;
};

int foo (Shape * shape1, Shape * shape2) {
  fprintf(stderr, "%d %d ", shape1->getWidth(), shape2->getWidth());
  int a =  shape1->getWidth() + shape2->getWidth();
  a = a + ((Rectangle * ) shape2)->area;
  fprintf(stderr, "%d\n", a);
  return a;
}

int main(int argc, char * argv[]) {
   Shape * sha_ptr = NULL; 
   Shape * rect_ptr = NULL;
   Rectangle Rect;
   Shape shape;
   rect_ptr = &Rect;
   sha_ptr = &shape;
   Rect.setWidth(40); //width = 400
   sha_ptr->setWidth(20); //width = 200
   rect_ptr->setWidth(45); //width = 450
   int width1 = foo (sha_ptr,rect_ptr);
   return width1;
}
