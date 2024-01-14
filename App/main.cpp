#include "ClientApp.hpp"

#define GPGPU_ENTRY
#include "Entry.hpp"

extern auto Core::make_application(const ApplicationProperties &props)
    -> Scope<App> {
  return Core::make_scope<ClientApp>(props);
}
