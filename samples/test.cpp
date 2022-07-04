#include <cstdio>
#include <cstdlib>

static long long x; // used in both

static int y;

const char * yz = "1001";

int p;

void _aux() {
	x = 110;
}

int main() {
	int p = x; // shadowing decl, original p should be unused now
	if (1 < 0) {
		p = atoi(yz);
	} else {
		p = 1000;
	}
	return y;
}

// expected: p unused, no warning for x, y only used in main, yz only used in if (1 < 0)
