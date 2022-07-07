#include <stdio.h>

char x[100] = {0};

const int t = 1;

int fr = t;

int main() {
	int x = 10;
	if (x == 1) {
		return x;
	}
	return 0;
}

void f() {
	x[0] = 1;
	x[1] = 11;
	x[2] = 21;
	if (1 + 1 * 0 / 1) {
		x[3] = 32;
	}
}
