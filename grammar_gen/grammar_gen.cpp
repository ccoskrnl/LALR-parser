#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <map>
#include <set>
#include <cctype>
#include <locale>
#include <codecvt>

// 文法规则解析器
class GrammarParser {
private:
    std::map<std::string, std::vector<std::vector<std::string>>> productions;
    std::set<std::string> nonTerminals;
    std::set<std::string> terminals;
    std::string startSymbol;

    // 清理字符串两端的空白字符
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        size_t end = str.find_last_not_of(" \t\n\r");
        if (start == std::string::npos || end == std::string::npos) {
            return "";
        }
        return str.substr(start, end - start + 1);
    }

    // 分割字符串
    std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(str);
        while (std::getline(tokenStream, token, delimiter)) {
            token = trim(token);
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        return tokens;
    }

    // 判断是否为非终结符（假设非终结符以大写字母开头或包含在<>中）
    bool isNonTerminal(const std::string& symbol) {
        if (symbol.empty()) return false;
        if (symbol == "ε" || symbol == "epsilon") return true;

        // 检查是否被<>包围
        if (symbol.size() >= 2 && symbol[0] == '<' && symbol[symbol.size() - 1] == '>') {
            return true;
        }

        // 检查是否以大写字母开头
        return std::isupper(static_cast<unsigned char>(symbol[0]));
    }

    // 提取符号名称（去掉尖括号）
    std::string extractSymbolName(const std::string& symbol) {
        if (symbol.size() >= 2 && symbol[0] == '<' && symbol[symbol.size() - 1] == '>') {
            return symbol.substr(1, symbol.size() - 2);
        }
        return symbol;
    }

    // 检测并处理不同的箭头表示方式
    size_t findArrowPosition(const std::string& line) {
        // 尝试查找不同的箭头表示方式
        size_t pos = line.find("->");
        if (pos != std::string::npos) return pos;

        pos = line.find("→");
        if (pos != std::string::npos) return pos;

        // 尝试查找UTF-8编码的箭头（可能是多字节字符）
        pos = line.find("\xE2\x86\x92"); // UTF-8编码的→
        if (pos != std::string::npos) return pos;

        return std::string::npos;
    }

    // 获取箭头长度
    size_t getArrowLength(const std::string& line, size_t arrowPos) {
        if (arrowPos == std::string::npos) return 0;

        // 检查是否是 "->"
        if (line.substr(arrowPos, 2) == "->") return 2;

        // 检查是否是 "→" (单字符)
        if (line.substr(arrowPos, 1) == "→") return 1;

        // 检查是否是UTF-8编码的→ (三字节)
        if (arrowPos + 2 < line.size() &&
            line.substr(arrowPos, 3) == "\xE2\x86\x92") return 3;

        return 0;
    }

    // 将符号名称转换为有效的C++标识符
    std::string toValidIdentifier(const std::string& name) {
        if (name.empty()) return "empty";

        std::string result;
        for (char c : name) {
            if (std::isalnum(static_cast<unsigned char>(c))) {
                result += c;
            }
            else {
                result += '_';
            }
        }

        // 如果以数字开头，添加前缀
        if (!result.empty() && std::isdigit(static_cast<unsigned char>(result[0]))) {
            result = "NT_" + result;
        }

        return result;
    }

