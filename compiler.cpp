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
	if (jump <= -16 || jump >= 16) {
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

	if (isNeg) { mask = revBits(mask); }

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

	print(output, "	vpcmpb k0 {k1}, zmm0, zmm1, 0");

	if (!isPowerOf2) {
		if (isNeg) {
			print(output, "	mov rdx, rax");
			print(output, "	shr rdx, %", shift);
			print(output, "	shl rax, %", jump - shift);
			print(output, "	or rax, rdx");
			print(output, "	kmovq k1, rax");

		} else {
			print(output, "	mov rdx, rax");
			print(output, "	shl rdx, %", shift);
			print(output, "	shr rax, %", jump - shift);
			print(output, "	or rax, rdx");
			print(output, "	kmovq k1, rax");
		}
	}

	print(output, "	kortestq k0, k0");
	print(output, "	je .SCAN_START%", loc);

	print(output, "	kmovq   rax, k0");
	if (isNeg) {
		print(output, "	rep bsr rdx, rax");
	} else {
		print(output, "	rep bsf rdx, rax");
	}
	if (isNeg) {
		print(output, "	add rbx, %", VEC_SZ - 1);
		print(output, "	sub rbx, rdx");
	} else {
		print(output, "	add rbx, rdx");
	}
}

void compileIncr(
	std::ofstream& output, const std::string& dest, const Instruction& inst) {
	if (inst.rRef.empty()) {
		print(output, "	mov eax, %", mod(inst.value));
		print(output, "	add BYTE PTR %, al", dest);
		return;
	}
	auto i = 0u;
	if (inst.value == 1 || inst.value == -1) {
		print(output, "	movzx eax, BYTE PTR tape[rbx+%]", inst.rRef[i++]);
	} else {
		print(output, "	mov eax, %", inst.value);
	}
	for (; i < inst.rRef.size(); ++i) {
		print(output, "	movzx ecx, BYTE PTR tape[rbx+%]", inst.rRef[i]);
		print(output, "	imul eax, ecx");
	}
	if (inst.value == -1) {
		print(output, "	sub BYTE PTR %, al", dest);
	} else {
		print(output, "	add BYTE PTR %, al", dest);
	}
}

void globalArray(
	std::ofstream& output, std::string_view name, const auto size = 0u) {
	print(output, "	.globl %", name);
	print(output, "	.align 32");
	print(output, "	.align 32");
	print(output, "	.size %, %", name, size);
	print(output, "%:", name);
	print(output, "	.zero %", size);
}

bool compile(std::span<Instruction> code, const std::filesystem::path& path) {
	std::ofstream output(path);
	output << R"(
.intel_syntax noprefix
.section .rodata
.text
	.globl main
	.type main, @function
main:
	sub rsp, 8
)";

	constexpr auto TAPE_LENGTH = 1000000u;
	// I store the current index of tape in register B
	print(output, "	mov rbx, %", TAPE_LENGTH / 2);

	std::map<int, int> locks;
	std::vector<int> tempSlots(VARIABLE_LIMIT, 0);
	std::iota(tempSlots.begin(), tempSlots.end(), 0);

	auto loc = 0u;
	for (auto& inst : code) {
		auto dest = "tape[rbx+" + std::to_string(inst.lRef) + "]";
		if (locks.contains(inst.lRef)) {
			dest = "temp[" + std::to_string(locks[inst.lRef]) + "]";
		}
		switch (inst.code) {
			case NO_OP:
				break;
			case TAPE_M:
				print(output, "	add rbx, %", inst.value);
				break;
			case SET_C:
				print(output, "	mov BYTE PTR %, %", dest, inst.value);
				break;
			case INCR:
				compileIncr(output, dest, inst);
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
				print(output, "	je .LOC%", loc + inst.value);
				print(output, ".LOC%:", loc);
				break;
			case JUMP_O:
				if (inst.lRef == 0) {
					print(output, "	cmp BYTE PTR tape[rbx], 0");
					print(output, "	jne .LOC%", loc + inst.value);
				}
				print(output, ".LOC%:", loc);
				break;
			case SCAN:
				scan(output, inst, loc);
				break;
			case DEBUG:
			case HALT:
				break;
			case WRITE_LOCK:
				if (locks.contains(inst.lRef)) {
					print(
						std::cerr, "Inst: %: cell % is already locked", loc,
						inst.lRef);
					return false;
				}
				locks[inst.lRef] = tempSlots.back();
				tempSlots.pop_back();
				print(output, "	movzx eax, BYTE PTR tape[rbx+%]", inst.lRef);
				print(output, "	mov BYTE PTR temp[%], al", locks[inst.lRef]);

				break;
			case WRITE_UNLOCK:
				if (!locks.contains(inst.lRef)) {
					print(
						std::cerr, "Inst: %: cell % is not locked", loc,
						inst.lRef);
					return false;
				}
				print(output, "	movzx eax, BYTE PTR temp[%]", locks[inst.lRef]);
				print(output, "	mov BYTE PTR tape[rbx+%], al", inst.lRef);

				tempSlots.push_back(locks[inst.lRef]);
				locks.erase(inst.lRef);
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
)";
	globalArray(output, "tape", TAPE_LENGTH);
	globalArray(output, "temp", VARIABLE_LIMIT);

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
	if (args.optimizeSecondLevelLoops) { p.linearLoops(); }

	auto outputPath =
		std::filesystem::temp_directory_path() / "tmp-bf-assembly.s";

	compile(p.instructions(), outputPath);
	auto command = "g++ -g " + outputPath.string();
	if (!args.output.empty()) { command += " -o " + args.output.string(); }

	auto retCode = std::system(command.c_str());

	if (retCode == 0) {
		std::filesystem::remove(outputPath);
		return 0;
	}
	std::cerr << "Bug in compiler\n";
	return 1;
}
