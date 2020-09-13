#pragma once

#include "common.h"
#include "Object.h"
#include "ProcedureArtifact.h"

namespace vm {

class Procedure {
   public:
    bool isSystemProcedure;
    std::optional<std::string> source;
    std::optional<std::unique_ptr<ProcedureArtifact>> artifact;
};

}  // namespace vm