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

int foo (Shape sha1[], Rectangle rect1[]) {
   
  return 0;
}

int main(int argc, char * argv[]) {
   Shape * sha_ptr = new Shape[argc + 4];
   Rectangle * rect_ptr = new Rectangle[argc + 2];

   int width1 = foo (sha_ptr, rect_ptr);

   delete [] sha_ptr;
   delete [] rect_ptr;
   return width1;
}
