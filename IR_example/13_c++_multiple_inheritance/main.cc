#include<iostream>
#include<cstdlib>

class Ma  {
public:
   Ma() {height = 42; }
   void setHeight(char h) { height = h; }
   char getHeight() { return height; }
protected:
   char height;
};

class Fa {
   public:
      Fa() {width = 42; }
      void setWidth(int w) { width = w; }
      int getWidth() { return width; }
   protected:
      int width;
};

class Child: public Ma, public Fa {
public:
   Child() {area = 423;}
protected:
   int area;
};

int foo (Child * ch) {
  return ch->getWidth();
}

int main(int argc, char * argv[]) {
  Child child;

  child.setWidth(40);

  int width1 = foo (&child);
  return width1;
}
