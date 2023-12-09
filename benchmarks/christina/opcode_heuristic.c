#include <stdio.h>

int main(){
	int i, j, k;
	j = 5;
    k = 10;
    int *ptr = &j;
    int *ptr2 = &k;
	while(j < 20) { // loop heuristic & branch direction heuristic
		if (j < 0) //opcode heuristic
        {
            j++;
            printf("Neg opcode heuristic not taken!");
        }
        else if (0 > j) {
            printf("Neg opcode heuristic not taken!");
        }
        else if (j > 6) {
            j++;
            printf("Neg opcode heuristic taken!");
        }
        else if (j == 5) {
            j++;
            printf("FP opcode heuristic taken!");
        }
        else if (j == 3.145678) {
            printf("FP opcode heuristic not taken!");
        }
        else if (ptr != NULL) {
            printf("ptr heuristic taken");
        }
        else if (ptr == NULL) {
            printf("ptr heuristic not taken");
        }
        else if (ptr != ptr2) {
            printf("ptr heuristic taken");
        }
        else if (ptr == ptr2) {
            printf("ptr heuristic not taken");
        }
        else if (j > 0){ // guard heuristic
            k=k/j;
            printf("guard heuristic taken!");
        }
        else {
            k = k / 2;
            printf("guard heuristic not taken!");
        }
	}
    printf("outside loop now");
	return 0;
}