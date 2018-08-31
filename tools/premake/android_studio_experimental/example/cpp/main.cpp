#include <stdio.h>
#include <jni.h>

extern "C"
int Java_com_package_name_hello_cpp(void* args)
{
    printf("oh hai! I'm c++\n");

    return 0;
}
