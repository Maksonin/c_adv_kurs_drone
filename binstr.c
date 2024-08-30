
// Online C Compiler - Build, Compile and Run your C programs online in your favorite browser

#include <stdio.h>
#include <stdlib.h>

typedef struct __sht21configStruct {
	unsigned resolution: 2;
	unsigned statusBt: 1;
	unsigned reserv: 3;
	unsigned heater: 1;
	unsigned otpreload: 1;
} sht21configStruct;

union __sht21config{
	int config;
	sht21configStruct configStruct;
} sht21config;

void printfBinBufer(int num){
    char *buf = calloc(1, sizeof(char));
    unsigned int len = 1;
    //printf("- %d -\n", sizeof(num));
    if(num == 0)
        buf[0] = '0';
    else
        while(num > 0){
            buf[len-1] = '0' + (num & 1);
            num >>= 1;
            if(num > 0){
                len++;
                buf = realloc(buf, len * sizeof(char));
            }
        }
    
    for(int i = len; i >= 0; i--)
        printf("%c", buf[i]);
        
    printf("\n");
    free(buf);
}

int main()
{
    sht21config.config = 89;
    
    printfBinBufer(10);
    
    printfBinBufer(sht21config.config);
    printfBinBufer(sht21config.configStruct.resolution);
    printfBinBufer(sht21config.configStruct.statusBt);
    printfBinBufer(sht21config.configStruct.reserv);
    printfBinBufer(sht21config.configStruct.heater);
    printfBinBufer(sht21config.configStruct.otpreload);
    return 0;
}
