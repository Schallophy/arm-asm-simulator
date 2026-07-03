#include <algorithm>
#include <array>
#include <cctype>
#include <clocale>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <unistd.h>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
#include "i18n.h"

#include <ncurses.h>

namespace {

enum class Cond {
    AL,
    EQ,
    NE,
    GT,
    LT,
    GE,
    LE,
    HI,
    LS,
    CS,
    CC,
    MI,
    PL,
    VS,
    VC,
};

enum class CompileErrorKind {
    Generic,
    DuplicateLabel,
    UnsupportedOpcode,
    OperandCount,
    InvalidRegister,
    InvalidMemoryOperand,
    UndefinedLabel,
};

struct Flags {
    bool n = false;
    bool z = false;
    bool c = false;
    bool v = false;
};

struct MemoryOperand {
    int baseReg = -1;
    long long offset = 0;
    bool writeBack = false;
};

struct Instruction {
    int lineNo = 0;
    std::string original;
    std::string op;
    std::vector<std::string> operands;
    Cond cond = Cond::AL;
    bool setFlags = false;
};

struct CompiledSource {
    std::vector<Instruction> instructions;
    std::unordered_map<std::string, int> codeLabels;
    std::unordered_map<std::string, long long> symbolAddresses;
    std::unordered_map<long long, long long> dataMemory;
};

std::string trim(const std::string &text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string upper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return text;
}

std::vector<std::string> splitOperands(const std::string &text) {
    std::vector<std::string> result;
    std::string current;
    int bracketDepth = 0;
    for (char ch : text) {
        if (ch == '[' || ch == '{') {
            ++bracketDepth;
            current.push_back(ch);
            continue;
        }
        if (ch == ']' || ch == '}') {
            --bracketDepth;
            current.push_back(ch);
            continue;
        }
        if (ch == ',' && bracketDepth == 0) {
            result.push_back(trim(current));
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!trim(current).empty()) {
        result.push_back(trim(current));
    }
    return result;
}

std::optional<long long> parseNumber(const std::string &text) {
    std::string value = trim(text);
    if (value.empty()) {
        return std::nullopt;
    }
    if (value[0] == '#') {
        value = value.substr(1);
    }
    bool negative = false;
    if (!value.empty() && value[0] == '-') {
        negative = true;
        value = value.substr(1);
    }
    int base = 10;
    if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        base = 16;
        value = value.substr(2);
    }
    if (value.empty()) {
        return std::nullopt;
    }
    try {
        long long parsed = std::stoll(value, nullptr, base);
        return negative ? -parsed : parsed;
    } catch (...) {
        return std::nullopt;
    }
}

int registerIndex(const std::string &name) {
    const std::string reg = upper(trim(name));
    if (reg == "SP") return 13;
    if (reg == "LR") return 14;
    if (reg == "PC") return 15;
    if (reg.size() >= 2 && reg[0] == 'R') {
        try {
            const int index = std::stoi(reg.substr(1));
            if (index >= 0 && index <= 12) {
                return index;
            }
        } catch (...) {
            return -1;
        }
    }
    return -1;
}

std::string registerName(int index) {
    if (index >= 0 && index <= 12) return "R" + std::to_string(index);
    if (index == 13) return "SP";
    if (index == 14) return "LR";
    if (index == 15) return "PC";
    return "?";
}

bool isSupportedOpcode(const std::string &op);

// Parse UAL opcode: op{cond}{S}. Returns true if a known opcode is found.
bool parseOpcode(const std::string &token, std::string &op, Cond &cond, bool &setFlags) {
    static const std::array<std::pair<const char *, Cond>, 14> condSuffixes = {{
        {"EQ", Cond::EQ}, {"NE", Cond::NE}, {"GT", Cond::GT}, {"LT", Cond::LT}, {"GE", Cond::GE},
        {"LE", Cond::LE}, {"HI", Cond::HI}, {"LS", Cond::LS}, {"CS", Cond::CS}, {"CC", Cond::CC},
        {"MI", Cond::MI}, {"PL", Cond::PL}, {"VS", Cond::VS}, {"VC", Cond::VC},
    }};

    std::string t = upper(token);
    setFlags = false;

    auto findCond = [&](const std::string &s, Cond &outCond, std::string &outBase) -> bool {
        for (const auto &[suffix, c] : condSuffixes) {
            std::size_t slen = std::char_traits<char>::length(suffix);
            if (s.size() > slen && s.substr(s.size() - slen) == suffix) {
                outCond = c;
                outBase = s.substr(0, s.size() - slen);
                return true;
            }
        }
        return false;
    };

    // UAL: op{cond}{S}
    if (t.size() > 1 && t.back() == 'S') {
        std::string withoutS = t.substr(0, t.size() - 1);
        std::string base;
        Cond c;
        if (findCond(withoutS, c, base) && isSupportedOpcode(base)) {
            op = upper(base);
            cond = c;
            setFlags = true;
            return true;
        }
    }

    // op{cond} (no S)
    {
        std::string base;
        Cond c;
        if (findCond(t, c, base) && isSupportedOpcode(base)) {
            op = upper(base);
            cond = c;
            setFlags = false;
            return true;
        }
    }

    // Legacy: op{S}{cond}
    {
        std::string base;
        Cond c;
        if (findCond(t, c, base)) {
            if (!base.empty() && base.back() == 'S') {
                std::string opBase = base.substr(0, base.size() - 1);
                if (isSupportedOpcode(opBase)) {
                    op = upper(opBase);
                    cond = c;
                    setFlags = true;
                    return true;
                }
            }
        }
    }

    // op{S} (no cond)
    if (t.size() > 1 && t.back() == 'S') {
        std::string opBase = t.substr(0, t.size() - 1);
        if (isSupportedOpcode(opBase)) {
            op = upper(opBase);
            cond = Cond::AL;
            setFlags = true;
            return true;
        }
    }

    // Plain opcode
    if (isSupportedOpcode(t)) {
        op = upper(t);
        cond = Cond::AL;
        return true;
    }

    return false;
}

bool isDirectiveToken(const std::string &token) {
    static const std::unordered_set<std::string> directives = {
        "AREA", "ENTRY", "EXPORT", "START", "END", "EQU", "ORG", "ALIGN", "LTORG",
        "CODE", "DATA", "THUMB", "ARM"
    };
    return directives.count(upper(token)) > 0;
}

bool isDataDirective(const std::string &token) {
    const std::string directive = upper(token);
    return directive == "DCD" || directive == "DCW" || directive == "DCB" || directive == "SPACE";
}

std::vector<int> parseRegisterList(const std::string &text) {
    std::vector<int> regs;
    std::string inner = trim(text);
    if (inner.size() < 2 || inner.front() != '{' || inner.back() != '}') {
        return regs;
    }
    inner = inner.substr(1, inner.size() - 2);
    const auto pieces = splitOperands(inner);
    for (const auto &piece : pieces) {
        const auto dashPos = piece.find('-');
        if (dashPos != std::string::npos) {
            int first = registerIndex(trim(piece.substr(0, dashPos)));
            int last = registerIndex(trim(piece.substr(dashPos + 1)));
            if (first >= 0 && last >= 0 && first <= last) {
                for (int r = first; r <= last; ++r) {
                    regs.push_back(r);
                }
            }
        } else {
            int r = registerIndex(trim(piece));
            if (r >= 0) {
                regs.push_back(r);
            }
        }
    }
    return regs;
}

bool isSupportedOpcode(const std::string &op) {
    static const std::unordered_set<std::string> supported = {
        "MOV", "MVN", "ADD", "SUB", "RSB", "RSC", "ADC", "SBC",
        "AND", "ORR", "EOR", "BIC",
        "MUL", "MLA",
        "CMP", "CMN", "TST", "TEQ",
        "LDR", "STR",
        "B", "BL", "BX",
        "NOP",
        "LDM", "LDMIA", "LDMIB", "LDMDA", "LDMDB", "LDMFD", "LDMED", "LDMEA", "LDMFA",
        "STM", "STMIA", "STMIB", "STMDA", "STMDB", "STMFD", "STMED", "STMEA", "STMFA",
        "PUSH", "POP"
    };
    return supported.count(upper(op)) > 0;
}

bool isSupportedInstructionToken(const std::string &token) {
    std::string op;
    Cond cond;
    bool setFlags;
    return parseOpcode(token, op, cond, setFlags);
}

bool isConditionSatisfied(Cond cond, const Flags &flags) {
    switch (cond) {
    case Cond::AL: return true;
    case Cond::EQ: return flags.z;
    case Cond::NE: return !flags.z;
    case Cond::GT: return !flags.z && (flags.n == flags.v);
    case Cond::LT: return flags.n != flags.v;
    case Cond::GE: return flags.n == flags.v;
    case Cond::LE: return flags.z || (flags.n != flags.v);
    case Cond::HI: return flags.c && !flags.z;
    case Cond::LS: return !flags.c || flags.z;
    case Cond::CS: return flags.c;
    case Cond::CC: return !flags.c;
    case Cond::MI: return flags.n;
    case Cond::PL: return !flags.n;
    case Cond::VS: return flags.v;
    case Cond::VC: return !flags.v;
    }
    return false;
}

std::string compileErrorCode(CompileErrorKind kind) {
    switch (kind) {
    case CompileErrorKind::DuplicateLabel: return "A2002E";
    case CompileErrorKind::UnsupportedOpcode: return "A2003E";
    case CompileErrorKind::OperandCount: return "A2004E";
    case CompileErrorKind::InvalidRegister: return "A2005E";
    case CompileErrorKind::InvalidMemoryOperand: return "A2006E";
    case CompileErrorKind::UndefinedLabel: return "A2007E";
    case CompileErrorKind::Generic:
    default:
        return "A2000E";
    }
}

class Simulator {
public:
    bool loadSource(const std::string &source, const std::string &sourceName) {
        sourceName_ = sourceName;
        compileErrors_.clear();
        compiled_ = CompiledSource{};
        sourceLines_.clear();
        instructionLines_.clear();
        {
            std::istringstream splitter(source);
            std::string sl;
            while (std::getline(splitter, sl)) {
                sourceLines_.push_back(sl);
            }
        }

        std::istringstream input(source);
        std::string line;
        int lineNo = 0;
        long long dataAddress = 0;
        std::vector<std::pair<std::string, int>> pendingLabels;

        auto addCompileError = [&](int errorLine, CompileErrorKind kind, const std::string &detail) {
            const std::string prefix = errorLine > 0 ? sourceName_ + "(" + std::to_string(errorLine) + ")" : sourceName_;
            compileErrors_.push_back(prefix + ": Error: " + compileErrorCode(kind) + ": " + detail);
        };

        auto defineCodeLabel = [&](const std::string &label, int labelLine) {
            const std::string normalized = upper(label);
            if (compiled_.codeLabels.count(normalized) > 0 || compiled_.symbolAddresses.count(normalized) > 0) {
                addCompileError(labelLine, CompileErrorKind::DuplicateLabel, STR_DUPLICATE_LABEL + label + "'");
                return;
            }
            compiled_.codeLabels[normalized] = static_cast<int>(compiled_.instructions.size());
            compiled_.symbolAddresses[normalized] = static_cast<long long>(compiled_.instructions.size()) * 4;
            codeLabelLines_[normalized] = labelLine;
        };

        auto defineDataLabel = [&](const std::string &label, int labelLine) {
            const std::string normalized = upper(label);
            if (compiled_.codeLabels.count(normalized) > 0 || compiled_.symbolAddresses.count(normalized) > 0) {
                addCompileError(labelLine, CompileErrorKind::DuplicateLabel, STR_DUPLICATE_LABEL + label + "'");
                return;
            }
            compiled_.symbolAddresses[normalized] = dataAddress;
            dataLabelLines_[normalized] = labelLine;
        };

        auto flushPendingLabels = [&](bool dataSection, int) {
            for (const auto &entry : pendingLabels) {
                const auto &label = entry.first;
                if (dataSection) {
                    defineDataLabel(label, entry.second);
                } else {
                    defineCodeLabel(label, entry.second);
                }
            }
            pendingLabels.clear();
        };

        auto parseInstructionLine = [&](const std::string &text, int targetLine) {
            instructionLines_.insert(targetLine);
            std::istringstream parser(text);
            std::string opToken;
            parser >> opToken;
            if (opToken.empty()) {
                return;
            }

            Instruction inst;
            inst.lineNo = targetLine;
            inst.original = trim(text);

            if (!parseOpcode(opToken, inst.op, inst.cond, inst.setFlags)) {
                inst.op = upper(opToken);
                inst.cond = Cond::AL;
                inst.setFlags = false;
            }

            std::string remaining;
            std::getline(parser, remaining);
            inst.operands = splitOperands(remaining);
            compiled_.instructions.push_back(std::move(inst));
        };

        auto processDataDirective = [&](const std::string &directive, const std::string &operandText, int targetLine) {
            flushPendingLabels(true, targetLine);
            const std::string normalized = upper(directive);
            const auto operands = splitOperands(operandText);

            if (normalized == "SPACE") {
                long long size = 0;
                if (!operands.empty()) {
                    const auto value = parseNumber(operands[0]);
                    if (!value.has_value()) {
                        addCompileError(targetLine, CompileErrorKind::InvalidMemoryOperand, STR_SPACE_SIZE_INVALID + operands[0]);
                        return;
                    }
                    size = *value;
                }
                dataAddress += std::max<long long>(0, size);
                return;
            }

            const long long step = normalized == "DCB" ? 1 : normalized == "DCW" ? 2 : 4;
            for (const auto &operand : operands) {
                const auto value = parseNumber(operand);
                if (!value.has_value()) {
                    addCompileError(targetLine, CompileErrorKind::InvalidMemoryOperand, normalized + STR_CONST_INVALID + operand);
                    continue;
                }
                compiled_.dataMemory[dataAddress] = *value;
                dataAddress += step;
            }
        };

        while (std::getline(input, line)) {
            ++lineNo;
            auto commentPos = line.find_first_of(";@");
            std::string cleaned = commentPos == std::string::npos ? line : line.substr(0, commentPos);
            cleaned = trim(cleaned);
            if (cleaned.empty()) {
                continue;
            }

            while (true) {
                const auto colonPos = cleaned.find(':');
                if (colonPos == std::string::npos) {
                    break;
                }
                const std::string label = trim(cleaned.substr(0, colonPos));
                if (!label.empty()) {
                    pendingLabels.push_back({label, lineNo});
                }
                cleaned = trim(cleaned.substr(colonPos + 1));
                if (cleaned.empty()) {
                    break;
                }
            }

            if (cleaned.empty()) {
                continue;
            }

            std::istringstream parser(cleaned);
            std::string firstToken;
            parser >> firstToken;
            if (firstToken.empty()) {
                continue;
            }

            std::string remaining;
            std::getline(parser, remaining);
            const std::string trimmedRemaining = trim(remaining);

            if (isDataDirective(firstToken)) {
                flushPendingLabels(true, lineNo);
                processDataDirective(firstToken, trimmedRemaining, lineNo);
                continue;
            }

            if (!trimmedRemaining.empty()) {
                std::istringstream nextParser(trimmedRemaining);
                std::string nextToken;
                nextParser >> nextToken;
                if (isDataDirective(nextToken)) {
                    pendingLabels.push_back({firstToken, lineNo});
                    flushPendingLabels(true, lineNo);
                    const std::size_t directivePos = trimmedRemaining.find(nextToken);
                    const std::string directiveOperands = directivePos == std::string::npos ? std::string() : trim(trimmedRemaining.substr(directivePos + nextToken.size()));
                    processDataDirective(nextToken, directiveOperands, lineNo);
                    continue;
                }
                if (isSupportedInstructionToken(nextToken)) {
                    pendingLabels.push_back({firstToken, lineNo});
                    flushPendingLabels(false, lineNo);
                    parseInstructionLine(trimmedRemaining, lineNo);
                    continue;
                }
            }

            if (isSupportedInstructionToken(firstToken)) {
                flushPendingLabels(false, lineNo);
                parseInstructionLine(cleaned, lineNo);
                continue;
            }

            if (isDirectiveToken(firstToken)) {
                if (upper(firstToken) == "AREA") {
                    // Skip AREA directives - they are section markers, not labels
                    continue;
                }
                pendingLabels.push_back({firstToken, lineNo});
                continue;
            }

            pendingLabels.push_back({firstToken, lineNo});
        }

        if (!pendingLabels.empty()) {
            flushPendingLabels(false, lineNo);
        }

        hasExecutableInstructions_ = !compiled_.instructions.empty();
        validateProgram();
        if (!compileErrors_.empty()) {
            compiled_ = CompiledSource{};
            hasExecutableInstructions_ = false;
            return false;
        }

        reset();
        return true;
    }

