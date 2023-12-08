#include <stdio.h>

int main(){
	int i, j, k;
	j = 5;
    k = 10;
	for(i = 0; i < 10; i++) {
		if (j < 0) 
        {
            j++;
            printf("Hello World!");
        }
        else if (j > 0){
            k=k/j;
            printf("Goodbye World!");
        }
        else {
            j = j / 2;
        }
	}
    printf("outside loop now");
	return 0;
}