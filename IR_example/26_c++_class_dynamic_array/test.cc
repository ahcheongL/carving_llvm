#include<iostream>
#include<cstdlib>

struct small {
   int a;
   int b;
};

class Shape {
   public:   
      Shape() {width = 42; age = 3;}
      virtual void setWidth(int w) {
         fprintf(stderr, "shape.setWidth: %d\n", w);
         width = w;
      }
      int getWidth() {
         fprintf(stderr, "shape.getWidth: %d\n", width);
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
         fprintf(stderr, "rectangle.setWidth: %d\n", w);
         width = w * 10;
      }
      int getWidth() {
         fprintf(stderr, "rectangle.getWidth: %d\n", width);
         return width * 10;
      }
      int area;
};

int foo (Shape * shape1, Rectangle * shape2, struct small * shape3, int x) {
  fprintf(stderr, "foo > sha_rr[2].getwidth(): %d\n", shape1[2].getWidth());
  fprintf(stderr, "foo > rect_arr[5].area: %d\n", shape2[5].area);
  fprintf(stderr, "foo > shape3[0].a: %d\n", shape3[0].a);
  fprintf(stderr, "foo > x: %d\n", x);
  return 0;
}

int main(int argc, char * argv[]) {
   int x = argc;
   Shape sha_arr[4];
   Rectangle rect_arr[2];

   rect_arr[1].area = x;

   sha_arr[2].setWidth(22);
   
   //Compile error!
   //rect_arr[2] = sha_arr[3];

   struct small small_arr[1];
   small_arr[0].a = x + 2;

   int width1 = foo (sha_arr,rect_arr, small_arr, x);
   
   return 0;
}
