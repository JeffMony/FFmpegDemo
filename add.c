
#include <stdio.h>


int add(int a, int b) {
  int sum = a + b;
  return sum;
}

int main(int argc, char **argv) {
  int sum = add(3, 4);
  printf("sum = %d\n", sum);
  return 0;
}


