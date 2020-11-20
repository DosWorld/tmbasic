#pragma once

#include "../../common.h"

namespace util {

struct membuf : std::streambuf {
    membuf(char* begin, char* end);
    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in)
        override;
    pos_type seekpos(pos_type sp, std::ios_base::openmode which) override;
};

}  // namespace util