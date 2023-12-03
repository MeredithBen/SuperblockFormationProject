#include <stdio.h>

int main(){
	int i;
    char grade='A';
	for(i = 0; i < 10; i++) {
        if(i==1){
            grade = 'B';
        }
        if(i==7){
            grade='C';
        }
        switch(grade){
            case 'A' :
                printf("Excellent!\n" );
                break;
            case 'B' :
                printf("Good job!\n" );
                break;
            case 'C' :
                printf("Well done.\n" );
                break;
            case 'D' :
                printf("Try harder.\n" );
                break;
            default :
                printf("Invalid grade\n" );
        }
	}
	return 0;
}