#include "syscall.h"

int main(){
    PutString("# Starting 2 processes\n",24);
    ForkExec("arithm");
    ForkExec("thread");
    return 0;
}
