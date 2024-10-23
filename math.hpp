#include <gmpxx.h>

#include <cassert>
#include <ostream>
#include <vector>

using D = mpq_class;
class Matrix {
	std::vector<std::vector<D>> mat;

   public:
	Matrix(size_t r, size_t c) {
		mat.clear();
		resize(r, c);
	}
	[[nodiscard]] auto rows() const { return mat.size(); }
	[[nodiscard]] auto cols() const { return mat[0].size(); }
	[[nodiscard]] auto isRowZero(size_t row) {
		return std::ranges::all_of(mat[row], [](auto i) { return i == 0; });
	}

	std::vector<D>& operator[](size_t row) { return mat[row]; }
	const std::vector<D>& operator[](size_t row) const { return mat[row]; }

	void resize(size_t r, size_t c) {
		for (auto& row : mat) { row.resize(c, 0); }
		mat.resize(r, std::vector<D>(c, 0));
	}

	[[nodiscard]] Matrix T() const {
		if (rows() == 0 || cols() == 0) { return {0, 0}; }
		Matrix t(cols(), rows());
		for (auto i = 0u; i < t.rows(); ++i) {
			for (auto j = 0u; j < t.cols(); ++j) { t[i][j] = mat[j][i]; }
		}
		return t;
	}
};

std::ostream& operator<<(std::ostream& os, const Matrix& m) {
	for (auto i = 0u; i < m.rows(); ++i) {
		for (auto j = 0u; j < m.cols(); ++j) { os << m[i][j] << "\t"; }
		os << "\n";
	}
	return os;
}

enum GaussianResult {
	ONE_SOLUTION = 0,
	MANY_SOLUTIONS,	// Tape Movement
	NO_SOLUTION,	// Increment by product of constant and reference
};
std::pair<GaussianResult, Matrix> gaussian(Matrix A, Matrix b) {
	assert(A.rows() == b.rows());

	// S = # samples
	// N = # coeffs to solve for
	// M = # distinct linear systems to solve

	const auto S = A.rows();
	const auto N = A.cols();
	const auto M = b.cols();
	for (auto i = 0u; i < S && i < N; ++i) {
		if (A[i][i] == 0) { return {MANY_SOLUTIONS, {0, 0}}; }
		auto t = A[i][i];
		for (auto j = 0u; j < N; ++j) { A[i][j] /= t; }
		for (auto j = 0u; j < M; ++j) { b[i][j] /= t; }
		for (auto k = 0u; k < S; ++k) {
			if (i == k) { continue; }
			t = A[k][i];
			for (auto j = 0u; j < N; ++j) { A[k][j] -= t * A[i][j]; }
			for (auto j = 0u; j < M; ++j) { b[k][j] -= t * b[i][j]; }
		}
	}
	for (auto i = N; i < S; ++i) {
		if (!b.isRowZero(i)) { return {NO_SOLUTION, {0, 0}}; }
	}
	b.resize(N, M);
	return {ONE_SOLUTION, b};
}
