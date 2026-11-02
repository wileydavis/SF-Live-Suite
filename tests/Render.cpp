#include "core/Processor.h"
#include "core/Registry.h"
int main() { for (auto& id : sf::allModuleIds()) { SFProcessor p (sf::makeModule (id)); std::printf ("%s %d\n", id.c_str(), (int) p.allSpecs().size()); } return 0; }
