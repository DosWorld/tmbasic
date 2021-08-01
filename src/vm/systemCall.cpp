#include "systemCall.h"
#include "Error.h"
#include "List.h"
#include "Map.h"
#include "Optional.h"
#include "String.h"
#include "TimeZone.h"
#include "constants.h"
#include "date.h"
#include "util/decimal.h"

namespace vm {

typedef void (*SystemCallFunc)(const SystemCallInput&, SystemCallResult*);

static bool _systemCallsInitialized = false;
static std::vector<SystemCallFunc> _systemCalls;

SystemCallInput::SystemCallInput(
    const std::array<Value, kValueStackSize>& valueStack,
    const std::array<boost::local_shared_ptr<Object>, kObjectStackSize>& objectStack,
    int valueStackIndex,
    int objectStackIndex,
    std::istream* consoleInputStream,
    std::ostream* consoleOutputStream,
    const Value& errorCode,
    const std::string& errorMessage)
    : valueStack(valueStack),
      objectStack(objectStack),
      valueStackIndex(valueStackIndex),
      objectStackIndex(objectStackIndex),
      consoleInputStream(consoleInputStream),
      consoleOutputStream(consoleOutputStream),
      errorCode(errorCode),
      errorMessage(errorMessage) {}

static void systemCallAvailableLocales(const SystemCallInput& /*unused*/, SystemCallResult* result) {
    int32_t count = 0;
    const auto* locales = icu::Locale::getAvailableLocales(count);

    auto objectListBuilder = ObjectListBuilder();
    for (int32_t i = 0; i < count; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const auto* name = locales[i].getName();
        objectListBuilder.items.push_back(boost::make_local_shared<String>(name, strlen(name)));
    }

    result->returnedObject = boost::make_local_shared<ObjectList>(&objectListBuilder);
}

static void systemCallAvailableTimeZones(const SystemCallInput& /*unused*/, SystemCallResult* result) {
    auto iter = std::unique_ptr<icu::StringEnumeration>(icu::TimeZone::createEnumeration());

    auto objectListBuilder = ObjectListBuilder();
    const char* item = nullptr;
    auto status = U_ZERO_ERROR;
    while ((item = iter->next(nullptr, status)) != nullptr) {
        objectListBuilder.items.push_back(boost::make_local_shared<String>(item, strlen(item)));
    }

    result->returnedObject = boost::make_local_shared<ObjectList>(&objectListBuilder);
}

static const icu::Locale& getBreakIteratorLocale(const icu::UnicodeString& name) {
    std::string nameUtf8;
    name.toUTF8String(nameUtf8);

    int32_t count = 0;
    const auto* locales = icu::BreakIterator::getAvailableLocales(count);
    for (int32_t i = 0; i < count; i++) {
        const auto& locale = locales[i];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (nameUtf8 == locale.getName()) {
            return locale;
        }
    }

    throw Error(ErrorCode::kInvalidLocaleName, "The locale name is invalid or unsupported.");
}

static void systemCallCharactersCore(
    const SystemCallInput& input,
    const icu::Locale& locale,
    SystemCallResult* result) {
    const auto& str = dynamic_cast<const String&>(input.getObject(-1));
    auto status = U_ZERO_ERROR;
    auto breakIterator =
        std::unique_ptr<icu::BreakIterator>(icu::BreakIterator::createCharacterInstance(locale, status));
    breakIterator->setText(str.value);
    auto index1 = 0;
    auto index2 = breakIterator->first();
    auto objectListBuilder = ObjectListBuilder();
    while (index2 != icu::BreakIterator::DONE) {
        if (index2 > 0) {
            objectListBuilder.items.push_back(
                boost::make_local_shared<String>(icu::UnicodeString(str.value, index1, index2)));
        }

        index1 = index2;
        index2 = breakIterator->next();
    }

    result->returnedObject = boost::make_local_shared<ObjectList>(&objectListBuilder);
}

static void systemCallCharacters1(const SystemCallInput& input, SystemCallResult* result) {
    systemCallCharactersCore(input, icu::Locale::getUS(), result);
}

static void systemCallCharacters2(const SystemCallInput& input, SystemCallResult* result) {
    const auto& localeName = dynamic_cast<const String&>(input.getObject(-2));
    const auto& locale = getBreakIteratorLocale(localeName.value);
    systemCallCharactersCore(input, locale, result);
}

static void systemCallChr(const SystemCallInput& input, SystemCallResult* result) {
    auto value = input.getValue(-1).getInt64();
    auto ch = static_cast<UChar32>(value);
    result->returnedObject = boost::make_local_shared<String>(ch > 0 ? icu::UnicodeString(ch) : icu::UnicodeString());
}

static void systemCallCounterIsPastLimit(const SystemCallInput& input, SystemCallResult* result) {
    // used with 'for' loops
    const auto& counter = input.getValue(-3).num;
    const auto& limit = input.getValue(-2).num;
    const auto& step = input.getValue(-1).num;
    bool condition{};
    if (step >= 0) {
        condition = counter > limit;
    } else {
        condition = counter < limit;
    }
    result->returnedValue.setBoolean(condition);
}

static void systemCallDateFromParts(const SystemCallInput& input, SystemCallResult* result) {
    auto year = input.getValue(-3).getInt32();
    auto month = input.getValue(-2).getInt32();
    auto day = input.getValue(-1).getInt32();
    result->returnedValue = newDate(year, month, day);
}

static void systemCallDateTimeFromParts(const SystemCallInput& input, SystemCallResult* result) {
    auto year = input.getValue(-7).getInt32();
    auto month = input.getValue(-6).getInt32();
    auto day = input.getValue(-5).getInt32();
    auto hour = input.getValue(-4).getInt32();
    auto minute = input.getValue(-3).getInt32();
    auto second = input.getValue(-2).getInt32();
    auto millisecond = input.getValue(-1).getInt32();
    result->returnedValue = newDateTime(year, month, day, hour, minute, second, millisecond);
}

static void systemCallDateTimeOffsetFromParts(const SystemCallInput& input, SystemCallResult* result) {
    auto year = input.getValue(-7).getInt32();
    auto month = input.getValue(-6).getInt32();
    auto day = input.getValue(-5).getInt32();
    auto hour = input.getValue(-4).getInt32();
    auto minute = input.getValue(-3).getInt32();
    auto second = input.getValue(-2).getInt32();
    auto millisecond = input.getValue(-1).getInt32();
    const auto& timeZone = dynamic_cast<const TimeZone&>(input.getObject(-1));
    auto dateTime = newDateTime(year, month, day, hour, minute, second, millisecond);
    Value offset{ timeZone.getUtcOffset(dateTime.num) };
    result->returnedObject = newDateTimeOffset(dateTime, offset);
}

static void systemCallDateToString(const SystemCallInput& input, SystemCallResult* result) {
    const auto& date = input.getValue(-1);
    result->returnedObject = dateToString(date);
}

static void systemCallDateTimeToString(const SystemCallInput& input, SystemCallResult* result) {
    const auto& dateTime = input.getValue(-1);
    result->returnedObject = dateTimeToString(dateTime);
}

static void systemCallDateTimeOffsetToString(const SystemCallInput& input, SystemCallResult* result) {
    const auto& date = dynamic_cast<const Record&>(input.getObject(-1));
    result->returnedObject = dateTimeOffsetToString(date);
}

static void systemCallHasValueV(const SystemCallInput& input, SystemCallResult* result) {
    const auto& opt = dynamic_cast<const ValueOptional&>(input.getObject(-1));
    result->returnedValue.setBoolean(opt.item.has_value());
}

static void systemCallHasValueO(const SystemCallInput& input, SystemCallResult* result) {
    const auto& opt = dynamic_cast<const ObjectOptional&>(input.getObject(-1));
    result->returnedValue.setBoolean(opt.item.has_value());
}

static void systemCallHours(const SystemCallInput& input, SystemCallResult* result) {
    result->returnedValue.num = input.getValue(-1).num * U_MILLIS_PER_HOUR;
}

static void systemCallInputString(const SystemCallInput& input, SystemCallResult* result) {
    std::string line;
    std::getline(*input.consoleInputStream, line);
    result->returnedObject = boost::make_local_shared<String>(line);
}

static void systemCallLen(const SystemCallInput& input, SystemCallResult* result) {
    const auto& str = dynamic_cast<const String&>(input.getObject(-1)).value;
    result->returnedValue.num = str.length();
}

static void systemCallNumberToString(const SystemCallInput& input, SystemCallResult* result) {
    const auto& value = input.getValue(-1);
    result->returnedObject = boost::make_local_shared<String>(value.getString());
}

static void systemCallObjectListGet(const SystemCallInput& input, SystemCallResult* result) {
    const auto& objectList = dynamic_cast<const ObjectList&>(input.getObject(-1));
    const auto& index = input.getValue(-1).getInt64();
    result->returnedObject = objectList.items.at(index);
    assert(result->returnedObject != nullptr);
}

static void systemCallObjectListLength(const SystemCallInput& input, SystemCallResult* result) {
    const auto& objectList = dynamic_cast<const ObjectList&>(input.getObject(-1));
    result->returnedValue.num = objectList.items.size();
}

static void systemCallTimeZoneFromName(const SystemCallInput& input, SystemCallResult* result) {
    const auto& name = dynamic_cast<const String&>(input.getObject(-1));
    auto icuTimeZone = std::unique_ptr<icu::TimeZone>(icu::TimeZone::createTimeZone(name.value));
    icu::UnicodeString nameString;
    if ((icuTimeZone->getID(nameString) == UCAL_UNKNOWN_ZONE_ID) != 0) {
        throw Error(ErrorCode::kInvalidTimeZone, "The specified time zone was not found.");
    }
    result->returnedObject = boost::make_local_shared<TimeZone>(std::move(icuTimeZone));
}

static void systemCallUtcOffset(const SystemCallInput& input, SystemCallResult* result) {
    const auto& timeZone = dynamic_cast<const TimeZone&>(input.getObject(-1));
    const auto& dateTime = input.getValue(-1);
    result->returnedValue.num = timeZone.getUtcOffset(dateTime.num);
}

static void systemCallValueV(const SystemCallInput& input, SystemCallResult* result) {
    const auto& opt = dynamic_cast<const ValueOptional&>(input.getObject(-1));
    if (!opt.item.has_value()) {
        throw Error(ErrorCode::kValueNotPresent, "Optional value is not present.");
    }
    result->returnedValue = *opt.item;
}

static void systemCallValueListGet(const SystemCallInput& input, SystemCallResult* result) {
    const auto& valueList = dynamic_cast<const ValueList&>(input.getObject(-1));
    const auto& index = input.getValue(-1).getInt64();
    result->returnedValue = valueList.items.at(index);
}

static void systemCallValueO(const SystemCallInput& input, SystemCallResult* result) {
    const auto& opt = dynamic_cast<const ObjectOptional&>(input.getObject(-1));
    if (!opt.item.has_value()) {
        throw Error(ErrorCode::kValueNotPresent, "Optional value is not present.");
    }
    result->returnedObject = *opt.item;
}

static boost::local_shared_ptr<String> stringConcat(const ObjectList& objectList, const String& separator) {
    std::vector<char16_t> uchars{};
    bool isFirst = true;
    size_t numSepCodeUnits = separator.value.length();
    for (const auto& object : objectList.items) {
        if (!isFirst && numSepCodeUnits > 0) {
            uchars.reserve(uchars.size() + numSepCodeUnits);
            for (size_t i = 0; i < numSepCodeUnits; i++) {
                uchars.push_back(separator.value.charAt(i));
            }
        }
        isFirst = false;

        const auto& str = dynamic_cast<const String&>(*object);
        size_t numCodeUnits = str.value.length();
        uchars.reserve(uchars.size() + numCodeUnits);
        for (size_t i = 0; i < numCodeUnits; i++) {
            uchars.push_back(str.value.charAt(i));
        }
    }
    icu::UnicodeString str{ uchars.data(), static_cast<int32_t>(uchars.size()) };
    return boost::make_local_shared<String>(std::move(str));
}

static void throwFileError(int posixError, const std::string& filePath) {
    switch (posixError) {
        case ENOENT:
            throw Error(ErrorCode::kFileNotFound, fmt::format("The file \"{}\" does not exist.", filePath));
        case EACCES:
            throw Error(ErrorCode::kAccessDenied, fmt::format("Access to the file \"{}\" was denied.", filePath));
        case ENAMETOOLONG:
            throw Error(ErrorCode::kPathTooLong, fmt::format("The path \"{}\" is too long.", filePath));
        case ENOSPC:
            throw Error(
                ErrorCode::kDiskFull, fmt::format("The disk containing the file \"{}\" is out of space.", filePath));
        case EISDIR:
            throw Error(ErrorCode::kPathIsDirectory, fmt::format("The path \"{}\" is a directory.", filePath));
        default:
            throw Error(
                ErrorCode::kIoFailure,
                fmt::format("Failed to access the file \"{}\". {}", filePath, strerror(posixError)));
    }
}

static void initSystemCall(SystemCall which, SystemCallFunc func) {
    auto index = static_cast<size_t>(which);

    while (_systemCalls.size() <= index) {
        _systemCalls.push_back(nullptr);
    }

    _systemCalls.at(index) = func;
}

void initSystemCalls() {
    if (_systemCallsInitialized) {
        return;
    }

    initSystemCall(SystemCall::kAbs, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num.abs();
    });
    initSystemCall(SystemCall::kAcos, [](const auto& input, auto* result) {
        result->returnedValue.setDouble(std::acos(input.getValue(-1).getDouble()));
    });
    initSystemCall(SystemCall::kAsin, [](const auto& input, auto* result) {
        result->returnedValue.setDouble(std::asin(input.getValue(-1).getDouble()));
    });
    initSystemCall(SystemCall::kAtan, [](const auto& input, auto* result) {
        result->returnedValue.setDouble(std::atan(input.getValue(-1).getDouble()));
    });
    initSystemCall(SystemCall::kAtan2, [](const auto& input, auto* result) {
        result->returnedValue.setDouble(std::atan2(input.getValue(-2).getDouble(), input.getValue(-1).getDouble()));
    });
    initSystemCall(SystemCall::kAvailableLocales, systemCallAvailableLocales);
    initSystemCall(SystemCall::kAvailableTimeZones, systemCallAvailableTimeZones);
    initSystemCall(SystemCall::kCeil, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num.ceil();
    });
    initSystemCall(SystemCall::kCharacters1, systemCallCharacters1);
    initSystemCall(SystemCall::kCharacters2, systemCallCharacters2);
    initSystemCall(SystemCall::kChr, systemCallChr);
    initSystemCall(SystemCall::kCodePoints, [](const auto& input, auto* result) {
        const auto& str = dynamic_cast<const String&>(input.getObject(-1));
        auto numCodePoints = str.value.countChar32();
        std::vector<int32_t> codePoints(numCodePoints);
        UErrorCode status = U_ZERO_ERROR;
        str.value.toUTF32(codePoints.data(), numCodePoints, status);
        if (U_FAILURE(status)) {
            throw Error(
                ErrorCode::kInternalIcuError,
                fmt::format("Failed to convert the string to code points. ICU error: {}", u_errorName(status)));
        }
        ValueListBuilder vlb{};
        for (const auto& codePoint : codePoints) {
            vlb.items.push_back(Value{ codePoint });
        }
        result->returnedObject = boost::make_local_shared<ValueList>(&vlb);
    });
    initSystemCall(SystemCall::kCodeUnit1, [](const auto& input, auto* result) {
        const auto& str = dynamic_cast<const String&>(input.getObject(-1));
        auto codeUnit = str.value.charAt(0);
        result->returnedValue.num = codeUnit == 0xFFFF ? 0 : codeUnit;
    });
    initSystemCall(SystemCall::kCodeUnit2, [](const auto& input, auto* result) {
        const auto& str = dynamic_cast<const String&>(input.getObject(-1));
        const auto& index = input.getValue(-1).getInt64();
        auto codeUnit = str.value.charAt(index);
        result->returnedValue.num = codeUnit == 0xFFFF ? 0 : codeUnit;
    });
    initSystemCall(SystemCall::kCodeUnits, [](const auto& input, auto* result) {
        const auto& str = dynamic_cast<const String&>(input.getObject(-1));
        ValueListBuilder b{};
        size_t numCodeUnits = str.value.length();
        for (size_t i = 0; i < numCodeUnits; i++) {
            b.items.push_back(Value{ static_cast<int64_t>(str.value.charAt(i)) });
        }
        result->returnedObject = boost::make_local_shared<ValueList>(&b);
    });
    initSystemCall(SystemCall::kConcat1, [](const auto& input, auto* result) {
        const auto& objectList = dynamic_cast<const ObjectList&>(input.getObject(-1));
        String empty{ "", 0 };
        result->returnedObject = stringConcat(objectList, empty);
    });
    initSystemCall(SystemCall::kConcat2, [](const auto& input, auto* result) {
        const auto& objectList = dynamic_cast<const ObjectList&>(input.getObject(-2));
        const auto& separator = dynamic_cast<const String&>(input.getObject(-1));
        result->returnedObject = stringConcat(objectList, separator);
    });
    initSystemCall(SystemCall::kCos, [](const auto& input, auto* result) {
        result->returnedValue.setDouble(std::cos(input.getValue(-1).getDouble()));
    });
    initSystemCall(SystemCall::kCounterIsPastLimit, systemCallCounterIsPastLimit);
    initSystemCall(SystemCall::kDateFromParts, systemCallDateFromParts);
    initSystemCall(SystemCall::kDateTimeFromParts, systemCallDateTimeFromParts);
    initSystemCall(SystemCall::kDateTimeOffsetFromParts, systemCallDateTimeOffsetFromParts);
    initSystemCall(SystemCall::kDateToString, systemCallDateToString);
    initSystemCall(SystemCall::kDateTimeToString, systemCallDateTimeToString);
    initSystemCall(SystemCall::kDateTimeOffsetToString, systemCallDateTimeOffsetToString);
    initSystemCall(SystemCall::kDays, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num * U_MILLIS_PER_DAY;
    });
    initSystemCall(SystemCall::kDeleteFile, [](const auto& input, auto* result) {
        const auto& path = dynamic_cast<const String&>(input.getObject(-1));
        auto pathStr = path.toUtf8();
        if (unlink(pathStr.c_str()) != 0) {
            auto err = errno;
            if (err == ENOENT) {
                // not an error
                return;
            }
            throwFileError(err, pathStr);
        }
    });
    initSystemCall(
        SystemCall::kErrorCode, [](const auto& input, auto* result) { result->returnedValue = input.errorCode; });
    initSystemCall(SystemCall::kErrorMessage, [](const auto& input, auto* result) {
        result->returnedObject = boost::make_local_shared<String>(input.errorMessage);
    });
    initSystemCall(SystemCall::kExp, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num.exp();
    });
    initSystemCall(SystemCall::kFloor, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num.floor();
    });
    initSystemCall(SystemCall::kFlushConsoleOutput, [](const auto& input, auto* /*result*/) {
        input.consoleOutputStream->flush();
    });
    initSystemCall(SystemCall::kHasValueO, systemCallHasValueO);
    initSystemCall(SystemCall::kHasValueV, systemCallHasValueV);
    initSystemCall(SystemCall::kHours, systemCallHours);
    initSystemCall(SystemCall::kInputString, systemCallInputString);
    initSystemCall(SystemCall::kListLen, [](const auto& input, auto* result) {
        result->returnedValue.num = dynamic_cast<const ListBase&>(input.getObject(-1)).size();
    });
    initSystemCall(SystemCall::kLog, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num.ln();
    });
    initSystemCall(SystemCall::kLog10, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num.log10();
    });
    initSystemCall(SystemCall::kMilliseconds, [](const auto& input, auto* result) {
        result->returnedValue = input.getValue(-1);  // already in milliseconds!
    });
    initSystemCall(SystemCall::kMinutes, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num * U_MILLIS_PER_MINUTE;
    });
    initSystemCall(SystemCall::kNumberAdd, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-2).num + input.getValue(-1).num;
    });
    initSystemCall(SystemCall::kNumberDivide, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-2).num / input.getValue(-1).num;
    });
    initSystemCall(SystemCall::kNumberEquals, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-2).num == input.getValue(-1).num ? 1 : 0;
    });
    initSystemCall(SystemCall::kNumberGreaterThan, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-2).num > input.getValue(-1).num ? 1 : 0;
    });
    initSystemCall(SystemCall::kNumberGreaterThanEquals, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-2).num >= input.getValue(-1).num ? 1 : 0;
    });
    initSystemCall(SystemCall::kNumberLessThan, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-2).num < input.getValue(-1).num ? 1 : 0;
    });
    initSystemCall(SystemCall::kNumberLessThanEquals, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-2).num <= input.getValue(-1).num ? 1 : 0;
    });
    initSystemCall(SystemCall::kNumberModulus, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-2).num % input.getValue(-1).num;
    });
    initSystemCall(SystemCall::kNumberMultiply, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-2).num * input.getValue(-1).num;
    });
    initSystemCall(SystemCall::kNumberNotEquals, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-2).num != input.getValue(-1).num ? 1 : 0;
    });
    initSystemCall(SystemCall::kNumberSubtract, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-2).num - input.getValue(-1).num;
    });
    initSystemCall(SystemCall::kNumberToString, systemCallNumberToString);
    initSystemCall(SystemCall::kObjectListGet, systemCallObjectListGet);
    initSystemCall(SystemCall::kObjectListLength, systemCallObjectListLength);
    initSystemCall(SystemCall::kObjectOptionalNewMissing, [](const auto& /*input*/, auto* result) {
        result->returnedObject = boost::make_local_shared<ObjectOptional>();
    });
    initSystemCall(SystemCall::kObjectOptionalNewPresent, [](const auto& input, auto* result) {
        result->returnedObject = boost::make_local_shared<ObjectOptional>(input.getObjectPtr(-1));
    });
    initSystemCall(SystemCall::kObjectToObjectMapNew, [](const auto& /*input*/, auto* result) {
        result->returnedObject = boost::make_local_shared<ObjectToObjectMap>();
    });
    initSystemCall(SystemCall::kObjectToValueMapNew, [](const auto& /*input*/, auto* result) {
        result->returnedObject = boost::make_local_shared<ObjectToValueMap>();
    });
    initSystemCall(SystemCall::kPow, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-2).num.pow(input.getValue(-1).num);
    });
    initSystemCall(SystemCall::kRound, [](const auto& input, auto* result) {
        result->returnedValue.num = util::round(input.getValue(-1).num);
    });
    initSystemCall(SystemCall::kPrintString, [](const auto& input, auto* /*result*/) {
        *input.consoleOutputStream << dynamic_cast<const String&>(input.getObject(-1)).toUtf8();
    });
    initSystemCall(SystemCall::kReadFileLines, [](const auto& input, auto* result) {
        auto filePath = dynamic_cast<const String&>(input.getObject(-1)).toUtf8();
        std::ifstream stream{ filePath };
        if (stream.fail()) {
            throwFileError(errno, filePath);
        }
        ObjectListBuilder builder{};
        std::string line;
        while (std::getline(stream, line)) {
            if (stream.fail()) {
                throwFileError(errno, filePath);
            }
            builder.items.push_back(boost::make_local_shared<String>(line));
        }
        if (stream.fail() && !stream.eof()) {
            throwFileError(errno, filePath);
        }
        result->returnedObject = boost::make_local_shared<ObjectList>(&builder);
    });
    initSystemCall(SystemCall::kReadFileText, [](const auto& input, auto* result) {
        auto filePath = dynamic_cast<const String&>(input.getObject(-1)).toUtf8();
        std::ifstream stream{ filePath };
        if (stream.fail()) {
            throwFileError(errno, filePath);
        }
        std::ostringstream ss;
        ss << stream.rdbuf();
        if (stream.fail() && !stream.eof()) {
            throwFileError(errno, filePath);
        }
        result->returnedObject = boost::make_local_shared<String>(ss.str());
    });
    initSystemCall(SystemCall::kSeconds, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num * U_MILLIS_PER_SECOND;
    });
    initSystemCall(SystemCall::kSin, [](const auto& input, auto* result) {
        result->returnedValue.setDouble(std::sin(input.getValue(-1).getDouble()));
    });
    initSystemCall(SystemCall::kSqr, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num.sqrt();
    });
    initSystemCall(SystemCall::kStringFromCodePoints, [](const auto& input, auto* result) {
        const auto& valueList = dynamic_cast<const ValueList&>(input.getObject(-1));
        auto numCodePoints = valueList.size();
        std::vector<int32_t> codePoints{};
        codePoints.reserve(numCodePoints);
        for (size_t i = 0; i < numCodePoints; ++i) {
            codePoints.push_back(valueList.items.at(i).getInt32());
        }
        result->returnedObject =
            boost::make_local_shared<String>(icu::UnicodeString::fromUTF32(codePoints.data(), codePoints.size()));
    });
    initSystemCall(SystemCall::kStringFromCodeUnits, [](const auto& input, auto* result) {
        const auto& valueList = dynamic_cast<const ValueList&>(input.getObject(-1));
        std::vector<char16_t> uchars(valueList.items.size());
        auto ucharsIter = uchars.begin();
        for (const auto& value : valueList.items) {
            *ucharsIter = static_cast<char16_t>(value.getInt32());
            ucharsIter++;
        }
        icu::UnicodeString ustr{ uchars.data(), static_cast<int32_t>(uchars.size()) };
        result->returnedObject = boost::make_local_shared<String>(std::move(ustr));
    });
    initSystemCall(SystemCall::kStringConcat, [](const auto& input, auto* result) {
        const auto& lhs = dynamic_cast<const String&>(input.getObject(-2));
        const auto& rhs = dynamic_cast<const String&>(input.getObject(-1));
        result->returnedObject = boost::make_local_shared<String>(lhs.toUtf8() + rhs.toUtf8());
    });
    initSystemCall(SystemCall::kStringEquals, [](const auto& input, auto* result) {
        const auto& lhs = dynamic_cast<const String&>(input.getObject(-2));
        const auto& rhs = dynamic_cast<const String&>(input.getObject(-1));
        result->returnedValue.setBoolean(lhs.equals(rhs));
    });
    initSystemCall(SystemCall::kStringLen, systemCallLen);
    initSystemCall(SystemCall::kTan, [](const auto& input, auto* result) {
        result->returnedValue.setDouble(std::tan(input.getValue(-1).getDouble()));
    });
    initSystemCall(SystemCall::kTimeSpanToString, [](const auto& input, auto* result) {
        result->returnedObject = timeSpanToString(input.getValue(-1));
    });
    initSystemCall(SystemCall::kTimeZoneFromName, systemCallTimeZoneFromName);
    initSystemCall(SystemCall::kTimeZoneToString, [](const auto& input, auto* result) {
        const auto& timeZone = dynamic_cast<const TimeZone&>(input.getObject(-1));
        icu::UnicodeString name{};
        timeZone.zone->getDisplayName(name);
        result->returnedObject = boost::make_local_shared<String>(std::move(name));
    });
    initSystemCall(SystemCall::kTotalDays, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num / U_MILLIS_PER_DAY;
    });
    initSystemCall(SystemCall::kTotalHours, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num / U_MILLIS_PER_HOUR;
    });
    initSystemCall(SystemCall::kTotalMilliseconds, [](const auto& input, auto* result) {
        result->returnedValue = input.getValue(-1);  // already in milliseconds!
    });
    initSystemCall(SystemCall::kTotalMinutes, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num / U_MILLIS_PER_MINUTE;
    });
    initSystemCall(SystemCall::kTotalSeconds, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num / U_MILLIS_PER_SECOND;
    });
    initSystemCall(SystemCall::kTrunc, [](const auto& input, auto* result) {
        result->returnedValue.num = input.getValue(-1).num.trunc();
    });
    initSystemCall(SystemCall::kUtcOffset, systemCallUtcOffset);
    initSystemCall(SystemCall::kValueO, systemCallValueO);
    initSystemCall(SystemCall::kValueListGet, systemCallValueListGet);
    initSystemCall(SystemCall::kValueOptionalNewMissing, [](const auto& /*input*/, auto* result) {
        result->returnedObject = boost::make_local_shared<ValueOptional>();
    });
    initSystemCall(SystemCall::kValueOptionalNewPresent, [](const auto& input, auto* result) {
        result->returnedObject = boost::make_local_shared<ValueOptional>(input.getValue(-1));
    });
    initSystemCall(SystemCall::kValueToObjectMapNew, [](const auto& /*input*/, auto* result) {
        result->returnedObject = boost::make_local_shared<ValueToObjectMap>();
    });
    initSystemCall(SystemCall::kValueToValueMapNew, [](const auto& /*input*/, auto* result) {
        result->returnedObject = boost::make_local_shared<ValueToValueMap>();
    });
    initSystemCall(SystemCall::kValueV, systemCallValueV);
    initSystemCall(SystemCall::kWriteFileLines, [](const auto& input, auto* result) {
        const auto& filePath = dynamic_cast<const String&>(input.getObject(-2)).toUtf8();
        const auto& lines = dynamic_cast<const ObjectList&>(input.getObject(-1));
        std::ofstream stream{ filePath };
        if (stream.fail()) {
            throwFileError(errno, filePath);
        }
        for (const auto& line : lines.items) {
            stream << dynamic_cast<const String&>(*line).toUtf8() << kNewLine;
            if (stream.fail()) {
                throwFileError(errno, filePath);
            }
        }
    });
    initSystemCall(SystemCall::kWriteFileText, [](const auto& input, auto* result) {
        const auto& filePath = dynamic_cast<const String&>(input.getObject(-2)).toUtf8();
        const auto& text = dynamic_cast<const String&>(input.getObject(-1));
        std::ofstream stream{ filePath };
        if (stream.fail()) {
            throwFileError(errno, filePath);
        }
        stream << text.toUtf8();
        if (stream.fail()) {
            throwFileError(errno, filePath);
        }
    });

    _systemCallsInitialized = true;
}

SystemCallResult systemCall(SystemCall which, const SystemCallInput& input) {
    SystemCallResult result;

    try {
        auto index = static_cast<size_t>(which);
        _systemCalls.at(index)(input, &result);
    } catch (Error& ex) {
        result.hasError = true;
        result.errorCode = static_cast<int>(ex.code);
        result.errorMessage = ex.what();
    } catch (std::exception& ex) {
        result.hasError = true;
        result.errorCode = -1;
        result.errorMessage = ex.what();
    }

    return result;
}

}  // namespace vm
