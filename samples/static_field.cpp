// Example with static variables in class

// This should produce 1 warning for Dummy::p2

class Dummy {
      private:
	int p1;

      public:
	static int p2;
	int f1() { return p2 * p2; }
};

int Dummy::p2 = 0;

int main(int argc, char **argv) {
	int i1 = Dummy::p2;
	if (true) {
		char c1 = (char)(i1 * i1);
	}
	return i1;
}
