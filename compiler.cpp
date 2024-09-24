#include <filesystem>

#include "parser.hpp"

int mod(int v) {
	while (v < 256) { v += 256; }
	return v & 255;
}

bool compile(std::span<Instruction> code, const std::filesystem::path& path) {
	std::ofstream output(path);

	output << R"(
.intel_syntax noprefix
.text
	.globl main
	.type main, @function
main:
	sub rsp, 8
)";

	constexpr auto TAPE_LENGTH = 88888;
	// I store the current index of tape in register B
	output << "	mov ebx, " << TAPE_LENGTH / 2 << "\n";

	auto loc = 0u;
	for (auto& i : code) {
		switch (i.code) {
			case NO_OP:
				break;
			case TAPE_M:
				output << "	add ebx, " << i.value << "\n";
				break;
			case INCR_C:
				output << "	movzx ecx, BYTE PTR tape[rbx]\n";
				output << "	add ecx, " << mod(i.value) << "\n";
				output << "	mov BYTE PTR tape[rbx], cl\n";
				break;
			case INCR_R:
				break;
			case WRITE:
				output << "	mov rsi, QWORD PTR stdout\n";
				output << "	movzx edi, BYTE PTR tape[rbx]\n";
				output << "	call putc\n";
				break;
			case READ:
				output << "	mov rdi, QWORD PTR stdin\n";
				output << "	call getc\n";
				output << "	mov BYTE PTR tape[rbx], al\n";
				break;
			case JUMP_C:
				output << "	cmp BYTE PTR tape[rbx], 0\n";
				output << "	je .LOC" << loc + i.value << "\n";
				output << ".LOC" << loc << ":\n";
				break;
			case JUMP_O:
				output << "	cmp BYTE PTR tape[rbx], 0\n";
				output << "	jne .LOC" << loc + i.value << "\n";
				output << ".LOC" << loc << ":\n";
				break;
			case DEBUG:
			case HALT:
				break;
		}
		loc++;
	}

	output << R"(
	xor eax, eax
	add rsp, 8
	ret
	.size	main, .-main
.bss
	.globl tape
	.align 32
	.type tape, @object
	.size tape, )"
		   << TAPE_LENGTH << R"(
tape:
	.zero )"
		   << TAPE_LENGTH << "\n";

	return true;
}

int main(int argc, char* argv[]) {
	std::filesystem::path file, executable;
	std::vector<std::string> args(argv, argv + argc);
	for (auto i = 1u; i < args.size(); ++i) {
		if (args[i - 1] == "-o") {
			executable = args[i];
		} else if (file.empty()) {
			file = args[i];
		}
	}

	if (file.empty()) {
		std::cerr << "bfc: fatal error: no input files\n";
		return 1;
	}

	Program p(file);

	if (!p.isOK()) {
		std::cerr << p.error() << "\n";
		return 1;
	}

	auto outputPath = std::filesystem::temp_directory_path() / file.filename();
	outputPath.replace_extension("s");

	compile(p.instructions(), outputPath);
	auto command = "g++ -g " + outputPath.string();
	if (!executable.empty()) { command += " -o " + executable.string(); }

	auto retCode = std::system(command.c_str());

	std::filesystem::remove(outputPath);

	return retCode;
}
