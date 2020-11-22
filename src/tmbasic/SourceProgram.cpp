#include "SourceProgram.h"

namespace tmbasic {

SourceMember::SourceMember(SourceMemberType memberType, std::string source, int selectionStart, int selectionEnd)
    : memberType(memberType), source(source), selectionStart(selectionStart), selectionEnd(selectionEnd) {
    updateDisplayName();
}

static bool isBlankOrComment(std::string str) {
    for (auto ch : str) {
        if (ch == '\'') {
            return true;
        } else if (ch != ' ' && ch != '\t') {
            return false;
        }
    }
    return true;
}

void SourceMember::updateDisplayName() {
    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line)) {
        if (!isBlankOrComment(line)) {
            displayName = line;
            return;
        }
    }
    displayName = "Untitled";
}

static std::vector<const SourceMember*> sortMembers(const SourceProgram* program) {
    std::vector<const SourceMember*> sortedMembers;
    for (auto& x : program->members) {
        sortedMembers.push_back(x.get());
    }
    std::sort(sortedMembers.begin(), sortedMembers.end(), [](const auto* a, const auto* b) -> bool {
        if (a->memberType == b->memberType) {
            return a->displayName > b->displayName;
        } else {
            return a->memberType > b->memberType;
        }
    });
    return sortedMembers;
}

static std::string removeLeadingAndTrailingNonCodeLines(std::list<std::string>* linesPtr) {
    auto& lines = *linesPtr;
    auto whitespaceRegex = std::regex("^[ \t]*$");
    auto disabledRegex = std::regex("^#disabled");
    auto endDisabledRegex = std::regex("^#end");

    // remove leading lines
    /*while (lines.size() > 0 &&
           (std::regex_match(lines.front(), whitespaceRegex) || std::regex_match(lines.front(), disabledRegex))) {
        lines.erase(lines.begin());
    }

    // remove trailing lines
    while (lines.size() > 0 &&
           (std::regex_match(lines.back(), whitespaceRegex) || std::regex_match(lines.back(), endDisabledRegex))) {
        lines.erase(--lines.end());
    }*/

    // join into a string
    std::ostringstream s;
    for (auto& line : lines) {
        s << line << "\n";
    }
    return s.str();
}

void SourceProgram::load(const std::string& filePath) {
    members.clear();

    std::ifstream file(filePath);
    if (!file) {
        throw std::system_error(errno, std::system_category());
    }

    auto endSubRegex = std::regex("^[ \t]*[Ee][Nn][Dd][ \t]+[Ss][Uu][Bb]");
    auto endFunctionRegex = std::regex("^[ \t]*[Ee][Nn][Dd][ \t]+[Ff][Uu][Nn][Cc][Tt][Ii][Oo][Nn]");
    auto dimRegex = std::regex("^[ \t]*[Dd][Ii][Mm][ \t]");
    auto constRegex = std::regex("^[ \t]*[Cc][Oo][Nn][Ss][Tt][ \t]");
    auto endTypeRegex = std::regex("^[ \t]*[Ee][Nn][Dd][ \t]+[Tt][Yy][Pp][Ee]");
    auto disabledRegex = std::regex("^#disabled");
    auto endDisabledConstRegex = std::regex("^#end disabled const");
    auto endDisabledDimRegex = std::regex("^#end disabled dim");
    auto endDisabledProcedureRegex = std::regex("^#end disabled procedure");
    auto endDisabledTypeRegex = std::regex("^#end disabled type");

    std::list<std::string> currentBlock;
    auto isDisabledBlock = false;
    std::string line;
    while (std::getline(file, line)) {
        currentBlock.push_back(line);

        auto isBlockDone = false;
        SourceMemberType memberType;  // set this when setting isBlockDone=true

        if (isDisabledBlock) {
            if (std::regex_match(line, endDisabledConstRegex)) {
                memberType = SourceMemberType::kConstant;
                isBlockDone = true;
            } else if (std::regex_match(line, endDisabledDimRegex)) {
                memberType = SourceMemberType::kGlobalVariable;
                isBlockDone = true;
            } else if (std::regex_match(line, endDisabledProcedureRegex)) {
                memberType = SourceMemberType::kProcedure;
                isBlockDone = true;
            } else if (std::regex_match(line, endDisabledTypeRegex)) {
                memberType = SourceMemberType::kType;
                isBlockDone = true;
            }
        } else if (std::regex_match(line, disabledRegex)) {
            isDisabledBlock = true;
        } else if (std::regex_match(line, endSubRegex) || std::regex_match(line, endFunctionRegex)) {
            memberType = SourceMemberType::kProcedure;
            isBlockDone = true;
        } else if (std::regex_match(line, dimRegex)) {
            memberType = SourceMemberType::kGlobalVariable;
            isBlockDone = true;
        } else if (std::regex_match(line, constRegex)) {
            memberType = SourceMemberType::kConstant;
            isBlockDone = true;
        } else if (std::regex_match(line, endTypeRegex)) {
            memberType = SourceMemberType::kType;
            isBlockDone = true;
        }

        if (isBlockDone) {
            members.push_back(
                std::make_unique<SourceMember>(memberType, removeLeadingAndTrailingNonCodeLines(&currentBlock), 0, 0));
            currentBlock = {};
            isDisabledBlock = false;
        }
    }
}

void SourceProgram::save(const std::string& filePath) const {
    auto stream = std::ofstream(filePath);

    for (auto* member : sortMembers(this)) {
        auto trimmedSource = boost::trim_copy(member->source);
        if (member->isCompiledMemberUpToDate && member->compiledMember) {
            // this code was compiled successfully so we know it's valid
            stream << trimmedSource << "\n\n";
        } else {
            // since we don't know that this is valid, use a #disabled block so this file will parse again in the face
            // of syntax errors in this block
            std::string typeName;
            switch (member->memberType) {
                case SourceMemberType::kConstant:
                    typeName = "const";
                    break;
                case SourceMemberType::kGlobalVariable:
                    typeName = "dim";
                    break;
                case SourceMemberType::kProcedure:
                    typeName = "procedure";
                    break;
                case SourceMemberType::kType:
                    typeName = "type";
                    break;
                default:
                    assert(false);
            }

            stream << "#disabled " << typeName;
            boost::replace_all(trimmedSource, "#", "##");  // escape # symbol
            stream << "\n"
                   << trimmedSource << "\n"
                   << "#end disabled " << typeName << "\n\n";
        }
    }
}

}  // namespace tmbasic