public:
    GrammarParser() = default;

    // 解析文法文件
    bool parseGrammarFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << filename << std::endl;
            return false;
        }

        std::string line;
        bool firstProduction = true;

        while (std::getline(file, line)) {
            line = trim(line);

            // 跳过空行和注释
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // 查找箭头位置
            size_t arrowPos = findArrowPosition(line);
            if (arrowPos == std::string::npos) {
                std::cerr << "Error: Invalid production format (no arrow found): " << line << std::endl;
                continue;
            }

            // 获取箭头长度
            size_t arrowLen = getArrowLength(line, arrowPos);
            if (arrowLen == 0) {
                std::cerr << "Error: Invalid arrow format: " << line << std::endl;
                continue;
            }

            std::string leftStr = trim(line.substr(0, arrowPos));
            std::string rightStr = trim(line.substr(arrowPos + arrowLen));

            // 提取左部非终结符
            std::string leftSymbol = extractSymbolName(leftStr);
            if (leftSymbol.empty()) {
                std::cerr << "Error: Invalid left symbol: " << leftStr << std::endl;
                continue;
            }

            // 设置开始符号（第一个非终结符）
            if (firstProduction) {
                startSymbol = leftSymbol;
                firstProduction = false;
            }

            // 记录非终结符
            nonTerminals.insert(leftSymbol);

            // 处理右部多个产生式（用|分隔）
            std::vector<std::string> alternatives;
            size_t pipePos = rightStr.find('|');

            if (pipePos == std::string::npos) {
                alternatives.push_back(rightStr);
            }
            else {
                // 分割多个产生式
                size_t start = 0;
                while (pipePos != std::string::npos) {
                    alternatives.push_back(trim(rightStr.substr(start, pipePos - start)));
                    start = pipePos + 1;
                    pipePos = rightStr.find('|', start);
                }
                alternatives.push_back(trim(rightStr.substr(start)));
            }

            // 处理每个产生式
            for (const auto& alt : alternatives) {
                std::vector<std::string> rightSymbols;

                if (alt == "ε" || alt == "epsilon" || alt.empty()) {
                    // 空产生式
                    rightSymbols.push_back("epsilon");
                }
                else {
                    // 按空格分割右部符号
                    std::vector<std::string> tokens = split(alt, ' ');
                    for (const auto& token : tokens) {
                        std::string symbol = extractSymbolName(token);
                        if (isNonTerminal(symbol)) {
                            nonTerminals.insert(symbol);
                        }
                        else if (symbol != "epsilon") {
                            terminals.insert(symbol);
                        }
                        rightSymbols.push_back(symbol);
                    }
                }

                // 添加到产生式集合
                productions[leftSymbol].push_back(rightSymbols);
            }
        }

        file.close();
        return true;
    }

    // 生成C++代码
    std::string generateCppCode() {
        std::ostringstream code;

        code << "// 创建文法\n";
        code << "Grammar createGrammar() {\n";
        code << "    Grammar grammar;\n\n";

        // 定义非终结符
        code << "    // 定义非终结符\n";
        for (const auto& nt : nonTerminals) {
            if (nt != "epsilon") {
                std::string varName = toValidIdentifier(nt);
                code << "    Symbol " << varName << "(\"" << nt << "\", SymbolType::NON_TERMINAL);\n";
            }
        }
        code << "\n";

        // 设置开始符号
        code << "    // 设置开始符号\n";
        code << "    grammar.startSymbol = " << toValidIdentifier(startSymbol) << ";\n\n";

        // 添加产生式
        code << "    // 添加产生式\n";
        for (const auto& prod : productions) {
            const std::string& left = prod.first;
            std::string leftVar = toValidIdentifier(left);

            for (const auto& right : prod.second) {
                code << "    grammar.addProduction(" << leftVar << ", {";

                for (size_t i = 0; i < right.size(); ++i) {
                    const std::string& symbol = right[i];
                    if (symbol == "epsilon") {
                        code << "grammar.epsilon";
                    }
                    else if (nonTerminals.find(symbol) != nonTerminals.end()) {
                        code << toValidIdentifier(symbol);
                    }
                    else {
                        // 终结符需要转义特殊字符
                        std::string escapedSymbol = symbol;
                        // 转义引号
                        size_t pos = 0;
                        while ((pos = escapedSymbol.find("\"", pos)) != std::string::npos) {
                            escapedSymbol.replace(pos, 1, "\\\"");
                            pos += 2;
                        }
                        code << "Symbol(\"" << escapedSymbol << "\", SymbolType::TERMINAL)";
                    }

                    if (i < right.size() - 1) {
                        code << ", ";
                    }
                }

                code << "});\n";
            }
        }

        code << "\n    return grammar;\n";
        code << "}\n";

        return code.str();
    }

    // 获取非终结符集合
    const std::set<std::string>& getNonTerminals() const {
        return nonTerminals;
    }

    // 获取终结符集合
    const std::set<std::string>& getTerminals() const {
        return terminals;
    }

    // 获取开始符号
    const std::string& getStartSymbol() const {
        return startSymbol;
    }
};


int main(int argc, char* argv[]) {
    //if (argc != 2) {
    //    std::cerr << "Usage: " << argv[0] << " <grammar_file>" << std::endl;
    //    return 1;
    //}

    GrammarParser parser;

    if (!parser.parseGrammarFile("gram_exp01.txt")) {
    //if (!parser.parseGrammarFile("grammar_example.txt")) {
        return 1;
    }
    //if (!parser.parseGrammarFile(argv[1])) {
    //    return 1;
    //}

    std::cout << parser.generateCppCode();

    return 0;
}