#include "types.h"
#include "user.h"

int main(int argc, char *argv[])
{
//    int i = 1;

    sbrk(100000);


    printf(0, "%d\n\n\n", (int*)0x9fffff2f);

    //printf(1, "%d\n", sbrk(20000));

    /*char writeBuffer[3000];
    writeBuffer[0] = 0;
    printf(1, "%s\n", writeBuffer);
*/

    /*char writeBuffer2[3000];
    writeBuffer2[0] = 0;
    printf(1, "%s\n", writeBuffer2);
*/

    /*char writeBuffer3[3000];
    writeBuffer3[0] = 0;
    printf(1, "%s\n", writeBuffer3);


    char writeBuffer4[3000];
    writeBuffer4[0] = 0;
    printf(1, "%s\n", writeBuffer4);


    char writeBuffer5[3000];
    writeBuffer5[0] = 0;
    printf(1, "%s\n", writeBuffer5);
*/

  int returnValue;
	returnValue = testCall();

	printf( 0, "Returned value = %d\n", returnValue);



	exit();

}
