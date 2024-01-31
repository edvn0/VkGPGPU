#include "ClientApp.hpp"

#define GPGPU_ENTRY
#include "Entry.hpp"

extern auto Core::make_application(const Core::ApplicationProperties &props)
    -> Core::Scope<Core::App, Core::AppDeleter> {
  return Core::make_scope<ClientApp, Core::AppDeleter>(props);
}