    const std::vector<std::string> &compileErrors() const {
        return compileErrors_;
    }

    bool hasExecutableInstructions() const {
        return hasExecutableInstructions_;
    }

    bool finished() const {
        return finished_ || pc_ >= static_cast<int>(compiled_.instructions.size());
    }

    void reset() {
        registers_.fill(0);
        flags_ = {};
        memory_ = compiled_.dataMemory;
        pc_ = 0;
        finished_ = false;
        touchedRegisters_.clear();
        pendingLabelLine_ = -1;
    }

    void run() {
        while (!finished()) {
            step(true);
        }
    }

    void step(bool verbose = true) {
        if (finished()) {
            if (verbose) {
                std::cout << STR_PROGRAM_FINISHED;
            }
            return;
        }

        pendingLabelLine_ = -1;

        const Instruction &inst = compiled_.instructions[pc_];
        const int currentPc = pc_;
        std::set<int> usedRegisters;
        std::set<int> writtenRegisters;
        bool branched = false;

        if (verbose) {
            std::cout << "\n[Step] " << (currentPc + 1) << " | line " << inst.lineNo << " | " << inst.original << "\n";
        }

        auto readValue = [&](const std::string &token) -> long long {
            const int reg = registerIndex(token);
            if (reg >= 0) {
                usedRegisters.insert(reg);
                if (reg == 15) return static_cast<long long>(pc_);
                return registers_[reg];
            }
            const auto number = parseNumber(token);
            return number.has_value() ? *number : 0;
        };

        auto alu32 = [](long long v) -> long long {
            return static_cast<long long>(static_cast<int>(static_cast<unsigned>(v & 0xFFFFFFFF)));
        };

        auto writeValue = [&](const std::string &token, long long value) {
            const int reg = registerIndex(token);
            if (reg >= 0) {
                writtenRegisters.insert(reg);
                touchedRegisters_.insert(reg);
                if (reg == 15) {
                    pc_ = static_cast<int>(value);
                    branched = true;
                    return;
                }
                registers_[reg] = alu32(value);
            }
        };

        auto updateNZ = [&](long long value) {
            flags_.z = (value == 0);
            flags_.n = (value < 0);
        };

        auto parseMemoryOperand = [&](const std::string &token) -> std::optional<MemoryOperand> {
            std::string text = trim(token);
            if (text.size() < 3 || text.front() != '[') {
                return std::nullopt;
            }
            bool writeBack = false;
            if (text.back() == '!') {
                writeBack = true;
                text.pop_back();
                text = trim(text);
            }
            if (text.size() < 2 || text.back() != ']') {
                return std::nullopt;
            }
            text = trim(text.substr(1, text.size() - 2));
            const auto pieces = splitOperands(text);
            if (pieces.empty()) {
                return std::nullopt;
            }
            MemoryOperand operand;
            operand.baseReg = registerIndex(pieces[0]);
            operand.writeBack = writeBack;
            if (operand.baseReg < 0) {
                return std::nullopt;
            }
            usedRegisters.insert(operand.baseReg);
            if (pieces.size() >= 2) {
                const auto offset = parseNumber(pieces[1]);
                if (offset.has_value()) {
                    operand.offset = *offset;
                } else {
                    const int offsetReg = registerIndex(pieces[1]);
                    if (offsetReg < 0) {
                        return std::nullopt;
                    }
                    operand.offset = offsetReg == 15 ? static_cast<long long>(pc_) : registers_[offsetReg];
                    usedRegisters.insert(offsetReg);
                }
            }
            return operand;
        };

        auto resolveSymbolAddress = [&](const std::string &symbol) -> std::optional<long long> {
            const auto number = parseNumber(symbol);
            if (number.has_value()) {
                return number;
            }
            const auto it = compiled_.symbolAddresses.find(upper(trim(symbol)));
            if (it == compiled_.symbolAddresses.end()) {
                return std::nullopt;
            }
            return it->second;
        };

        auto updateALUFlags = [&](long long left, long long right, long long result, const std::string &opcode) {
            unsigned ul = static_cast<unsigned>(left & 0xFFFFFFFF);
            unsigned ur = static_cast<unsigned>(right & 0xFFFFFFFF);
            unsigned ures = static_cast<unsigned>(result & 0xFFFFFFFF);
            int sl = static_cast<int>(ul);
            int sr = static_cast<int>(ur);
            int sres = static_cast<int>(ures);
            flags_.n = sres < 0;
            flags_.z = (ures == 0);
            if (opcode == "ADD" || opcode == "ADC" || opcode == "CMN") {
                flags_.c = (static_cast<unsigned long long>(ul) + ur) > 0xFFFFFFFFULL;
                flags_.v = ((~(sl ^ sr)) & (sl ^ sres)) < 0;
            } else if (opcode == "SUB" || opcode == "SBC" || opcode == "CMP") {
                flags_.c = static_cast<unsigned long long>(ul) >= static_cast<unsigned long long>(ur);
                flags_.v = ((sl ^ sr) & (sl ^ sres)) < 0;
            } else if (opcode == "RSB" || opcode == "RSC") {
                flags_.c = static_cast<unsigned long long>(ur) >= static_cast<unsigned long long>(ul);
                flags_.v = ((sl ^ sr) & (sr ^ sres)) < 0;
            } else {
                flags_.c = false;
                flags_.v = false;
            }
        };

        auto branchTo = [&](const std::string &label) -> bool {
            const std::string normalized = upper(trim(label));
            const auto it = compiled_.codeLabels.find(normalized);
            if (it == compiled_.codeLabels.end()) {
                return false;
            }
            pc_ = it->second;
            branched = true;
            if (codeLabelLines_.count(normalized)) {
                pendingLabelLine_ = codeLabelLines_[normalized];
            }
            return true;
        };

        if (!isConditionSatisfied(inst.cond, flags_)) {
            if (verbose) {
                std::cout << STR_COND_SKIP;
                printRegisterUsage(usedRegisters, writtenRegisters);
                printState();
            }
            ++pc_;
            return;
        }

        const std::string op = inst.op;
        if (op == "MOV" || op == "MVN") {
            if (inst.operands.size() >= 2) {
                const long long source = readValue(inst.operands[1]);
                const long long result = op == "MOV" ? source : (~source & 0xFFFFFFFF);
                writeValue(inst.operands[0], result);
                if (inst.setFlags) {
                    updateNZ(alu32(result));
                }
            }
        } else if (op == "ADD" || op == "SUB" || op == "RSB" || op == "ADC" || op == "SBC" || op == "RSC") {
            if (inst.operands.size() >= 3) {
                const long long left = readValue(inst.operands[1]);
                const long long right = readValue(inst.operands[2]);
                long long result = 0;
                if (op == "ADD") result = left + right;
                else if (op == "SUB") result = left - right;
                else if (op == "RSB") result = right - left;
                else if (op == "ADC") result = left + right + (flags_.c ? 1 : 0);
                else if (op == "SBC") result = left - right - (flags_.c ? 0 : 1);
                else if (op == "RSC") result = right - left - (flags_.c ? 0 : 1);
                long long masked = alu32(result);
                writeValue(inst.operands[0], masked);
                if (inst.setFlags) {
                    updateALUFlags(left, right, result, op);
                }
            }
        } else if (op == "AND" || op == "ORR" || op == "EOR" || op == "BIC") {
            if (inst.operands.size() >= 3) {
                const long long left = readValue(inst.operands[1]);
                const long long right = readValue(inst.operands[2]);
                long long result = 0;
                if (op == "AND") result = left & right;
                else if (op == "ORR") result = left | right;
                else if (op == "EOR") result = left ^ right;
                else if (op == "BIC") result = left & ~right;
                long long masked = alu32(result);
                writeValue(inst.operands[0], masked);
                if (inst.setFlags) {
                    updateNZ(masked);
                }
            }
        } else if (op == "CMP") {
            if (inst.operands.size() >= 2) {
                const long long left = readValue(inst.operands[0]);
                const long long right = readValue(inst.operands[1]);
                const long long result = left - right;
                updateALUFlags(left, right, result, "CMP");
            }
        } else if (op == "CMN") {
            if (inst.operands.size() >= 2) {
                const long long left = readValue(inst.operands[0]);
                const long long right = readValue(inst.operands[1]);
                const long long result = left + right;
                updateALUFlags(left, right, result, "CMN");
            }
        } else if (op == "TST") {
            if (inst.operands.size() >= 2) {
                const long long left = readValue(inst.operands[0]);
                const long long right = readValue(inst.operands[1]);
                const long long result = left & right;
                updateNZ(alu32(result));
            }
        } else if (op == "TEQ") {
            if (inst.operands.size() >= 2) {
                const long long left = readValue(inst.operands[0]);
                const long long right = readValue(inst.operands[1]);
                const long long result = left ^ right;
                updateNZ(alu32(result));
            }
        } else if (op == "MUL") {
            if (inst.operands.size() >= 3) {
                const long long rm = readValue(inst.operands[1]);
                const long long rs = readValue(inst.operands[2]);
                const long long result = rm * rs;
                long long masked = alu32(result);
                writeValue(inst.operands[0], masked);
                if (inst.setFlags) {
                    updateNZ(masked);
                }
            }
        } else if (op == "MLA") {
            if (inst.operands.size() >= 4) {
                const long long rm = readValue(inst.operands[1]);
                const long long rs = readValue(inst.operands[2]);
                const long long ra = readValue(inst.operands[3]);
                const long long result = (rm * rs) + ra;
                long long masked = alu32(result);
                writeValue(inst.operands[0], masked);
                if (inst.setFlags) {
                    updateNZ(masked);
                }
            }
        } else if (op == "NOP") {
            // do nothing
        }

        auto writeReg = [&](int reg, long long value) {
            if (reg == 15) {
                pc_ = static_cast<int>(value);
                branched = true;
            } else {
                registers_[reg] = alu32(value);
                writtenRegisters.insert(reg);
                touchedRegisters_.insert(reg);
            }
        };

        auto addToReg = [&](int reg, long long delta) {
            if (reg == 15) {
                pc_ = static_cast<int>(static_cast<long long>(pc_) + delta);
                branched = true;
            } else {
                registers_[reg] = alu32(registers_[reg] + delta);
                writtenRegisters.insert(reg);
                touchedRegisters_.insert(reg);
            }
        };

        if (op == "LDR") {
            if (inst.operands.size() >= 2) {
                const std::string &dest = inst.operands[0];
                if (!inst.operands[1].empty() && inst.operands[1][0] == '=') {
                    const auto address = resolveSymbolAddress(inst.operands[1].substr(1));
                    if (address.has_value()) {
                        writeValue(dest, *address);
                    }
                } else {
                    const auto memoryOperand = parseMemoryOperand(inst.operands[1]);
                    if (memoryOperand.has_value()) {
                        long long base = registers_[memoryOperand->baseReg];
                        if (memoryOperand->writeBack) {
                            base += memoryOperand->offset;
                            writeReg(memoryOperand->baseReg, base);
                        }
                        const long long address = base + (memoryOperand->writeBack ? 0 : memoryOperand->offset);
                        const long long value = memory_.count(address) ? memory_[address] : 0;
                        writeValue(dest, value);
                        if (!memoryOperand->writeBack && inst.operands.size() == 3) {
                            const long long postOffset = readValue(inst.operands[2]);
                            addToReg(memoryOperand->baseReg, postOffset);
                        }
                    }
                }
            }
        } else if (op == "STR") {
            if (inst.operands.size() >= 2) {
                const long long value = readValue(inst.operands[0]);
                const auto memoryOperand = parseMemoryOperand(inst.operands[1]);
                if (memoryOperand.has_value()) {
                    long long base = registers_[memoryOperand->baseReg];
                    if (memoryOperand->writeBack) {
                        base += memoryOperand->offset;
                        writeReg(memoryOperand->baseReg, base);
                    }
                    const long long address = base + (memoryOperand->writeBack ? 0 : memoryOperand->offset);
                    memory_[address] = value;
                    if (!memoryOperand->writeBack && inst.operands.size() == 3) {
                        const long long postOffset = readValue(inst.operands[2]);
                        addToReg(memoryOperand->baseReg, postOffset);
                    }
                }
            }
        } else if (op == "B" || op == "BL") {
            if (!inst.operands.empty()) {
                if (op == "BL") {
                    registers_[14] = currentPc + 1;
                    writtenRegisters.insert(14);
                    touchedRegisters_.insert(14);
                }
                branchTo(inst.operands[0]);
            }
        } else if (op == "BX") {
            if (!inst.operands.empty()) {
                const long long target = readValue(inst.operands[0]);
                pc_ = static_cast<int>(target);
                branched = true;
            }
        } else if (op == "LDM" || op == "STM" || op == "PUSH" || op == "POP"
                   || op.find("LDM") == 0 || op.find("STM") == 0) {
            std::string effOp;
            int mode = 0; // 0=IA, 1=IB, 2=DA, 3=DB
            if (op == "PUSH") { effOp = "STM"; mode = 3; }
            else if (op == "POP") { effOp = "LDM"; }
            else if (op == "LDM" || op == "LDMIA" || op == "LDMFD") effOp = "LDM";
            else if (op == "LDMIB" || op == "LDMED") { effOp = "LDM"; mode = 1; }
            else if (op == "LDMDA" || op == "LDMFA") { effOp = "LDM"; mode = 2; }
            else if (op == "LDMDB" || op == "LDMEA") { effOp = "LDM"; mode = 3; }
            else if (op == "STM" || op == "STMIA" || op == "STMEA") effOp = "STM";
            else if (op == "STMIB" || op == "STMFA") { effOp = "STM"; mode = 1; }
            else if (op == "STMDA" || op == "STMED") { effOp = "STM"; mode = 2; }
            else if (op == "STMDB" || op == "STMFD") { effOp = "STM"; mode = 3; }
            else { effOp.clear(); }

            if (!effOp.empty() && !inst.operands.empty()) {
                int baseReg = -1;
                bool writeBack = false;

                if (op == "PUSH" || op == "POP") {
                    baseReg = 13;
                    writeBack = true;
                } else {
                    std::string baseText = inst.operands[0];
                    if (!baseText.empty() && baseText.back() == '!') {
                        writeBack = true;
                        baseText.pop_back();
                        baseText = trim(baseText);
                    }
                    baseReg = registerIndex(baseText);
                }

                std::vector<int> regList = parseRegisterList(inst.operands.back());
                if (!regList.empty() && baseReg >= 0) {
                    std::sort(regList.begin(), regList.end());
                    const long long baseAddr = registers_[baseReg];
                    const int count = static_cast<int>(regList.size());
                    long long startAddr, finalAddr;

                    switch (mode) {
                    case 1: startAddr = baseAddr + 4;          finalAddr = baseAddr + 4LL * count; break;
                    case 2: startAddr = baseAddr - 4LL * (count - 1); finalAddr = baseAddr - 4LL * count; break;
                    case 3: startAddr = baseAddr - 4LL * count;       finalAddr = baseAddr - 4LL * count; break;
                    default: startAddr = baseAddr;              finalAddr = baseAddr + 4LL * count; break;
                    }

                    if (effOp == "STM") {
                        for (int i = 0; i < count; ++i)
                            memory_[startAddr + 4LL * i] = registers_[regList[i]];
                        if (writeBack) {
                            writeReg(baseReg, finalAddr);
                        }
                    } else {
                        for (int i = 0; i < count; ++i) {
                            const long long addr = startAddr + 4LL * i;
                            const long long loaded = memory_.count(addr) ? memory_[addr] : 0;
                            writeReg(regList[i], loaded);
                        }
                        if (writeBack) {
                            writeReg(baseReg, finalAddr);
                        }
                    }
                }
            }
        }

        if (!branched) {
            ++pc_;
        }

        if (verbose) {
            printRegisterUsage(usedRegisters, writtenRegisters);
            printState();
        }
    }

