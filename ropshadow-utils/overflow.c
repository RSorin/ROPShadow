#include <stdlib.h>
#include <stdio.h>
void func()
{
    char buffer[256];
    gets(buffer);
}
int main(int argc, char *argv[])
{
    func();
    return 0;
}
