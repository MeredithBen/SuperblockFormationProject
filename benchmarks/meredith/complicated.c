#include <stdio.h>

int main(){
	int i, j, k, l;
	for(i = 0; i < 10; i++) {
        printf("Nest level 1!");
        for(j=0; j<5; j++){
            printf("Nest level 2!");
            for(l=0; l<3; l++){
                printf("Nest level 3!");
            }
        }
        for(l=0; l<3; l++){
            printf("Nest level 2!");
            for(k=0; k<6; k++){
                printf("Nest level 3!");
            }
        }
	}
	return 0;
}