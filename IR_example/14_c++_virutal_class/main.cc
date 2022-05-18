#include<iostream>
#include<cstdlib>

class Person {
public :
   Person() {}
   int getWidth() { return 0;}
   void setWidth(int w) { return; }
protected:
   int width;
};

class Ma : public virtual Person {
public:
   Ma() {height = 42; }
   void setHeight(char h) { height = h; }
   char getHeight() { return height; }
protected:
   char height;
};

class Fa : public virtual Person {
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

int foo (Person * p) {
  return p->getWidth();
}

int main(int argc, char * argv[]) {
   Person * person_ptr;
   Child child;
   person_ptr = &child;
   person_ptr->setWidth(40);
   int width1 = foo (person_ptr);
   return width1;
}
