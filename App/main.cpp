#include "ClientApp.hpp"

#define GPGPU_ENTRY
#include "Entry.hpp"

extern auto Core::make_application(const Core::ApplicationProperties &props)
    -> Core::Scope<Core::App> {
  return Core::make_scope<ClientApp>(props);
}
