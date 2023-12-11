#include <stdio.h>

int main(){
	int a, b, c;
	a = 5;
    b = 10;
    int *ptr = &a;
    int *ptr2 = &b;
	for (int i = 0; i < 10; i++) { // loop heuristic & branch direction heuristic
        if (a < 0) {
            //true but the else if is also true
            printf("opcode not taken, ptr heuristic taken");
        }
        else if (ptr != NULL) {
            printf("ptr heuristic taken");
            if (i == 1) {
                ptr = NULL;
            }
        }
        else if (ptr == NULL) {
            printf("ptr heuristic not taken");
            ptr = &a;
        }
        if (ptr != ptr2) {
            printf("ptr heuristic taken");
            ptr = &b;

        }
        else if (ptr == ptr2) {
            printf("ptr heuristic not taken");
            ptr = &a;
        }

	}
    printf("outside loop now");
	return 0;
}