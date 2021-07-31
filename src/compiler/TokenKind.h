#pragma once

#include "../common.h"

namespace compiler {

enum class TokenKind {
    kError,
    kEndOfLine,
    kEndOfFile,
    kIdentifier,
    kComment,

    // literals
    kNumberLiteral,
    kStringLiteral,

    // punctuation
    kLeftParenthesis,
    kRightParenthesis,
    kLeftBracket,
    kRightBracket,
    kLeftBrace,
    kRightBrace,
    kColon,
    kSemicolon,
    kComma,
    kDot,
    kPlusSign,
    kMinusSign,
    kMultiplicationSign,
    kDivisionSign,
    kEqualsSign,
    kNotEqualsSign,
    kLessThanSign,
    kLessThanEqualsSign,
    kGreaterThanSign,
    kGreaterThanEqualsSign,
    kCaret,

    // keywords
    kAnd,
    kAs,
    kBoolean,
    kBy,
    kCase,
    kCatch,
    kConst,
    kContinue,
    kDate,
    kDateTime,
    kDateTimeOffset,
    kDim,
    kDo,
    kEach,
    kElse,
    kEnd,
    kExit,
    kFalse,
    kFinally,
    kFor,
    kFrom,
    kFunction,
    kGroup,
    kIf,
    kIn,
    kInput,
    kInto,
    kJoin,
    kKey,
    kList,
    kLoop,
    kMap,
    kMod,
    kNext,
    kNot,
    kNumber,
    kOf,
    kOn,
    kOptional,
    kOr,
    kPrint,
    kRecord,
    kRethrow,
    kReturn,
    kSelect,
    kShared,
    kStep,
    kSub,
    kString,
    kThen,
    kThrow,
    kTimeSpan,
    kTimeZone,
    kTo,
    kTrue,
    kTry,
    kType,
    kUntil,
    kWend,
    kWhere,
    kWhile,
    kWith,
};

}  // namespace compiler
