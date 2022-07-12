#include<vector>
#include<string>
#include<cstdlib>

std::vector<std::string> v;

int main() {
	if (const char *home = getenv("STORAGE_DIR")) {
		v.push_back(home);
	}

	// ..

	if (system("which my_app")) {
		v.push_back("~/.my_app");
	}
}
