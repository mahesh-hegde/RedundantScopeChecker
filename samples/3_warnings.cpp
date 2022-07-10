#include <cstdio>
#include <cstdlib>

// This file should produce 2 or 3 warnings
// yz used in inner most scope
// y used in main only
// p unused

// no warning for x

long long x; // used in both

int y;

const char *yz = "1001";

int p;

void _aux() { x = 110; }

int main() {
	int p = x; // shadowing decl, original p should be unused now
	if (1 < 0) {
		p = atoi(yz);
	} else {
		p = 1000;
	}
	return y;
}

// expected: p unused, no warning for x, y only used in main, yz only used in if

