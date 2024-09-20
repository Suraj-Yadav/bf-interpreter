#include "parser.hpp"

int main(int argc, char* argv[]) {
	std::string file;
	bool profile = false;
	std::vector<std::string> args(argv + 1, argv + argc);
	for (auto& arg : args) {
		if (arg == "-p") {
			profile = true;
		} else if (file.empty()) {
			file = arg;
		}
	}

	if (file.empty()) {
		std::cerr << "bf: fatal error: no input files\n";
		return 1;
	}

	Program p(file);

	if (!p.isOK()) {
		std::cerr << p.error() << "\n";
		return 1;
	}

	return 0;
}
