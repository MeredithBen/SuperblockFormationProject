#include <stdio.h>

int main(){
	int i, j;
	j = 5;
	for(i = 0; i < 10; i++) {
		if (j < 0) 
        {
            j++;
            printf("Hello World!");
        }
        else if (0 > j){
            j--;
            printf("Goodbye World!");
        }
        else if (j == 3.1456) {
            printf("stuff");
        }
	}
	return 0;
}