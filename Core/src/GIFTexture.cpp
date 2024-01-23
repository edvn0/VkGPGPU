#include "GIFTexture.hpp"

namespace Core {

auto GIFTexture::construct(const Device &device, const FS::Path &path)
    -> Scope<GIFTexture> {
  auto gif = Scope<GIFTexture>(new GIFTexture{device, path});
  return gif;
}

void GIFTexture::on_update(float dt) {
  frame_timer += dt;
  if (frame_timer >= frames[current_frame_index].duration) {
    frame_timer -= frames[current_frame_index].duration;
    current_frame_index = (current_frame_index + 1) % frames.size();
  }
}

auto GIFTexture::get_current_texture() const -> const Texture & {
  return *frames[current_frame_index].texture;
}

GIFTexture::GIFTexture(const Device &device, const FS::Path &path)
    : path(path) {
  load_frames();
}

auto GIFTexture::load_frames() -> void {}

} // namespace Core
