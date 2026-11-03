#pragma once
#include "Module.h"

namespace sf
{
ModulePtr makeModule (const std::string& id);
std::vector<std::string> allModuleIds();

// Factories (one per module, defined in Source/modules/*.cpp)
ModulePtr makeFILTER();  ModulePtr makeGATE();    ModulePtr makeDENSITY(); ModulePtr makeDELAY();
ModulePtr makeREVERB();  ModulePtr makeREPEAT();  ModulePtr makeDRIVE();   ModulePtr makeRADIO();
ModulePtr makeDUCK();    ModulePtr makeENERGY();  ModulePtr makeISOLATOR(); ModulePtr makeBUILD();
ModulePtr makeGLUE();    ModulePtr makeLOWEND();  ModulePtr makeLIMIT();   ModulePtr makeROOMEQ();
ModulePtr makeCONTROL(); ModulePtr makeALLOC();   ModulePtr makeTAP();     ModulePtr makeSTRIP();
ModulePtr makeKICK();    ModulePtr makePERC();    ModulePtr makeRUMBLE();  ModulePtr makeBASS();
ModulePtr makeLEAD();    ModulePtr makePAD();     ModulePtr makeVOICE();   ModulePtr makeDECK();
} // namespace sf
