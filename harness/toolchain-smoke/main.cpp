#include <expected>
#include <iostream>
#include <string_view>
#include <utility>

#if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202202L
#error C++23 std::expected support is required
#endif
#if !defined(__cpp_lib_to_underlying) || __cpp_lib_to_underlying < 202102L
#error C++23 std::to_underlying support is required
#endif
static_assert(_MSVC_LANG > 202002L);
static_assert(__cplusplus > 202002L);
enum class ProbeValue : int { answer = 42 };
static_assert(std::to_underlying(ProbeValue::answer) == 42);

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--fail") {
        std::cerr << "INTENTIONAL_FAILURE: checking CTest failure propagation\n";
        return 23;
    }
    const std::expected<int, int> good{std::to_underlying(ProbeValue::answer)};
    const std::expected<int, int> bad{std::unexpected{7}};
    if (!good || *good != 42 || bad || bad.error() != 7) {
        return 1;
    }
    std::cout << "_MSC_VER=" << _MSC_VER << " _MSVC_LANG=" << _MSVC_LANG
              << " __cplusplus=" << __cplusplus
              << " __cpp_lib_expected=" << __cpp_lib_expected
              << " __cpp_lib_to_underlying=" << __cpp_lib_to_underlying << '\n';
    return 0;
}
