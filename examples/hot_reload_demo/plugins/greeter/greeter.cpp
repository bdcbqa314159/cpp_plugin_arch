#include "greeter.hpp"

#include "PluginExport.hpp"

namespace examples {

std::string Greeter::greet() { return "Hello from GreeterPlugin!"; }

}  // namespace examples

REGISTER_PLUGIN(examples::Greeter)
