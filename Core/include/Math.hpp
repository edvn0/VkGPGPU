#pragma once

#include "Config.hpp"
#include "Types.hpp"

#include <glm/glm.hpp>

namespace Core::Math {

static consteval auto to_glm_qualifier() {
  if constexpr (Config::precision == Config::Precision::Low) {
    return glm::lowp;
  } else if constexpr (Config::precision == Config::Precision::Medium) {
    return glm::mediump;
  } else {
    return glm::highp;
  }
}

template <i32 R, i32 C>
using Matrix = glm::mat<R, C, floating, to_glm_qualifier()>;

template <i32 R> using Vector = glm::vec<R, floating, to_glm_qualifier()>;

using Mat4 = Matrix<4, 4>;
using Mat3 = Matrix<3, 3>;
using Mat2 = Matrix<2, 2>;

using Vec4 = Vector<4>;
using Vec3 = Vector<3>;
using Vec2 = Vector<2>;

template <class MatrixType>
concept SubmitsRowAndColumnTypeStatically = requires {
  typename std::decay_t<MatrixType>::row_type;
  typename std::decay_t<MatrixType>::col_type;
  requires std::decay_t<MatrixType>::row_type::length() > 0;
  requires std::decay_t<MatrixType>::col_type::length() > 0;
};

template <SubmitsRowAndColumnTypeStatically MatrixType>
static constexpr auto for_each(MatrixType &&matrix, auto &&callback) {
  constexpr auto R = std::decay_t<MatrixType>::row_type::length();
  constexpr auto C = std::decay_t<MatrixType>::col_type::length();
  for (auto r = 0; r < R; ++r) {
    for (auto c = 0; c < C; ++c) {
      callback(std::forward<MatrixType>(matrix)[r][c]);
    }
  }
}

} // namespace Core::Math