    // --- TUI accessors ---
    const std::vector<std::string>& getSourceLines() const { return sourceLines_; }
    const std::array<long long, 16>& getRegisters() const { return registers_; }
    const Flags& getFlags() const { return flags_; }
    const CompiledSource& getCompiled() const { return compiled_; }
    int getPC() const { return pc_; }
    int getPendingLabelLine() const { return pendingLabelLine_; }
    void clearPendingLabel() { pendingLabelLine_ = -1; }


private:
    void validateProgram() {
        for (const auto &inst : compiled_.instructions) {
            if (!isSupportedOpcode(inst.op)) {
                addCompileError(inst.lineNo, CompileErrorKind::UnsupportedOpcode, STR_UNSUPPORTED_OPCODE + inst.op + "'");
                continue;
            }

            if (inst.op == "MOV" || inst.op == "MVN") {
                if (inst.operands.size() != 2) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, STR_UNSUPPORTED_OPCODE + inst.op + STR_REQUIRES_OPERANDS + "2" + STR_OPERAND_SUFFIX);
                }
                if (!inst.operands.empty() && registerIndex(inst.operands[0]) < 0) {
                    addCompileError(inst.lineNo, CompileErrorKind::InvalidRegister, STR_UNSUPPORTED_OPCODE + inst.op + STR_DEST_REG_INVALID + inst.operands[0]);
                }
                continue;
            }

