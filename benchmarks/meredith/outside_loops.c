#include <stdio.h>

int main(){
	int i, j;
    j = 7;
    if(j>6){
        printf("j is >= 6!");
    }else{
        j=j+1;
        printf("j is smaller than 6!");
    }
	for(i = 0; i < 10; i++) {
       printf("Hello World!");
	}
	return 0;
}