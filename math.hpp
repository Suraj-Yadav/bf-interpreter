#include <gmpxx.h>

#include <cassert>
#include <numeric>
#include <ostream>
#include <vector>

using T = mpq_class;
class Matrix {
	std::vector<std::vector<T>> mat;

   public:
	Matrix(size_t r, size_t c) {
		mat.clear();
		mat.resize(r, std::vector<T>(c, 0));
	}
	[[nodiscard]] auto rows() const { return mat.size(); }
	[[nodiscard]] auto cols() const { return mat[0].size(); }

	std::vector<T>& operator[](size_t row) { return mat[row]; }
	const std::vector<T>& operator[](size_t row) const { return mat[row]; }
};

std::ostream& operator<<(std::ostream& os, const Matrix& m) {
	for (auto i = 0u; i < m.rows(); ++i) {
		for (auto j = 0u; j < m.cols(); ++j) { os << m[i][j] << "\t"; }
		os << "\n";
	}
	return os;
}

bool lup(Matrix& A, std::vector<int>& pi) {
	const auto N = A.rows();
	pi.resize(N);
	std::iota(pi.begin(), pi.end(), 0);

	for (auto k = 0u; k < N; ++k) {
		T p(0);
		auto k1 = k;
		for (auto i = k; i < N; ++i) {
			if (abs(A[i][k]) > p) {
				p = abs(A[i][k]);
				k1 = i;
			}
		}
		if (p == 0) { return false; }
		std::swap(pi[k], pi[k1]);
		std::swap(A[k], A[k1]);
		for (auto i = k + 1; i < N; ++i) {
			A[i][k] = A[i][k] / A[k][k];
			for (auto j = k + 1; j < N; ++j) {
				A[i][j] = A[i][j] - A[i][k] * A[k][j];
			}
		}
	}

	return true;
}

Matrix solve(Matrix A, const Matrix& b) {
	assert(A.rows() == b.cols());
	std::vector<int> pi;

	if (!lup(A, pi)) { return {0, 0}; }
	const auto N = A.rows();
	const auto M = b.rows();
	std::vector<T> y(N, 0);
	Matrix x(M, N);
	for (auto k = 0u; k < M; ++k) {
		for (auto i = 0u; i < N; ++i) {
			y[i] = b[k][pi[i]];
			for (auto j = 0u; j < i; ++j) { y[i] = y[i] - A[i][j] * y[j]; }
		}
		for (auto i = N - 1;; --i) {
			for (auto j = i + 1; j < N; ++j) {
				y[i] = y[i] - A[i][j] * x[k][j];
			}
			x[k][i] = y[i] / A[i][i];
			if (i == 0) { break; }
		}
	}

	return x;
}