            if (inst.op == "ADD" || inst.op == "SUB" || inst.op == "RSB" || inst.op == "ADC" || inst.op == "SBC" || inst.op == "RSC" || inst.op == "AND" || inst.op == "ORR" || inst.op == "EOR" || inst.op == "BIC") {
                if (inst.operands.size() != 3) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, STR_UNSUPPORTED_OPCODE + inst.op + STR_REQUIRES_OPERANDS + "3" + STR_OPERAND_SUFFIX);
                }
                if (!inst.operands.empty() && registerIndex(inst.operands[0]) < 0) {
                    addCompileError(inst.lineNo, CompileErrorKind::InvalidRegister, STR_UNSUPPORTED_OPCODE + inst.op + STR_DEST_REG_INVALID + inst.operands[0]);
                }
                continue;
            }

            if (inst.op == "CMP" || inst.op == "CMN") {
                if (inst.operands.size() != 2) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, STR_UNSUPPORTED_OPCODE + inst.op + STR_REQUIRES_OPERANDS + "2" + STR_OPERAND_SUFFIX);
                }
                continue;
            }

            if (inst.op == "LDR" || inst.op == "STR") {
                if (inst.operands.size() != 2 && inst.operands.size() != 3) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, STR_UNSUPPORTED_OPCODE + inst.op + STR_REQUIRES_OPERANDS + "2 or 3" + STR_OPERAND_SUFFIX);
                    continue;
                }
                if (!inst.operands.empty() && registerIndex(inst.operands[0]) < 0) {
                    addCompileError(inst.lineNo, CompileErrorKind::InvalidRegister, STR_UNSUPPORTED_OPCODE + inst.op + STR_INVALID_REG + inst.operands[0]);
                }

                const std::string &operand = inst.operands[1];
                if (!operand.empty() && operand[0] == '=') {
                    if (inst.op == "STR") {
                        addCompileError(inst.lineNo, CompileErrorKind::InvalidMemoryOperand, STR_STR_NO_LITERAL + operand);
                    } else if (!parseNumber(operand.substr(1)).has_value() &&
                               compiled_.symbolAddresses.count(upper(trim(operand.substr(1)))) == 0) {
                        addCompileError(inst.lineNo, CompileErrorKind::UndefinedLabel, STR_UNDEFINED_SYMBOL + operand.substr(1));
                    }
                    continue;
                }

                std::string opCheck = trim(operand);
                bool hasBang = !opCheck.empty() && opCheck.back() == '!';
                if (hasBang) opCheck.pop_back();
                if (opCheck.size() < 2 || opCheck.front() != '[' || opCheck.back() != ']') {
                    addCompileError(inst.lineNo, CompileErrorKind::InvalidMemoryOperand, STR_UNSUPPORTED_OPCODE + inst.op + STR_INVALID_MEM_OPERAND + operand);
                    continue;
                }

                const std::string inner = trim(opCheck.substr(1, opCheck.size() - 2));
                const auto pieces = splitOperands(inner);
                if (pieces.empty() || registerIndex(pieces[0]) < 0) {
                    addCompileError(inst.lineNo, CompileErrorKind::InvalidRegister, STR_UNSUPPORTED_OPCODE + inst.op + STR_INVALID_BASE_REG + operand);
                    continue;
                }
                if (pieces.size() >= 2) {
                    const bool ok = registerIndex(pieces[1]) >= 0 || parseNumber(pieces[1]).has_value();
                    if (!ok) {
                        addCompileError(inst.lineNo, CompileErrorKind::InvalidMemoryOperand, STR_UNSUPPORTED_OPCODE + inst.op + STR_INVALID_OFFSET + pieces[1]);
                    }
                }
                if (inst.operands.size() == 3) {
                    const bool ok = registerIndex(inst.operands[2]) >= 0 || parseNumber(inst.operands[2]).has_value();
                    if (!ok) {
                        addCompileError(inst.lineNo, CompileErrorKind::InvalidMemoryOperand, STR_INVALID_POST_OFFSET + inst.operands[2]);
                    }
                }
                continue;
            }

            if (inst.op == "B" || inst.op == "BL") {
                if (inst.operands.size() != 1) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, STR_UNSUPPORTED_OPCODE + inst.op + STR_NEED_BRANCH_TARGET);
                    continue;
                }
                if (compiled_.codeLabels.count(upper(trim(inst.operands[0]))) == 0) {
                    addCompileError(inst.lineNo, CompileErrorKind::UndefinedLabel, STR_UNDEFINED_LABEL + inst.operands[0]);
                }
                continue;
            }

            if (inst.op == "BX") {
                if (inst.operands.size() != 1) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, STR_BX_NEED_OPERAND);
                    continue;
                }
                if (registerIndex(inst.operands[0]) < 0) {
                    addCompileError(inst.lineNo, CompileErrorKind::InvalidRegister, STR_BX_INVALID_REG + inst.operands[0]);
                }
                continue;
            }

            if (inst.op == "TST" || inst.op == "TEQ" || inst.op == "CMP" || inst.op == "CMN") {
                if (inst.operands.size() != 2) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, STR_UNSUPPORTED_OPCODE + inst.op + STR_REQUIRES_OPERANDS + "2" + STR_OPERAND_SUFFIX);
                }
                continue;
            }

            if (inst.op == "MUL" || inst.op == "MLA") {
                if (inst.operands.size() < 3) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, STR_UNSUPPORTED_OPCODE + inst.op + STR_INSUFFICIENT_OPERANDS);
                }
                if (!inst.operands.empty() && registerIndex(inst.operands[0]) < 0) {
                    addCompileError(inst.lineNo, CompileErrorKind::InvalidRegister, STR_UNSUPPORTED_OPCODE + inst.op + STR_DEST_REG_INVALID + inst.operands[0]);
                }
                continue;
            }

            if (inst.op == "NOP") {
                if (!inst.operands.empty()) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, STR_NOP_NO_OPERANDS);
                }
                continue;
            }

            if (inst.op == "LDM" || inst.op == "STM" || inst.op == "PUSH" || inst.op == "POP"
                || inst.op.find("LDM") == 0 || inst.op.find("STM") == 0) {
                int baseReg = -1;
                if (inst.op == "PUSH" || inst.op == "POP") {
                    baseReg = 13;
                } else {
                    if (inst.operands.size() < 2) {
                        addCompileError(inst.lineNo, CompileErrorKind::OperandCount, STR_UNSUPPORTED_OPCODE + inst.op + STR_NEED_BASE_AND_LIST);
                        continue;
                    }
                    std::string baseText = inst.operands[0];
                    if (!baseText.empty() && baseText.back() == '!') {
                        baseText.pop_back();
                        baseText = trim(baseText);
                    }
                    baseReg = registerIndex(baseText);
                    if (baseReg < 0) {
                        addCompileError(inst.lineNo, CompileErrorKind::InvalidRegister, STR_UNSUPPORTED_OPCODE + inst.op + STR_INVALID_BASE_REG + inst.operands[0]);
                        continue;
                    }
                }
                const std::string &listText = inst.operands.back();
                const auto regs = parseRegisterList(listText);
                if (regs.empty()) {
                    addCompileError(inst.lineNo, CompileErrorKind::Generic, STR_UNSUPPORTED_OPCODE + inst.op + STR_INVALID_REG_LIST + listText);
                }
                continue;
            }
        }
    }

    void addCompileError(int lineNo, CompileErrorKind kind, const std::string &detail) {
        const std::string prefix = lineNo > 0 ? sourceName_ + "(" + std::to_string(lineNo) + ")" : sourceName_;
        compileErrors_.push_back(prefix + ": Error: " + compileErrorCode(kind) + ": " + detail);
    }

    void printRegisterUsage(const std::set<int> &usedRegisters, const std::set<int> &writtenRegisters) const {
        if (!usedRegisters.empty()) {
            std::cout << STR_REGISTERS_USED;
            bool first = true;
            for (int reg : usedRegisters) {
                if (!first) std::cout << ", ";
                std::cout << registerName(reg);
                first = false;
            }
            std::cout << "\n";
        }
        if (!writtenRegisters.empty()) {
            std::cout << STR_REGISTERS_WRITTEN;
            bool first = true;
            for (int reg : writtenRegisters) {
                if (!first) std::cout << ", ";
                std::cout << registerName(reg);
                first = false;
            }
            std::cout << "\n";
        }
    }

    void printState() const {
        if (touchedRegisters_.empty()) {
            return;
        }
        std::cout << STR_REGISTER_STATE;
        bool first = true;
        for (int index : touchedRegisters_) {
            if (!first) {
                std::cout << "  ";
            }
            std::cout << registerName(index) << '=' << registers_[index];
            first = false;
        }
        std::cout << "\n" << STR_FLAGS_LABEL << flags_.n << " Z=" << flags_.z << " C=" << flags_.c << " V=" << flags_.v << "\n";
    }

    std::string sourceName_ = "<stdin>";
    CompiledSource compiled_;
    std::vector<std::string> compileErrors_;
    std::vector<std::string> sourceLines_;
    std::array<long long, 16> registers_{};
    Flags flags_;
    std::unordered_map<long long, long long> memory_;
    std::set<int> touchedRegisters_;
    int pc_ = 0;
    bool finished_ = false;
    bool hasExecutableInstructions_ = false;
    std::unordered_set<int> instructionLines_;
    std::unordered_map<std::string, int> codeLabelLines_;
    std::unordered_map<std::string, int> dataLabelLines_;
    int pendingLabelLine_ = -1;
};

