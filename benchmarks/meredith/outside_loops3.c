#include <stdio.h>

int main(){
	int i, j;
    int A[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    j = 7;
    if(j>6){
        printf("j is >= 6!");
        A[0] = 9;
    }else{
        j=j+1;
        printf("j is smaller than 6!");
        A[0] = 8;
    }
    j=j+1;
	for(i = 0; i < 10; i++) {
        A[0] = 10;
        j=j+1;
        printf("Hello World!");
	}
	return 0;
}