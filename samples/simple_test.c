// this should produce 1 warning for p2
#include <stdio.h>

int p2 = 0;

int main(int argc, char **argv) {
	int i1 = p2;
	if (1) {
		char c1 = (char)(i1 * i1);
	}
	return i1-1;
}
