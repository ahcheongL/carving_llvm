#include<iostream>
#include<cstdlib>

class Ma  {
public:
   Ma() {height = 42; }
   void setHeight(char h) { height = h; }
   char getHeight() { return height; }
   char height;
};

class Fa {
   public:
      Fa() {width = 42; }
      void setWidth(int w) { width = w; }
      int getWidth() { return width; }
      int width;
};

class Child: public Ma, public Fa {
public:
   Child() {area = 423;}
   int area;
};

int foo (Child * ch) {
   int a = ch->width + ch->height;
   fprintf(stderr, "%d = %d + %d\n", a, ch->width, ch->height);
   return a;
}

int main(int argc, char * argv[]) {
   Child child;

   child.setWidth(40);

   int width1 = foo(&child);
   return width1;
}