std::optional<std::string> readSourceFromFile(const std::string &filePath) {
    std::ifstream input(filePath);
    if (!input.is_open()) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string readSourceFromUser() {
    std::cout << STR_ENTER_SOURCE;
    std::cout << "--------------------------------------------------\n";
    std::string source;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (trim(upper(line)) == "END") {
            break;
        }
        source += line;
        source.push_back('\n');
    }
    return source;
}

std::string defaultProgram() {
    return R"(MOV R0, #3
MOV R1, #0
LOOP: ADD R1, R1, R0
SUBS R0, R0, #1
BNE LOOP
)";
}

void printUsage(const char *programName) {
    std::cout << STR_USAGE << programName << STR_USAGE_DETAIL;
}

// ============================================================
// TUI (Terminal User Interface) using ncurses
// ============================================================

class TUI {
public:
    TUI(Simulator &sim, const std::string &source, const std::string &sourceName)
        : sim_(sim), sourceText_(source), sourceName_(sourceName) {}

    void run() {
        // ncurses picks its output encoding from the C runtime locale.
        // Different platforms name the "always UTF-8" locale differently:
        //   * Microsoft UCRT (used by MSYS2 MinGW64)  -> "C.UTF-8"
        //   * glibc (Linux)                            -> ".UTF-8" or "C.UTF-8"
        //   * macOS                                    -> "C.UTF-8" (10.15+) or "en_US.UTF-8"
        // Try them in order; if none stick, fall back to the environment
        // locale, which is already UTF-8 under MSYS2's mintty.
        static const char *const utf8_locales[] = {
            "C.UTF-8",
            ".UTF-8",
            "en_US.UTF-8",
            "en-US.UTF-8",
        };
        const char *loc = nullptr;
        for (const char *name : utf8_locales) {
            if (setlocale(LC_ALL, name) != nullptr) {
                loc = name;
                break;
            }
        }
        if (loc == nullptr) {
            setlocale(LC_ALL, "");
        }
#ifdef _WIN32
        // Also switch the console code page for any non-ncurses output
        // (messages before initscr, batch mode, etc.). ncurses itself does
        // not consult the console code page.
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
        initscr();
        cbreak();
        noecho();
        curs_set(0);
        keypad(stdscr, TRUE);
        mousemask(ALL_MOUSE_EVENTS, NULL);
        mouseinterval(0);
        write(STDOUT_FILENO, "\033[?1000h\033[?1002h\033[?1006h", 21);

        setup();
        render();

        while (true) {
            int ch = getch();
            switch (ch) {
            case 's': case 'S': {
                if (!sim_.finished()) {
                    if (sim_.getPendingLabelLine() > 0) {
                        sim_.clearPendingLabel();
                    } else {
                        sim_.step(false);
                    }
                }
                statusMsg_.clear();
                render();
                break;
            }
            case 'r': {
                bool reloaded = false;
                sim_.reset();
                statusMsg_.clear();
                render();
                nodelay(stdscr, TRUE);
                while (!sim_.finished()) {
                    if (sim_.getPendingLabelLine() > 0) {
                        sim_.clearPendingLabel();
                    } else {
                        sim_.step(false);
                    }
                    render();
                    napms(20);
                    int k = getch();
                    if (k == 'q' || k == 'Q') {
                        nodelay(stdscr, FALSE);
                        teardown();
                        endwin();
                        return;
                    }
                    if (k == 'l' || k == 'L') {
                        nodelay(stdscr, FALSE);
                        if (!sim_.loadSource(sourceText_, sourceName_)) {
                            statusMsg_ = STR_COMPILE_FAIL + std::to_string(sim_.compileErrors().size()) + STR_ERRORS_SUFFIX;
                        } else {
                            statusMsg_ = STR_RELOADED;
                        }
                        sim_.reset();
                        srcScroll_ = 1;
                        reloaded = true;
                        break;
                    }
                    if (k != ERR) {
                        break;
                    }
                }
                nodelay(stdscr, FALSE);
                render();
                if (reloaded) break;
                break;
            }
            case 'l': case 'L':
                if (!sim_.loadSource(sourceText_, sourceName_)) {
                    statusMsg_ = STR_COMPILE_FAIL + std::to_string(sim_.compileErrors().size()) + STR_ERRORS_SUFFIX;
                } else {
                    statusMsg_ = STR_RELOADED;
                }
                sim_.reset();
                srcScroll_ = 1;
                render();
                break;
            case 'q': case 'Q':
                teardown();
                write(STDOUT_FILENO, "\033[?1006l\033[?1002l\033[?1000l", 24);
                endwin();
                return;
            case KEY_UP:
                if (srcScroll_ > 1) { --srcScroll_; render(); }
                break;
            case KEY_DOWN:
                { ++srcScroll_; render(); }
                break;
            case KEY_PPAGE: {
                int h, d;
                getmaxyx(srcWin_, h, d);
                srcScroll_ -= h - 2;
                if (srcScroll_ < 1) srcScroll_ = 1;
                render();
                break;
            }
            case KEY_NPAGE: {
                int h, d;
                getmaxyx(srcWin_, h, d);
                srcScroll_ += h - 2;
                render();
                break;
            }
            case KEY_MOUSE: {
                MEVENT ev;
                if (getmouse(&ev) == OK) {
                    if (ev.bstate & ((mmask_t)0x80000)) {
                        if (srcScroll_ > 1) { --srcScroll_; render(); }
                    } else if (ev.bstate & ((mmask_t)0x2000000)) {
                        ++srcScroll_; render();
                    }
                }
                break;
            }
            case KEY_RESIZE:
                teardown();
                setup();
                render();
                break;
            }
        }
    }

private:
    void setup() {
        int maxY, maxX;
        getmaxyx(stdscr, maxY, maxX);

        if (maxY < 22 || maxX < 50) {
            endwin();
            std::cerr << STR_TERMINAL_TOO_SMALL << maxX << "x" << maxY << STR_TERMINAL_SUGGESTION << "\n";
            std::exit(1);
        }

        int divX = maxX * 3 / 5;
        if (divX < 20) divX = 20;

        srcWin_ = newwin(maxY - 2, divX, 1, 0);
        regWin_ = newwin(maxY - 2, maxX - divX, 1, divX);
    }

