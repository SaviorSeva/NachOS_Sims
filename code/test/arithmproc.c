#include "syscall.h"
#define THAT ""

void g(int* s){
    PutString("Hi, I am process1 \n",20);
    PutString("Enter an integer to compute it's square: ", 42);
    GetInt(s);

    int i = (int)*s;
    PutString("The square is: ", 16);
    PutInt(i*i);
    PutChar('\n');
    PutString("Now I am done !\n",17);

}

int main(){
    g((int*) THAT);
    return 1;
}