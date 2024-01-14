namespace Core::Math {

namespace detail {
template <u64 N, u64 M> struct SimpleMatrix {
  std::array<std::array<float, N>, M> data{};
};
} // namespace detail

using Mat4 = detail::SimpleMatrix<4, 4>;
using Mat3 = detail::SimpleMatrix<3, 3>;
using Mat2 = detail::SimpleMatrix<2, 2>;

using Vec4 = detail::SimpleMatrix<4, 1>;
using Vec3 = detail::SimpleMatrix<3, 1>;
using Vec2 = detail::SimpleMatrix<2, 1>;

} // namespace Core::Math