    void teardown() {
        if (srcWin_) { delwin(srcWin_); srcWin_ = nullptr; }
        if (regWin_) { delwin(regWin_); regWin_ = nullptr; }
    }

    void render() {
        renderTitleBar();
        renderSourcePanel();
        renderRegisterPanel();
        renderStatusBar();
        doupdate();
    }

    void renderTitleBar() {
        int maxX;
        [[maybe_unused]] int maxY;
        getmaxyx(stdscr, maxY, maxX);

        attron(A_REVERSE);
        const char *title = STR_ARM_SIMULATOR;
        mvprintw(0, 0, "%s", title);
        const char *help = STR_HELP_B_TITLE;
        mvprintw(0, maxX - static_cast<int>(std::strlen(help)) - 1, "%s", help);
        for (int x = static_cast<int>(std::strlen(title)); x < maxX - static_cast<int>(std::strlen(help)) - 1; ++x) {
            mvaddch(0, x, ' ');
        }
        attroff(A_REVERSE);
        wnoutrefresh(stdscr);
    }

    void renderSourcePanel() {
        werase(srcWin_);

        int pH, pW;
        getmaxyx(srcWin_, pH, pW);
        box(srcWin_, 0, 0);
        mvwprintw(srcWin_, 0, 2, " Source ");

        const auto &lines = sim_.getSourceLines();
        if (lines.empty()) {
            wnoutrefresh(srcWin_);
            return;
        }

        int currentLine = sim_.getPendingLabelLine();
        if (currentLine < 0 && !sim_.finished()) {
            const auto &compiled = sim_.getCompiled();
            int pc = sim_.getPC();
            if (pc >= 0 && pc < static_cast<int>(compiled.instructions.size())) {
                currentLine = compiled.instructions[pc].lineNo;
            }
        }

        int visible = pH - 2;
        if (currentLine > 0) {
            if (currentLine < srcScroll_) {
                srcScroll_ = currentLine;
            } else if (currentLine >= srcScroll_ + visible) {
                srcScroll_ = currentLine - visible + 1;
            }
        }
        if (srcScroll_ < 1) srcScroll_ = 1;

        for (int i = 0; i < visible; ++i) {
            int lineNum = srcScroll_ + i;
            if (lineNum < 1 || lineNum > static_cast<int>(lines.size())) break;

            bool isCurrent = (lineNum == currentLine);
            if (isCurrent) {
                wattron(srcWin_, A_REVERSE);
            }

            char prefix[16];
            snprintf(prefix, sizeof(prefix), "%3d: ", lineNum);
            std::string text = lines[lineNum - 1];
            int maxText = pW - 2 - static_cast<int>(std::strlen(prefix));
            if (maxText < 0) maxText = 0;
            if (static_cast<int>(text.size()) > maxText) {
                text = text.substr(0, maxText);
            }
            mvwprintw(srcWin_, i + 1, 1, "%s%s", prefix, text.c_str());

            if (isCurrent) {
                wattroff(srcWin_, A_REVERSE);
            }
        }

        wnoutrefresh(srcWin_);
    }

