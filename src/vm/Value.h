#pragma once

#include "common.h"

namespace vm {

union Value {
    bool boolean;
    int64_t integer;
    uint64_t bits;

    bool operator==(const Value& rhs) const;
};

struct ValuePointerCompare {
    bool operator()(const Value& lhs, const Value*& rhs) { return lhs == *rhs; }
};

}  // namespace vm

namespace std {

template <>
struct hash<vm::Value> {
    std::size_t operator()(vm::Value const& k) const noexcept { return hash<uint64_t>{}(k.bits); }
};

}  // namespace std