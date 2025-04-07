#include <stdio.h>


int main() {
    int a = 0x12345678;
    int* p = &a;
    char* t = p;
    printf("%p\n", t);
    printf("%p\n", t + 1);
    printf("%x\n", *(t + 1));
}