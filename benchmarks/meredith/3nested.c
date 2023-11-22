#include <stdio.h>

int main(){
	int i, j, l;
	for(i = 0; i < 10; i++) {
        for(j=0; j<5; j++){
            for(l=0; l<3; l++){
                printf("Hello World!");
            }
        }
	}
	return 0;
}