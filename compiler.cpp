#include <immintrin.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>

#include <filesystem>

#include "parser.hpp"
#include "util.hpp"

namespace manual {
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
		constexpr auto LARGE_JUMP = 16;
		if (std::abs(jump) >= LARGE_JUMP) {
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

		const auto shift = jump - (static_cast<int>(VEC_SZ) % jump);

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
		std::ofstream& output, const std::string& dest,
		const Instruction& inst) {
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

	bool compile(
		std::span<Instruction> code, const std::filesystem::path& path) {
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
					print(
						output, "	movzx eax, BYTE PTR tape[rbx+%]",
						inst.lRef);
					print(
						output, "	mov BYTE PTR temp[%], al",
						locks[inst.lRef]);

					break;
				case WRITE_UNLOCK:
					if (!locks.contains(inst.lRef)) {
						print(
							std::cerr, "Inst: %: cell % is not locked", loc,
							inst.lRef);
						return false;
					}
					print(
						output, "	movzx eax, BYTE PTR temp[%]",
						locks[inst.lRef]);
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
}  // namespace manual

namespace llvm {
	constexpr auto TAPE_LENGTH = 1000000u;
	class Compiler {
		LLVMContext ctx;
		std::unique_ptr<Module> module;
		IRBuilder<> builder;
		IntegerType *Tint8, *Tint32;
		AllocaInst* tape = nullptr;
		AllocaInst* ptr = nullptr;
		std::map<int, Value*> locks;
		std::vector<BasicBlock*> blocks;

		static auto constant(int value, IntegerType* type) {
			return ConstantInt::get(type, value);
		}

		auto ptrValue() {
			return static_cast<Value*>(builder.CreateLoad(Tint32, ptr));
		}

		auto cellAddr(int x) {
			auto* idx = builder.CreateAdd(ptrValue(), constant(x, Tint32));
			auto* addr = builder.CreateGEP(Tint8, tape, {idx});
			return addr;
		}

		auto loadCell(auto addr) { return builder.CreateLoad(Tint8, addr); }
		auto storeCell(auto addr, auto val) {
			return builder.CreateStore(val, addr);
		}

		auto cell(int x) {
			if (locks.contains(x)) { return locks[x]; }
			return static_cast<Value*>(loadCell(cellAddr(x)));
		}

		void compileIncr(const ::Instruction& i) {
			if (i.value == 0) { return; }
			Value* t = constant(i.value, Tint8);
			for (const auto& e : i.rRef) { t = builder.CreateMul(t, cell(e)); }
			auto* addr = cellAddr(i.lRef);
			auto* res = builder.CreateAdd(loadCell(addr), t);
			storeCell(addr, res);
		}

		bool compile(std::span<::Instruction> code) {
			for (const auto& i : code) {
				switch (i.code) {
					case NO_OP:
						break;
					case TAPE_M: {
						auto* newValue = builder.CreateAdd(
							ptrValue(), constant(i.value, Tint32));
						builder.CreateStore(newValue, ptr);
						break;
					}
					case SET_C:
						storeCell(cellAddr(i.lRef), constant(i.value, Tint8));
						break;
					case INCR: {
						compileIncr(i);
						break;
					}
					case WRITE:
						builder.CreateCall(
							module->getFunction("putchar"),
							{builder.CreateZExt(
								loadCell(cellAddr(0)), Tint32)});
						break;
					case READ:
						storeCell(
							cellAddr(0),
							builder.CreateTrunc(
								builder.CreateCall(
									module->getFunction("getchar"), {}),
								Tint8)

						);
						break;
					case WRITE_LOCK:
						if (locks.contains(i.lRef)) {
							::print(
								std::cerr, "Inst: cell % is already locked",
								i.lRef);
							return false;
						}
						locks[i.lRef] = cell(i.lRef);
						break;
					case WRITE_UNLOCK:
						if (!locks.contains(i.lRef)) {
							::print(
								std::cerr, "Inst: %: cell % is not locked",
								i.lRef);
							return false;
						}
						locks.erase(i.lRef);
						break;
					case JUMP_C: {
						auto* condBlock = BasicBlock::Create(
							ctx, "", blocks.back()->getParent());
						auto* loopBlock = BasicBlock::Create(
							ctx, "", blocks.back()->getParent());
						auto* endBlock = BasicBlock::Create(
							ctx, "", blocks.back()->getParent());

						builder.CreateBr(condBlock);
						builder.SetInsertPoint(condBlock);
						auto* cond =
							builder.CreateICmpNE(cell(0), constant(0, Tint8));
						builder.CreateCondBr(cond, loopBlock, endBlock);

						// Set loopBlockPush as current so all subsequent
						// instructions are pushed here
						builder.SetInsertPoint(loopBlock);
						// Push endBlock so that we can retrieve it later
						blocks.push_back(endBlock);

						break;
					}
					case JUMP_O: {
						// Jump to condition block
						builder.CreateBr(
							blocks.back()->getPrevNode()->getPrevNode());
						builder.SetInsertPoint(blocks.back());
						blocks.pop_back();
						break;
					}
				}
			}
			return true;
		}

		void optimize() {
			// Create the analysis managers.
			// These must be declared in this order so that they are destroyed
			// in the correct order due to inter-analysis-manager references.
			LoopAnalysisManager LAM;
			FunctionAnalysisManager FAM;
			CGSCCAnalysisManager CGAM;
			ModuleAnalysisManager MAM;

			// Create the new pass manager builder.
			// Take a look at the PassBuilder constructor parameters for more
			// customization, e.g. specifying a TargetMachine or various
			// debugging options.
			PassBuilder PB;

			// Register all the basic analyses with the managers.
			PB.registerModuleAnalyses(MAM);
			PB.registerCGSCCAnalyses(CGAM);
			PB.registerFunctionAnalyses(FAM);
			PB.registerLoopAnalyses(LAM);
			PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

			// Create the pass manager.
			// This one corresponds to a typical -O2 optimization pipeline.
			ModulePassManager MPM =
				PB.buildPerModuleDefaultPipeline(OptimizationLevel::O3);

			// Optimize the IR!
			MPM.run(*module, MAM);
		}

		bool object(const std::filesystem::path& path) {
			{
				std::error_code ec;
				raw_fd_ostream before("/tmp/before-IR-opt.ll", ec),
					after("/tmp/after-IR-opt.ll", ec);

				module->print(before, nullptr);
				optimize();
				module->print(after, nullptr);
			}
			auto TargetTriple = sys::getDefaultTargetTriple();
			InitializeAllTargetInfos();
			InitializeAllTargets();
			InitializeAllTargetMCs();
			InitializeAllAsmParsers();
			InitializeAllAsmPrinters();

			std::string Error;
			const auto* Target =
				TargetRegistry::lookupTarget(TargetTriple, Error);

			if (Target == nullptr) {
				errs() << Error;
				return false;
			}

			const auto* CPU = "generic";
			const auto* Features = "";

			TargetOptions opt;
			auto targetMachine =
				std::unique_ptr<TargetMachine>(Target->createTargetMachine(
					TargetTriple, CPU, Features, opt, Reloc::PIC_));

			module->setDataLayout(targetMachine->createDataLayout());
			module->setTargetTriple(TargetTriple);

			std::error_code EC;
			raw_fd_ostream dest(path.string(), EC, sys::fs::OF_None);

			if (EC) {
				errs() << "Could not open file: " << EC.message();
				return false;
			}

			legacy::PassManager pass;
			auto FileType = CodeGenFileType::ObjectFile;

			if (targetMachine->addPassesToEmitFile(
					pass, dest, nullptr, FileType)) {
				errs() << "TargetMachine can't emit a file of this type";
				return false;
			}

			pass.run(*module);
			dest.flush();
			return true;
		}

	   public:
		Compiler()
			: module(std::make_unique<Module>("BF Module", ctx)),
			  builder(ctx),
			  Tint8(builder.getInt8Ty()),
			  Tint32(builder.getInt32Ty()) {}

		bool compile(
			std::span<::Instruction> code, const std::filesystem::path& path) {
			{
				// Create declaration for putchar and getchar
				auto* functionType = FunctionType::get(Tint32, {Tint32}, false);
				Function::Create(
					functionType, Function::ExternalLinkage, "putchar",
					module.get());

				functionType = FunctionType::get(Tint32, false);
				Function::Create(
					functionType, Function::ExternalLinkage, "getchar",
					module.get());
			}

			auto* functionReturnType = FunctionType::get(Tint32, false);
			auto* mainFunction = Function::Create(
				functionReturnType, Function::ExternalLinkage, "main",
				module.get());

			auto* body = BasicBlock::Create(ctx, "body", mainFunction);
			builder.SetInsertPoint(body);

			blocks.push_back(body);

			tape = builder.CreateAlloca(Tint8, constant(TAPE_LENGTH, Tint32));
			builder.CreateMemSet(
				builder.CreateGEP(Tint8, tape, {constant(0, Tint32)}),
				constant(0, Tint8), constant(TAPE_LENGTH, Tint32),
				tape->getAlign());

			{
				ptr = builder.CreateAlloca(Tint32, constant(1, Tint32));
				// auto* addr = builder.CreateGEP(Tint32, ptr, {constant(0)});
				builder.CreateStore(constant(TAPE_LENGTH / 2, Tint32), ptr);
			}

			auto result = compile(code);
			// auto result = true;
			builder.CreateRet(constant(0, Tint32));

			return result && !verifyModule(*module, &llvm::errs()) &&
				   object(path);
		}

		void print() { module->print(llvm::errs(), nullptr); }
	};

}  // namespace llvm

int main(int argc, char* argv[]) {
	llvm::InitLLVM(argc, argv);
	auto args = argparse(argc, argv);
	args.optimizeScans = false;
	Program p(args);

	if (!p.isOK()) {
		std::cerr << p.error() << "\n";
		return 1;
	}

	auto outputPath =
		std::filesystem::temp_directory_path() / "tmp-bf-object.o";

	llvm::Compiler compiler;
	if (compiler.compile(p.instructions(), outputPath)) {
		// compiler.print();
	} else {
		std::cerr << "Unable to generate LLVM IR\n";
		return 1;
	}
	// manual::compile(p.instructions(), outputPath);
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
