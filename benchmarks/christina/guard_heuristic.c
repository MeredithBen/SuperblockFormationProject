#include <stdio.h>

int main(){
	int a, b, c;
	a = 5;
    b = 10;
	for (int i = 0; i < 10; i++) { // loop heuristic & branch direction heuristic
        if (a > 0){ // guard heuristic
            b=b/a;
            printf("guard heuristic taken!");
        }
        else {
            b = b / 2;
            printf("guard heuristic not taken!");
        }
        a--;
	}
    printf("outside loop now");
	return 0;
}