    void renderRegisterPanel() {
        werase(regWin_);

        int pH, pW;
        getmaxyx(regWin_, pH, pW);

        box(regWin_, 0, 0);
        mvwprintw(regWin_, 0, 2, " Registers ");

        const auto &regs = sim_.getRegisters();
        const auto &flags = sim_.getFlags();
        static const char *regNames[16] = {
            "R0","R1","R2","R3","R4","R5","R6","R7",
            "R8","R9","R10","R11","R12","SP","LR","PC"
        };

        int line = 1;
        for (int i = 0; i < 16 && line < pH - 1; ++i, ++line) {
            bool isPC = (i == 15);
            if (isPC) wattron(regWin_, A_BOLD);
            long long val = (long long)regs[i];
            unsigned u32 = (unsigned)(val & 0xFFFFFFFFULL);
            char buf[64];
            snprintf(buf, sizeof(buf), " %3s = %11lld  (0x%08X)", regNames[i], val, u32);
            mvwprintw(regWin_, line, 1, "%s", buf);
            if (isPC) wattroff(regWin_, A_BOLD);
        }

        ++line;
        if (line < pH - 1) {
            char fbuf[64];
            snprintf(fbuf, sizeof(fbuf), " Flags: N=%d  Z=%d  C=%d  V=%d",
                     flags.n ? 1 : 0, flags.z ? 1 : 0,
                     flags.c ? 1 : 0, flags.v ? 1 : 0);
            mvwprintw(regWin_, line, 1, "%s", fbuf);
        }

        wnoutrefresh(regWin_);
    }

