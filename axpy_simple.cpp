// Simplified axpy without system headers
// Manual array implementation to avoid std::array header issues

template<typename T, unsigned long N>
struct Array {
    T data[N];

    T* begin() { return data; }
    T* end() { return data + N; }
    unsigned long size() const { return N; }
    T* get_data() { return data; }

    T& operator[](unsigned long i) { return data[i]; }
    const T& operator[](unsigned long i) const { return data[i]; }
};

static const unsigned long kElements = 1024;

static void axpy(double a, const double *x, double *y, unsigned long n) {
    for (unsigned long i = 0; i < n; ++i) {
        y[i] = a * x[i] + y[i];
    }
}

static double checksum(const double *values, unsigned long n) {
    double sum = 0.0;
    for (unsigned long i = 0; i < n; ++i) {
        sum += values[i];
    }
    return sum;
}

int main() {
    const double a = 2.5;

    Array<double, kElements> x;
    Array<double, kElements> y;

    // Initialize arrays
    for (unsigned long i = 0; i < kElements; ++i) {
        x[i] = (double)i;
        y[i] = (double)i * 2.0;
    }

    axpy(a, x.get_data(), y.get_data(), y.size());

    const double result = checksum(y.get_data(), y.size());
    const double expected = (a + 2.0) * ((double)((kElements - 1) * kElements)) * 0.5;

    // Simple check
    double diff = result - expected;
    if (diff < 0) diff = -diff;
    double rel_error = diff / expected;

    if (rel_error > 1e-9) {
        return 1;
    }

    return 0;
}
