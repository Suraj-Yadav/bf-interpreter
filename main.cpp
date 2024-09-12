#include <fstream>
#include <iostream>
#include <stack>
#include <vector>

class Tape {
	using T = unsigned char;
	static_assert(sizeof(T) == 1);

	std::vector<T> right, left;
	int index = 0;

   public:
	Tape() { right.push_back(0); }

	void print() {
		std::cout << "index = " << index << '\n';
		while (!left.empty()) {
			std::cout << (int)left.back() << "\t";
			left.pop_back();
		}
		for (auto& e : right) { std::cout << (int)e << "\t"; }
		std::cout << '\n';
	}

	T& get() { return operator[](index); }

	void moveRight() {
		index++;
		if (index == right.size()) { right.push_back(0); }
	}

	void moveLeft() {
		index--;
		auto i = -index - 1;
		if (i == left.size()) { left.push_back(0); }
	}

	T& operator[](int index) {
		if (index >= 0) { return right[index]; }
		index = -index - 1;
		return left[index];
	}
};

class Jumps {
	std::vector<int> jumps;

   public:
	void add() { jumps.push_back(-1); }
	void addLeft(int l) {}
	void addRight(int l, int r) {
		jumps[r] = l;
		jumps[l] = r;
	}

	int getJump(int p) { return jumps[p]; }
};

constexpr std::string_view ALLOWED_CHARS = "><+-[].,$";

class Program {
	std::string program;
	int p = 0;
	std::ifstream input;
	std::stack<int> stack;
	Jumps jumps;

   public:
	Program(const std::string& file) : input(file) {}

	char get() {
		while (p >= program.size()) {
			auto ch = '\0';
			if (!(input >> ch)) { return ch; }
			if (ALLOWED_CHARS.find(ch) == std::string_view::npos) { continue; }
			program.push_back(ch);
			jumps.add();
			if (ch == '[') {
				stack.push(p);
				jumps.addLeft(p);
			}
			if (ch == ']') {
				jumps.addRight(stack.top(), p);
				stack.pop();
			}
		}
		return program[p];
	}

	void next() { p++; }

	void jump() {
		auto tempP = p;
		while (jumps.getJump(tempP) == -1) {
			get();
			next();
		}
		p = jumps.getJump(tempP);
	}
};

int main(int argc, char* argv[]) {
	char ch = 0;
	Tape tape;
	std::string file = "./input.txt";
	if (argc > 1) { file = argv[1]; }
	Program p(file);

	while (true) {
		auto ch = p.get();
		if (ch == 0) { break; }
		// std::cout << ch << "\n";
		switch (ch) {
			case '>': {
				tape.moveRight();
				p.next();
				break;
			}
			case '<': {
				tape.moveLeft();
				p.next();
				break;
			}
			case '+': {
				tape.get()++;
				p.next();
				break;
			}
			case '-': {
				tape.get()--;
				p.next();
				break;
			}
			case '.': {
				std::cout << tape.get();
				p.next();
				break;
			}
			case ',': {
				std::cin >> tape.get();
				p.next();
				break;
			}
			case '[': {
				if (tape.get() == 0) {
					p.jump();
				} else {
					p.next();
				}
				break;
			}
			case ']': {
				if (tape.get() != 0) {
					p.jump();
				} else {
					p.next();
				}
				break;
			}
			case '$': {
				tape.print();
				p.next();
				break;
			}
			default:
				p.next();
				break;
		}
	}

	return 0;
}
