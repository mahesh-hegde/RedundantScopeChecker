#include <iostream>

class SideEffect {
      public:
	SideEffect() {
		std::cout << "🤠" << "\n";
	}
};

class NoSideEffect {
	public:
	int _;
	constexpr NoSideEffect(): _(6*6+1) {}
};

SideEffect s1{};

NoSideEffect ns1{};

int i1 = 10 + 1;

int main() {
	auto s = s1;
	i1 = 0;
}

