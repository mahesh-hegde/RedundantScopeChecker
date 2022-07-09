#include <stdio.h>

__attribute__((rcs_ignore))
char x[100] = {0};

const int twenty_eight = 28;

int t = twenty_eight+3;

int main() {
	t = -2;
	int x = 10;
	if (x == 1) {
		return x;
	}
	return 0;
}

void f() {
	t = -1;
	x[0] = 1;
	x[1] = 11;
	x[2] = 21;
	if (1 + 1 * 0 / 1) {
		x[3] = 32;
	}
}
