#pragma once

#include "Texture.hpp"
#include "Types.hpp"

#include <vector>

namespace Core {

class GIFTexture {
public:
  static auto construct(const Device &, const FS::Path &) -> Scope<GIFTexture>;
  ~GIFTexture() = default;

  void on_update(float dt);

  [[nodiscard]] auto get_current_texture() const -> const Texture &;

private:
  GIFTexture(const Device &, const FS::Path &);

  const FS::Path path;
  struct Frame {
    Scope<Texture> texture;
    floating duration;
  };

  u32 current_frame_index;
  std::vector<Frame> frames;
  floating frame_timer;
  auto load_frames() -> void;
};

} // namespace Core
