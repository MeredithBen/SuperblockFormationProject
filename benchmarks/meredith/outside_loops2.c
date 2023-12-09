#include <stdio.h>

int main(){
	int i, j;
    j = 7;
    if(j>6){
        printf("j is >= 6!");
        j=j+2;
    }else{
        printf("j is smaller than 6!");
    }
    j=j+1;
	for(i = 0; i < 10; i++) {
        j=j+1;
        printf("Hello World!");
	}
	return 0;
}