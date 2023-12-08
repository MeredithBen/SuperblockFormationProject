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
        int k = 7;
        if(k>8){ //comparing i here causes issues
            //int a = 8;
            printf("Larger than 5");
        }else{
            //k=k+2;
            printf("Smaller than 6");
        }
	}
	return 0;
}