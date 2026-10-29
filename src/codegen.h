#ifndef ROBOTLANG_CODEGEN_H
#define ROBOTLANG_CODEGEN_H

#include "optimizer.h"
#include <ostream>

namespace robotlang::extension {
class Codegen {
public:
    explicit Codegen(std::ostream& out, Integer tickLimit = 100000);
    void emit(const Program& program, const std::vector<Rewrite>& rewrites = {}, bool optimized = false);
private:
    std::ostream& out_;
    Integer tickLimit_;
};
}
#endif
