#include<iostream>
#include<cstdlib>

struct small {
   int a;
};

class Shape {
   public:   
      Shape() {width = 42; age = 3;}
      virtual void setWidth(int w) {
         //std::cout << "Call Shape setwidth\n";
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
      //std::cout << "Call Rect setwidth\n";
      width = w * 10;
   }
   int getWidth() {
      return width * 10;
   }
   int area;
};

int foo (Shape * shape1, Rectangle * shape2, struct small shape3[1], int x) {
  // std::cerr << "sha_arr[2].area : " << ((Rectangle *) shape1 + 2)->area << "\n";
  printf("sha_rr[2].width: %d\n", shape1[2].getWidth());
  printf("rect_arr[5].area: %d\n", shape2[5].area);
  printf("x: %d\n", x);
  return 0;
}

int main(int argc, char * argv[]) {
   int x = argc+1;
   Shape sha_arr[10];
   Rectangle rect_arr[10];

   rect_arr[5].area = argc;

   //sha_arr[2]. area will be destroyed
   sha_arr[2] = rect_arr[5];
   
   //Compile error!
   //rect_arr[2] = sha_arr[3];

   struct small small_arr[1];

   int width1 = foo (sha_arr,rect_arr, small_arr, x);
   return width1;
}
