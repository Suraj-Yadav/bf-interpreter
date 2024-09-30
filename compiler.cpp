#include <immintrin.h>

#include <filesystem>

#include "parser.hpp"
#include "util.hpp"

int mod(int v) {
	constexpr int MAX = std::numeric_limits<DATA_TYPE>::max();
	constexpr int MOD = MAX + 1;
	while (v < MOD) { v += MOD; }
	return v & MAX;
}

void scan(std::ofstream& output, const Instruction& inst, auto loc = 0u) {
	auto jump = inst.value;
	if (jump == 0) { return; }

	// For large jump, normal scan works fine
	if (false) {
		print(output, "	add rbx, %", -jump);
		print(output, ".SCAN_START%:", loc);
		print(output, "	add rbx, %", jump);
		print(output, "	cmp BYTE PTR tape[rbx], 0");
		print(output, "	jne .SCAN_START%", loc);
		print(output, ".SCAN_END%:", loc);
		return;
	}

	auto isNeg = jump < 0;
	const auto sign = isNeg ? -1 : 1;
	if (isNeg) { jump = -jump; }
	auto isPowerOf2 = (jump & (jump - 1)) == 0;
	const auto VEC_SZ = 64;
	__mmask64 mask = 0;
	for (auto i = 0u; i < VEC_SZ; i += jump) { mask = mask | 1ULL << i; }

	const auto shift = jump - static_cast<int>(VEC_SZ) % jump;
	const auto mask_mask = (1 << (shift + 1)) - 1;

	print(output, "#Scan of %", sign * jump);
	// Generate instructions
	if (isNeg) { print(output, "	add rbx, %", -VEC_SZ + 1); }

	print(output, "	mov rax, %", mask);
	print(output, "	kmovq k1, rax");

	print(output, "	vpxorq zmm0, zmm0, zmm0");
	print(output, "	add rbx, %", -sign * VEC_SZ);
	print(output, ".SCAN_START%:", loc);
	print(output, "	add rbx, %", sign * VEC_SZ);
	print(output, "	vmovdqu64 zmm1, ZMMWORD PTR tape[rbx]");

	if (isNeg) {
		// reverse zmm1
		print(output, "	vpshufb zmm1, zmm1, ZMMWORD PTR SHUFFLE");
		print(output, "	vmovdqu64 zmm2, ZMMWORD PTR PERMUTE");
		print(output, "	vpermq zmm1, zmm2, zmm1");
	}
	_mm512_set_epi64(0, 0, 0, 0, 0, 0, 0, 0);
	print(output, "	vpcmpb k0 {k1}, zmm0, zmm1, 0");

	if (!isPowerOf2) {
		print(output, "	mov rdx, rax");
		print(output, "	shl rdx, %", shift);
		print(output, "	shr rax, %", jump - shift);
		print(output, "	and rax, %", mask_mask);
		print(output, "	or rax, rdx");
		print(output, "	kmovq k1, rax");
	}

	print(output, "	kortestq k0, k0");
	print(output, "	je .SCAN_START%", loc);

	print(output, "	kmovq   rax, k0");
	print(output, "	rep bsf rdx, rax");
	if (isNeg) {
		print(output, "	add rbx, %", VEC_SZ - 1);
		print(output, "	sub rbx, rdx");
	} else {
		print(output, "	add rbx, rdx");
	}
}

bool compile(std::span<Instruction> code, const std::filesystem::path& path) {
	std::ofstream output(path);
	output << R"(
.intel_syntax noprefix
.section .rodata
SHUFFLE:
        .byte   15
        .byte   14
        .byte   13
        .byte   12
        .byte   11
        .byte   10
        .byte   9
        .byte   8
        .byte   7
        .byte   6
        .byte   5
        .byte   4
        .byte   3
        .byte   2
        .byte   1
        .byte   0
        .byte   15
        .byte   14
        .byte   13
        .byte   12
        .byte   11
        .byte   10
        .byte   9
        .byte   8
        .byte   7
        .byte   6
        .byte   5
        .byte   4
        .byte   3
        .byte   2
        .byte   1
        .byte   0
        .byte   15
        .byte   14
        .byte   13
        .byte   12
        .byte   11
        .byte   10
        .byte   9
        .byte   8
        .byte   7
        .byte   6
        .byte   5
        .byte   4
        .byte   3
        .byte   2
        .byte   1
        .byte   0
        .byte   15
        .byte   14
        .byte   13
        .byte   12
        .byte   11
        .byte   10
        .byte   9
        .byte   8
        .byte   7
        .byte   6
        .byte   5
        .byte   4
        .byte   3
        .byte   2
        .byte   1
        .byte   0
PERMUTE:
        .quad   6
        .quad   7
        .quad   4
        .quad   5
        .quad   2
        .quad   3
        .quad   0
        .quad   1
.text
	.globl main
	.type main, @function
main:
	sub rsp, 8
)";

	constexpr auto TAPE_LENGTH = 1000000u;
	// I store the current index of tape in register B
	print(output, "	mov rbx, %", TAPE_LENGTH / 2);

	auto loc = 0u;
	for (auto& i : code) {
		switch (i.code) {
			case NO_OP:
				break;
			case TAPE_M:
				print(output, "	add rbx, %", i.value);
				break;
			case SET_C:
				print(output, "	mov BYTE PTR tape[rbx], %", i.value);
				break;
			case INCR_C:
				print(output, "	movzx ecx, BYTE PTR tape[rbx]");
				print(output, "	add ecx, %", mod(i.value));
				print(output, "	mov BYTE PTR tape[rbx], cl");
				break;
			case INCR_R:

				print(output, "	movzx ecx, BYTE PTR tape[rbx]");
				print(output, "	mov eax, %", i.value);
				print(output, "	imul eax, ecx");
				print(output, "	add BYTE PTR tape[rbx+%], al", i.lRef);
				break;
			case WRITE:
				print(output, "	mov rsi, QWORD PTR stdout");
				print(output, "	movzx edi, BYTE PTR tape[rbx]");
				print(output, "	call putc");
				break;
			case READ:
				print(output, "	mov rdi, QWORD PTR stdin");
				print(output, "	call getc");
				print(output, "	mov BYTE PTR tape[rbx], al");
				break;
			case JUMP_C:
				print(output, "	cmp BYTE PTR tape[rbx], 0");
				print(output, "	je .LOC%", loc + i.value);
				print(output, ".LOC%:", loc);
				break;
			case JUMP_O:
				print(output, "	cmp BYTE PTR tape[rbx], 0");
				print(output, "	jne .LOC%", loc + i.value);
				print(output, ".LOC%:", loc);
				break;
			case SCAN:
				scan(output, i, loc);
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
	auto args = argparse(argc, argv);

	if (args.input.empty()) {
		std::cerr << "bfc: fatal error: no input files\n";
		return 1;
	}

	Program p(args.input);

	if (!p.isOK()) {
		std::cerr << p.error() << "\n";
		return 1;
	}

	if (args.optimizeSimpleLoops) { p.optimizeSimpleLoops(); }
	if (args.optimizeScans) { p.optimizeScans(); }

	auto outputPath =
		std::filesystem::temp_directory_path() / "tmp-bf-assembly.s";
	// outputPath.replace_extension("s");

	compile(p.instructions(), outputPath);
	auto command = "g++ -g " + outputPath.string();
	if (!args.output.empty()) { command += " -o " + args.output.string(); }

	auto retCode = std::system(command.c_str());

	if (retCode == 0) {
		// std::filesystem::remove(outputPath);
		return 0;
	}
	std::cerr << "Bug in compiler\n";
	return 1;
}
