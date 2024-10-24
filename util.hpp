#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

template <typename S> inline void print(S& s, std::string_view fmt) {
	s << fmt << "\n";
}

template <typename S, typename T, typename... Args>
void print(S& s, std::string_view fmt, const T& t, Args... args) {
	auto i = fmt.find('%');
	s << fmt.substr(0, i);
	fmt.remove_prefix(i);
	if (fmt.empty()) {
		s << "\n";
		return;
	}
	fmt.remove_prefix(1);
	s << t;
	print(s, fmt, args...);
}

#define debug(FMT, ...) \
	print(std::cerr, "%:%: " FMT, __FILE__, __LINE__, __VA_ARGS__)

struct Args {
	std::filesystem::path input;
	std::filesystem::path output;
	bool profile = false;
	bool optimizeSimpleLoops = true;
	bool optimizeScans = true;
	bool linearizeLoops = true;
};

Args argparse(int argc, char* argv[]) {
	Args a;
	std::vector<std::string> args(argv + 1, argv + argc);
	auto last = args.front();
	for (auto& arg : args) {
		if (arg == "-p") {
			a.profile = true;
		} else if (last == "-o") {
			a.output = arg;
		} else if (arg == "--no-simple-loop-optimize") {
			a.optimizeSimpleLoops = false;
		} else if (arg == "--no-scan-optimize") {
			a.optimizeScans = false;
		} else if (arg == "--no-linearize-loop-optimize") {
			a.linearizeLoops = false;
		} else if (a.input.empty()) {
			a.input = arg;
		}
		last = arg;
	}
	return a;
}

template <typename T> T revBits(T v) {
	auto r = v;
	auto s = std::numeric_limits<T>::digits - 1;
	for (v >>= 1; v; v >>= 1) {
		r <<= 1;
		r |= v & 1;
		s--;
	}
	r <<= s;
	return r;
}