    void renderStatusBar() {
        int maxY, maxX;
        getmaxyx(stdscr, maxY, maxX);

        attron(A_REVERSE);
        std::string status;
        if (!statusMsg_.empty()) {
            status = " " + statusMsg_;
        } else if (sim_.finished()) {
            status = STR_STATUS_FINISHED;
        } else {
            int pc = sim_.getPC();
            int total = static_cast<int>(sim_.getCompiled().instructions.size());
            status = STR_STATUS_RUNNING + std::to_string(pc) + "/" + std::to_string(total);
        }
        mvprintw(maxY - 1, 0, "%s", status.c_str());
        clrtoeol();
        attroff(A_REVERSE);
        wnoutrefresh(stdscr);
    }

    Simulator &sim_;
    std::string sourceText_;
    std::string sourceName_;
    std::string statusMsg_;
    WINDOW *srcWin_ = nullptr;
    WINDOW *regWin_ = nullptr;
    int srcScroll_ = 1;
};

} // namespace

int main(int argc, char *argv[]) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Simulator simulator;

    std::string sourceText;
    std::string sourceName;

    if (argc >= 2) {
        const std::string argument = argv[1];
        if (argument == "-h" || argument == "--help") {
            printUsage(argv[0]);
            return 0;
        }

        if (argument == "--batch" && argc >= 3) {
            auto opt = readSourceFromFile(argv[2]);
            if (!opt.has_value()) {
                std::cerr << STR_CANNOT_READ_SOURCE << argv[2] << "\n";
                return 1;
            }
            sourceText = *opt;
            sourceName = argv[2];
            if (!simulator.loadSource(sourceText, sourceName)) {
                std::cerr << STR_SOURCE_CHECK_FAIL;
                for (const auto &error : simulator.compileErrors()) {
                    std::cerr << error << "\n";
                }
                return 1;
            }
            int maxInstructions = 10000;
            int executed = 0;
            while (!simulator.finished() && executed < maxInstructions) {
                simulator.step(false);
                executed++;
            }
            if (executed >= maxInstructions) {
                std::cerr << STR_MAX_INSTRUCTIONS;
            }
            const auto &regs = simulator.getRegisters();
            const auto &f = simulator.getFlags();
            for (int i = 0; i < 16; ++i) {
                std::cout << registerName(i) << "=" << regs[i] << " ";
            }
            std::cout << "N=" << (f.n ? 1 : 0) << " Z=" << (f.z ? 1 : 0)
                      << " C=" << (f.c ? 1 : 0) << " V=" << (f.v ? 1 : 0) << "\n";
            return 0;
        }

        auto opt = readSourceFromFile(argument);
        if (!opt.has_value()) {
            std::cout << STR_CANNOT_READ_SOURCE << argument << "\n";
            return 1;
        }
        sourceText = *opt;
        sourceName = argument;
    } else {
        sourceText = readSourceFromUser();
        if (trim(sourceText).empty()) {
            std::cout << STR_NO_SOURCE_INPUT;
            sourceText = defaultProgram();
        }
        sourceName = "<stdin>";
    }

    if (!simulator.loadSource(sourceText, sourceName)) {
        std::cout << STR_SOURCE_CHECK_FAIL_RUN;
        for (const auto &error : simulator.compileErrors()) {
            std::cout << error << "\n";
        }
        return 1;
    }

    TUI tui(simulator, sourceText, sourceName);
    tui.run();
    return 0;
}
