#include <unistd.h> //incluir para system call

int main(void){
    write(1, "hello, World!\n",17);
    return 0;
}