#include <algorithm>
#include <array>
#include <cctype>
#include <clocale>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
        if (ch == '[') {
            ++bracketDepth;
            current.push_back(ch);
            continue;
        }
        if (ch == ']') {
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

Cond parseCondSuffix(const std::string &op, std::string &base) {
    static const std::array<std::pair<const char *, Cond>, 14> suffixes = {{
        {"EQ", Cond::EQ}, {"NE", Cond::NE}, {"GT", Cond::GT}, {"LT", Cond::LT}, {"GE", Cond::GE},
        {"LE", Cond::LE}, {"HI", Cond::HI}, {"LS", Cond::LS}, {"CS", Cond::CS}, {"CC", Cond::CC},
        {"MI", Cond::MI}, {"PL", Cond::PL}, {"VS", Cond::VS}, {"VC", Cond::VC},
    }};
    for (const auto &[suffix, cond] : suffixes) {
        const std::size_t suffixLength = std::char_traits<char>::length(suffix);
        if (op.size() > suffixLength && upper(op.substr(op.size() - suffixLength)) == suffix) {
            base = op.substr(0, op.size() - suffixLength);
            return cond;
        }
    }
    base = op;
    return Cond::AL;
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

bool isSupportedOpcode(const std::string &op) {
    static const std::unordered_set<std::string> supported = {
        "MOV", "MVN", "ADD", "SUB", "AND", "ORR", "EOR", "BIC",
        "CMP", "CMN", "LDR", "STR", "B", "BL", "BX"
    };
    return supported.count(upper(op)) > 0;
}

bool isSupportedInstructionToken(const std::string &token) {
    std::string baseOp;
    parseCondSuffix(upper(token), baseOp);
    if (!baseOp.empty() && baseOp.back() == 'S' && baseOp != "B" && baseOp != "BL" && baseOp != "BX") {
        baseOp.pop_back();
    }
    return isSupportedOpcode(baseOp);
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
                addCompileError(labelLine, CompileErrorKind::DuplicateLabel, "重复定义标签 '" + label + "'");
                return;
            }
            compiled_.codeLabels[normalized] = static_cast<int>(compiled_.instructions.size());
            compiled_.symbolAddresses[normalized] = static_cast<long long>(compiled_.instructions.size()) * 4;
            codeLabelLines_[normalized] = labelLine;
        };

        auto defineDataLabel = [&](const std::string &label, int labelLine) {
            const std::string normalized = upper(label);
            if (compiled_.codeLabels.count(normalized) > 0 || compiled_.symbolAddresses.count(normalized) > 0) {
                addCompileError(labelLine, CompileErrorKind::DuplicateLabel, "重复定义标签 '" + label + "'");
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

            std::string baseOp;
            inst.cond = parseCondSuffix(upper(opToken), baseOp);
            inst.setFlags = false;
            if (!baseOp.empty() && baseOp.back() == 'S' && baseOp != "B" && baseOp != "BL" && baseOp != "BX") {
                inst.setFlags = true;
                baseOp.pop_back();
            }
            inst.op = upper(baseOp);

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
                        addCompileError(targetLine, CompileErrorKind::InvalidMemoryOperand, "SPACE 大小无效: " + operands[0]);
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
                    addCompileError(targetLine, CompileErrorKind::InvalidMemoryOperand, normalized + " 常量无效: " + operand);
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
                std::cout << "程序已结束。\n";
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
                return registers_[reg];
            }
            const auto number = parseNumber(token);
            return number.has_value() ? *number : 0;
        };

        auto writeValue = [&](const std::string &token, long long value) {
            const int reg = registerIndex(token);
            if (reg >= 0) {
                writtenRegisters.insert(reg);
                registers_[reg] = value;
                touchedRegisters_.insert(reg);
            }
        };

        auto parseMemoryOperand = [&](const std::string &token) -> std::optional<MemoryOperand> {
            std::string text = trim(token);
            if (text.size() < 3 || text.front() != '[' || text.back() != ']') {
                return std::nullopt;
            }
            text = trim(text.substr(1, text.size() - 2));
            const auto pieces = splitOperands(text);
            if (pieces.empty()) {
                return std::nullopt;
            }
            MemoryOperand operand;
            operand.baseReg = registerIndex(pieces[0]);
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
                    operand.offset = registers_[offsetReg];
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

        auto updateNZ = [&](long long value) {
            flags_.z = (value == 0);
            flags_.n = (value < 0);
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
                std::cout << "条件不满足，跳过执行。\n";
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
                const long long result = op == "MOV" ? source : ~source;
                writeValue(inst.operands[0], result);
                if (inst.setFlags) {
                    updateNZ(result);
                }
            }
        } else if (op == "ADD" || op == "SUB" || op == "AND" || op == "ORR" || op == "EOR" || op == "BIC") {
            if (inst.operands.size() >= 3) {
                const long long left = readValue(inst.operands[1]);
                const long long right = readValue(inst.operands[2]);
                long long result = 0;
                if (op == "ADD") result = left + right;
                if (op == "SUB") result = left - right;
                if (op == "AND") result = left & right;
                if (op == "ORR") result = left | right;
                if (op == "EOR") result = left ^ right;
                if (op == "BIC") result = left & ~right;
                writeValue(inst.operands[0], result);
                if (inst.setFlags) {
                    updateNZ(result);
                }
            }
        } else if (op == "CMP") {
            if (inst.operands.size() >= 2) {
                const long long left = readValue(inst.operands[0]);
                const long long right = readValue(inst.operands[1]);
                const long long result = left - right;
                updateNZ(result);
                flags_.c = left >= right;
                flags_.v = ((left ^ right) & (left ^ result)) < 0;
            }
        } else if (op == "CMN") {
            if (inst.operands.size() >= 2) {
                const long long left = readValue(inst.operands[0]);
                const long long right = readValue(inst.operands[1]);
                const long long result = left + right;
                updateNZ(result);
                flags_.c = result < left;
                flags_.v = ((~(left ^ right)) & (left ^ result)) < 0;
            }
        } else if (op == "LDR") {
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
                        const long long address = registers_[memoryOperand->baseReg] + memoryOperand->offset;
                        const long long value = memory_.count(address) ? memory_[address] : 0;
                        writeValue(dest, value);
                        if (inst.operands.size() == 3) {
                            const long long postOffset = readValue(inst.operands[2]);
                            registers_[memoryOperand->baseReg] += postOffset;
                            writtenRegisters.insert(memoryOperand->baseReg);
                        }
                    }
                }
            }
        } else if (op == "STR") {
            if (inst.operands.size() >= 2) {
                const long long value = readValue(inst.operands[0]);
                const auto memoryOperand = parseMemoryOperand(inst.operands[1]);
                if (memoryOperand.has_value()) {
                    const long long address = registers_[memoryOperand->baseReg] + memoryOperand->offset;
                    memory_[address] = value;
                    if (inst.operands.size() == 3) {
                        const long long postOffset = readValue(inst.operands[2]);
                        registers_[memoryOperand->baseReg] += postOffset;
                        writtenRegisters.insert(memoryOperand->baseReg);
                        touchedRegisters_.insert(memoryOperand->baseReg);
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
                addCompileError(inst.lineNo, CompileErrorKind::UnsupportedOpcode, "不支持的指令 '" + inst.op + "'");
                continue;
            }

            if (inst.op == "MOV" || inst.op == "MVN") {
                if (inst.operands.size() != 2) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, "指令 '" + inst.op + "' 需要 2 个操作数");
                }
                if (!inst.operands.empty() && registerIndex(inst.operands[0]) < 0) {
                    addCompileError(inst.lineNo, CompileErrorKind::InvalidRegister, "指令 '" + inst.op + "' 的目标寄存器无效: " + inst.operands[0]);
                }
                continue;
            }

            if (inst.op == "ADD" || inst.op == "SUB" || inst.op == "AND" || inst.op == "ORR" || inst.op == "EOR" || inst.op == "BIC") {
                if (inst.operands.size() != 3) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, "指令 '" + inst.op + "' 需要 3 个操作数");
                }
                if (!inst.operands.empty() && registerIndex(inst.operands[0]) < 0) {
                    addCompileError(inst.lineNo, CompileErrorKind::InvalidRegister, "指令 '" + inst.op + "' 的目标寄存器无效: " + inst.operands[0]);
                }
                continue;
            }

            if (inst.op == "CMP" || inst.op == "CMN") {
                if (inst.operands.size() != 2) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, "指令 '" + inst.op + "' 需要 2 个操作数");
                }
                continue;
            }

            if (inst.op == "LDR" || inst.op == "STR") {
                if (inst.operands.size() != 2 && inst.operands.size() != 3) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, "指令 '" + inst.op + "' 需要 2 或 3 个操作数");
                    continue;
                }
                if (!inst.operands.empty() && registerIndex(inst.operands[0]) < 0) {
                    addCompileError(inst.lineNo, CompileErrorKind::InvalidRegister, "指令 '" + inst.op + "' 的寄存器无效: " + inst.operands[0]);
                }

                const std::string &operand = inst.operands[1];
                if (!operand.empty() && operand[0] == '=') {
                    if (inst.op == "STR") {
                        addCompileError(inst.lineNo, CompileErrorKind::InvalidMemoryOperand, "STR 不支持字面量操作数: " + operand);
                    } else if (compiled_.symbolAddresses.count(upper(trim(operand.substr(1)))) == 0) {
                        addCompileError(inst.lineNo, CompileErrorKind::UndefinedLabel, "未定义的符号: " + operand.substr(1));
                    }
                    continue;
                }

                if (operand.size() < 3 || operand.front() != '[' || operand.back() != ']') {
                    addCompileError(inst.lineNo, CompileErrorKind::InvalidMemoryOperand, "指令 '" + inst.op + "' 的内存操作数格式错误: " + operand);
                    continue;
                }

                const std::string inner = trim(operand.substr(1, operand.size() - 2));
                const auto pieces = splitOperands(inner);
                if (pieces.empty() || registerIndex(pieces[0]) < 0) {
                    addCompileError(inst.lineNo, CompileErrorKind::InvalidRegister, "指令 '" + inst.op + "' 的基址寄存器无效: " + operand);
                    continue;
                }
                if (pieces.size() >= 2) {
                    const bool ok = registerIndex(pieces[1]) >= 0 || parseNumber(pieces[1]).has_value();
                    if (!ok) {
                        addCompileError(inst.lineNo, CompileErrorKind::InvalidMemoryOperand, "指令 '" + inst.op + "' 的偏移量无效: " + pieces[1]);
                    }
                }
                if (inst.operands.size() == 3) {
                    const bool ok = registerIndex(inst.operands[2]) >= 0 || parseNumber(inst.operands[2]).has_value();
                    if (!ok) {
                        addCompileError(inst.lineNo, CompileErrorKind::InvalidMemoryOperand, "后索引偏移无效: " + inst.operands[2]);
                    }
                }
                continue;
            }

            if (inst.op == "B" || inst.op == "BL") {
                if (inst.operands.size() != 1) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, "指令 '" + inst.op + "' 需要 1 个跳转目标");
                    continue;
                }
                if (compiled_.codeLabels.count(upper(trim(inst.operands[0]))) == 0) {
                    addCompileError(inst.lineNo, CompileErrorKind::UndefinedLabel, "跳转目标标签未定义: " + inst.operands[0]);
                }
                continue;
            }

            if (inst.op == "BX") {
                if (inst.operands.size() != 1) {
                    addCompileError(inst.lineNo, CompileErrorKind::OperandCount, "指令 'BX' 需要 1 个操作数");
                    continue;
                }
                if (registerIndex(inst.operands[0]) < 0) {
                    addCompileError(inst.lineNo, CompileErrorKind::InvalidRegister, "指令 'BX' 的寄存器无效: " + inst.operands[0]);
                }
            }
        }
    }

    void addCompileError(int lineNo, CompileErrorKind kind, const std::string &detail) {
        const std::string prefix = lineNo > 0 ? sourceName_ + "(" + std::to_string(lineNo) + ")" : sourceName_;
        compileErrors_.push_back(prefix + ": Error: " + compileErrorCode(kind) + ": " + detail);
    }

    void printRegisterUsage(const std::set<int> &usedRegisters, const std::set<int> &writtenRegisters) const {
        if (!usedRegisters.empty()) {
            std::cout << "使用寄存器: ";
            bool first = true;
            for (int reg : usedRegisters) {
                if (!first) std::cout << ", ";
                std::cout << registerName(reg);
                first = false;
            }
            std::cout << "\n";
        }
        if (!writtenRegisters.empty()) {
            std::cout << "写入寄存器: ";
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
        std::cout << "寄存器状态: ";
        bool first = true;
        for (int index : touchedRegisters_) {
            if (!first) {
                std::cout << "  ";
            }
            std::cout << registerName(index) << '=' << registers_[index];
            first = false;
        }
        std::cout << "\nFlags: N=" << flags_.n << " Z=" << flags_.z << " C=" << flags_.c << " V=" << flags_.v << "\n";
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
    std::cout << "请输入 ARM/Keil 风格汇编源码，单独输入 END 结束：\n";
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
    std::cout << "用法: " << programName << " [源码文件路径]\n";
    std::cout << "不传参数时，会进入粘贴源码模式，输入 END 结束。\n";
}

// ============================================================
// TUI (Terminal User Interface) using ncurses
// ============================================================

class TUI {
public:
    TUI(Simulator &sim, const std::string &source, const std::string &sourceName)
        : sim_(sim), sourceText_(source), sourceName_(sourceName) {}

    void run() {
        setlocale(LC_ALL, "");
        initscr();
        cbreak();
        noecho();
        curs_set(0);
        keypad(stdscr, TRUE);

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
                    sim_.step(false);
                    napms(5);
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
                            statusMsg_ = "编译失败 (" + std::to_string(sim_.compileErrors().size()) + " 个错误)";
                        } else {
                            statusMsg_ = "已重新加载";
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
                    statusMsg_ = "编译失败 (" + std::to_string(sim_.compileErrors().size()) + " 个错误)";
                } else {
                    statusMsg_ = "已重新加载";
                }
                sim_.reset();
                srcScroll_ = 1;
                render();
                break;
            case 'q': case 'Q':
                teardown();
                endwin();
                return;
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
            std::cerr << "终端太小（需要至少 50x22，当前 " << maxX << "x" << maxY << "）\n";
            std::exit(1);
        }

        int divX = maxX * 3 / 5;
        if (divX < 18) divX = 18;

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
        const char *title = " ARM Simulator ";
        mvprintw(0, 0, "%s", title);
        const char *help = " S:step  R:run  L:reload  Q:quit ";
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
            char buf[64];
            snprintf(buf, sizeof(buf), " %3s = %12lld", regNames[i], (long long)regs[i]);
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
            status = " 程序已结束";
        } else {
            int pc = sim_.getPC();
            int total = static_cast<int>(sim_.getCompiled().instructions.size());
            status = " 运行中  PC=" + std::to_string(pc) + "/" + std::to_string(total);
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

        auto opt = readSourceFromFile(argument);
        if (!opt.has_value()) {
            std::cout << "无法读取源码文件: " << argument << "\n";
            return 1;
        }
        sourceText = *opt;
        sourceName = argument;
    } else {
        sourceText = readSourceFromUser();
        if (trim(sourceText).empty()) {
            std::cout << "未输入源码，使用内置示例。\n";
            sourceText = defaultProgram();
        }
        sourceName = "<stdin>";
    }

    if (!simulator.loadSource(sourceText, sourceName)) {
        std::cout << "源码检查失败，未开始运行。\n";
        for (const auto &error : simulator.compileErrors()) {
            std::cout << error << "\n";
        }
        return 1;
    }

    TUI tui(simulator, sourceText, sourceName);
    tui.run();
    return 0;
}
