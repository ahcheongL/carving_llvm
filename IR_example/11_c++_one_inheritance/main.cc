#include<cstdlib>
#include<stdio.h>

class Shape {
   public:
      Shape() {width = 42; }
      void setWidth(int w) {
         width = w;
      }
      int getWidth() {
         return width;
      }
   protected:
      int width;
};

class Rectangle: public Shape {
public:
   Rectangle() {area = 423;}
   void dump() {
      fprintf(stderr, "Rectangle: Shape.width : %d, area : %d\n", width, area);
   }
protected:
   int area;
};

int foo (Rectangle * rect) {
   rect->dump();
   return 0;
}

int main(int argc, char * argv[]) {
  Rectangle Rect;

  //Rect.setWidth(40);

  int width1 = foo (&Rect);
  Rectangle * rect2 = new Rectangle();
  int width2 = foo (rect2);
  delete rect2;
  return width1 + width2;
}
