// evaluator.cpp
#include "evaluator.hpp"
#include <sstream>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <cctype>

// Helper to format double values to strings without trailing zeros
std::string formatDouble(double val) {
    if (std::isnan(val)) return "NaN";
    if (std::isinf(val)) return "Infinity";
    std::ostringstream oss;
    oss << std::setprecision(12) << val;
    std::string s = oss.str();
    return s;
}

// Exception-free numeric validator to avoid C++ exception runtime in WASM
static bool isNumber(const std::string& s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[i] == '-' || s[i] == '+') i++;
    bool dotSeen = false;
    bool digitSeen = false;
    bool eSeen = false;
    while (i < s.length()) {
        char c = s[i];
        if (c == '.') {
            if (dotSeen || eSeen) return false;
            dotSeen = true;
        } else if (c == 'e' || c == 'E') {
            if (eSeen || !digitSeen) return false;
            eSeen = true;
            digitSeen = false;
            if (i + 1 < s.length() && (s[i+1] == '-' || s[i+1] == '+')) {
                i++;
            }
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            digitSeen = true;
        } else {
            return false;
        }
        i++;
    }
    return digitSeen;
}


SmartExpressionEvaluator::SmartExpressionEvaluator() {
    // Initialize standard mathematical constants
    variables["pi"] = 3.14159265358979323846;
    variables["e"] = 2.71828182845904523536;
}

bool SmartExpressionEvaluator::isOperator(char c) const {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '^';
}

bool SmartExpressionEvaluator::isOperator(const std::string& op) const {
    return op == "+" || op == "-" || op == "*" || op == "/" || op == "%" || op == "^" || op == "U-" || op == "U+";
}

int SmartExpressionEvaluator::getPrecedence(const std::string& op) const {
    if (op == "U-" || op == "U+") return 4;
    if (op == "^") return 3;
    if (op == "*" || op == "/" || op == "%") return 2;
    if (op == "+" || op == "-") return 1;
    return 0;
}

bool SmartExpressionEvaluator::isLeftAssociative(const std::string& op) const {
    // Exponentiation and unary operators are right-associative
    if (op == "^" || op == "U-" || op == "U+") return false;
    return true;
}

std::vector<SmartExpressionEvaluator::Token> SmartExpressionEvaluator::tokenize(const std::string& expr, std::string& errorMsg) {
    std::vector<Token> tokens;
    size_t i = 0;
    size_t len = expr.length();

    while (i < len) {
        char c = expr[i];

        // Skip whitespace
        if (std::isspace(c)) {
            i++;
            continue;
        }

        // Parentheses
        if (c == '(') {
            tokens.push_back({Token::LPAREN, "("});
            i++;
            continue;
        }
        if (c == ')') {
            tokens.push_back({Token::RPAREN, ")"});
            i++;
            continue;
        }

        // Operators
        if (isOperator(c)) {
            std::string opStr(1, c);
            
            // Check if it is unary minus/plus
            // It is unary if it is '+' or '-' AND:
            // 1. It is the first token, OR
            // 2. The previous token is an operator or a left parenthesis.
            if (c == '-' || c == '+') {
                bool isUnary = false;
                if (tokens.empty()) {
                    isUnary = true;
                } else {
                    Token prev = tokens.back();
                    if (prev.type == Token::OPERATOR || prev.type == Token::LPAREN) {
                        isUnary = true;
                    }
                }
                
                if (isUnary) {
                    opStr = (c == '-') ? "U-" : "U+";
                }
            }
            
            tokens.push_back({Token::OPERATOR, opStr});
            i++;
            continue;
        }

        // Numbers (must handle decimals)
        if (std::isdigit(c) || c == '.') {
            size_t start = i;
            bool dotSeen = (c == '.');
            i++;
            while (i < len && (std::isdigit(expr[i]) || (!dotSeen && expr[i] == '.'))) {
                if (expr[i] == '.') dotSeen = true;
                i++;
            }
            tokens.push_back({Token::NUMBER, expr.substr(start, i - start)});
            continue;
        }

        // Identifiers (variables or functions)
        if (std::isalpha(c) || c == '_') {
            size_t start = i;
            i++;
            while (i < len && (std::isalnum(expr[i]) || expr[i] == '_')) {
                i++;
            }
            std::string id = expr.substr(start, i - start);
            
            // Look ahead for '(' (skipping spaces) to see if it's a function call
            size_t nextIdx = i;
            while (nextIdx < len && std::isspace(expr[nextIdx])) {
                nextIdx++;
            }
            
            bool isFunc = false;
            if (id == "sqrt" || id == "factorial" || id == "percentage") {
                isFunc = true; 
            } else if (nextIdx < len && expr[nextIdx] == '(') {
                isFunc = true;
            }

            if (isFunc) {
                tokens.push_back({Token::FUNCTION, id});
            } else {
                tokens.push_back({Token::VARIABLE, id});
            }
            continue;
        }

        // Unknown character
        errorMsg = "Unknown character: '" + std::string(1, c) + "'";
        return {};
    }

    return tokens;
}

bool SmartExpressionEvaluator::evaluate(const std::string& expr, std::string& outPostfix, std::string& outAnswer, std::string& outError) {
    std::string err;
    std::vector<Token> tokens = tokenize(expr, err);
    if (!err.empty()) {
        outError = "Tokenization Error: " + err;
        return false;
    }
    if (tokens.empty()) {
        outError = "Expression is empty";
        return false;
    }

    // 1. Infix to Postfix Conversion using Stack
    std::vector<std::string> postfixList;
    std::stack<Token> opStack;

    for (const auto& token : tokens) {
        if (token.type == Token::NUMBER) {
            postfixList.push_back(token.value);
        } else if (token.type == Token::VARIABLE) {
            postfixList.push_back(token.value);
        } else if (token.type == Token::FUNCTION) {
            opStack.push(token);
        } else if (token.type == Token::OPERATOR) {
            std::string o1 = token.value;
            while (!opStack.empty()) {
                Token top = opStack.top();
                if (top.type == Token::FUNCTION) {
                    postfixList.push_back(top.value);
                    opStack.pop();
                } else if (top.type == Token::OPERATOR) {
                    std::string o2 = top.value;
                    int p1 = getPrecedence(o1);
                    int p2 = getPrecedence(o2);
                    if (p2 > p1 || (p2 == p1 && isLeftAssociative(o1))) {
                        postfixList.push_back(o2);
                        opStack.pop();
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
            opStack.push(token);
        } else if (token.type == Token::LPAREN) {
            opStack.push(token);
        } else if (token.type == Token::RPAREN) {
            bool lparenFound = false;
            while (!opStack.empty()) {
                Token top = opStack.top();
                if (top.type == Token::LPAREN) {
                    opStack.pop();
                    lparenFound = true;
                    break;
                } else {
                    postfixList.push_back(top.value);
                    opStack.pop();
                }
            }
            if (!lparenFound) {
                outError = "Mismatched parentheses (missing '(')";
                return false;
            }
            // If top is function, pop it to output
            if (!opStack.empty() && opStack.top().type == Token::FUNCTION) {
                postfixList.push_back(opStack.top().value);
                opStack.pop();
            }
        }
    }

    while (!opStack.empty()) {
        Token top = opStack.top();
        if (top.type == Token::LPAREN || top.type == Token::RPAREN) {
            outError = "Mismatched parentheses";
            return false;
        }
        postfixList.push_back(top.value);
        opStack.pop();
    }

    // Build postfix string representation
    std::string postfixStr;
    for (size_t i = 0; i < postfixList.size(); ++i) {
        if (i > 0) postfixStr += " ";
        postfixStr += postfixList[i];
    }
    outPostfix = postfixStr;

    // 2. Postfix Evaluation using Stack
    std::stack<double> evalStack;

    for (const auto& tokenVal : postfixList) {
        // Check if token is a number literal
        bool isNumeric = false;
        if (isNumber(tokenVal)) {
            evalStack.push(std::stod(tokenVal));
            isNumeric = true;
        }

        if (isNumeric) {
            continue;
        }

        // Check if token is a variable
        if (variables.find(tokenVal) != variables.end()) {
            evalStack.push(variables[tokenVal]);
        }
        // Check if token is a unary operator
        else if (tokenVal == "U-" || tokenVal == "U+") {
            if (evalStack.empty()) {
                outError = "Invalid expression structure (unary operator lacks operand)";
                return false;
            }
            double operand = evalStack.top();
            evalStack.pop();
            double res = applyUnaryOperator(tokenVal, operand, err);
            if (!err.empty()) {
                outError = err;
                return false;
            }
            evalStack.push(res);
        }
        // Check if token is a binary operator
        else if (isOperator(tokenVal)) {
            if (evalStack.size() < 2) {
                outError = "Invalid expression (operator '" + tokenVal + "' requires two operands)";
                return false;
            }
            double right = evalStack.top();
            evalStack.pop();
            double left = evalStack.top();
            evalStack.pop();
            double res = applyOperator(tokenVal, left, right, err);
            if (!err.empty()) {
                outError = err;
                return false;
            }
            evalStack.push(res);
        }
        // Check if token is a function
        else if (tokenVal == "sqrt" || tokenVal == "factorial" || tokenVal == "percentage") {
            if (evalStack.empty()) {
                outError = "Function '" + tokenVal + "' requires an argument";
                return false;
            }
            double arg = evalStack.top();
            evalStack.pop();
            double res = applyFunction(tokenVal, arg, err);
            if (!err.empty()) {
                outError = err;
                return false;
            }
            evalStack.push(res);
        } else {
            outError = "Undefined variable or operand: '" + tokenVal + "'";
            return false;
        }
    }

    if (evalStack.size() != 1) {
        outError = "Invalid expression (operator/operand mismatch)";
        return false;
    }

    outAnswer = formatDouble(evalStack.top());
    return true;
}

double SmartExpressionEvaluator::applyOperator(const std::string& op, double a, double b, std::string& errorMsg) {
    if (op == "+") return a + b;
    if (op == "-") return a - b;
    if (op == "*") return a * b;
    if (op == "/") {
        if (b == 0) {
            errorMsg = "Division by zero";
            return 0;
        }
        return a / b;
    }
    if (op == "%") {
        if (b == 0) {
            errorMsg = "Modulo by zero";
            return 0;
        }
        return std::fmod(a, b);
    }
    if (op == "^") {
        return std::pow(a, b);
    }
    errorMsg = "Unknown operator: " + op;
    return 0;
}

double SmartExpressionEvaluator::applyUnaryOperator(const std::string& op, double a, std::string& errorMsg) {
    if (op == "U-") return -a;
    if (op == "U+") return a;
    errorMsg = "Unknown unary operator: " + op;
    return 0;
}

double SmartExpressionEvaluator::applyFunction(const std::string& func, double arg, std::string& errorMsg) {
    if (func == "sqrt") {
        if (arg < 0) {
            errorMsg = "Square root of a negative number is undefined in real numbers";
            return 0;
        }
        return std::sqrt(arg);
    }
    if (func == "factorial") {
        if (arg < 0) {
            errorMsg = "Factorial of a negative number is undefined";
            return 0;
        }
        int n = static_cast<int>(std::round(arg));
        if (n > 20) {
            errorMsg = "Factorial overflow (maximum input is 20)";
            return 0;
        }
        return static_cast<double>(factorialRecursive(n));
    }
    if (func == "percentage") {
        return arg / 100.0;
    }
    errorMsg = "Unknown function: " + func;
    return 0;
}

// Recursive factorial implementation
unsigned long long SmartExpressionEvaluator::factorialRecursive(int n) {
    if (n <= 1) return 1;
    return n * factorialRecursive(n - 1);
}

// Variable management
void SmartExpressionEvaluator::setVariable(const std::string& name, double value) {
    if (name != "pi" && name != "e") {
        variables[name] = value;
    }
}

void SmartExpressionEvaluator::deleteVariable(const std::string& name) {
    if (name != "pi" && name != "e") {
        variables.erase(name);
    }
}

void SmartExpressionEvaluator::clearVariables() {
    variables.clear();
    variables["pi"] = 3.14159265358979323846;
    variables["e"] = 2.71828182845904523536;
}

std::unordered_map<std::string, double> SmartExpressionEvaluator::getVariables() const {
    return variables;
}

std::string SmartExpressionEvaluator::getVariablesJson() const {
    std::string json = "{";
    bool first = true;
    for (const auto& pair : variables) {
        if (!first) json += ",";
        json += "\"" + pair.first + "\":" + formatDouble(pair.second);
        first = false;
    }
    json += "}";
    return json;
}

void SmartExpressionEvaluator::loadVariablesFromJson(const std::string& jsonStr) {
    variables.clear();
    // Re-initialize default constants
    variables["pi"] = 3.14159265358979323846;
    variables["e"] = 2.71828182845904523536;

    size_t i = 0;
    while (i < jsonStr.length()) {
        if (jsonStr[i] == '"') {
            i++; // skip opening quote
            size_t startName = i;
            while (i < jsonStr.length() && jsonStr[i] != '"') i++;
            std::string name = jsonStr.substr(startName, i - startName);
            if (i < jsonStr.length()) i++; // skip closing quote
            
            // Find colon
            while (i < jsonStr.length() && jsonStr[i] != ':') i++;
            if (i < jsonStr.length()) i++; // skip colon
            
            // Read value
            while (i < jsonStr.length() && (jsonStr[i] == ' ' || jsonStr[i] == '\t')) i++;
            size_t startVal = i;
            while (i < jsonStr.length() && jsonStr[i] != ',' && jsonStr[i] != '}') i++;
            std::string valStr = jsonStr.substr(startVal, i - startVal);
            
            if (!name.empty() && !valStr.empty() && name != "pi" && name != "e") {
                if (isNumber(valStr)) {
                    variables[name] = std::stod(valStr);
                }
            }
        }
        i++;
    }
}

// History management
void SmartExpressionEvaluator::addHistoryRecord(const HistoryRecord& record) {
    history.push_back(record);
}

std::vector<HistoryRecord> SmartExpressionEvaluator::getHistory() const {
    return history;
}

void SmartExpressionEvaluator::deleteHistoryRecord(const std::string& id) {
    auto it = std::remove_if(history.begin(), history.end(), [&id](const HistoryRecord& r) {
        return r.id == id;
    });
    history.erase(it, history.end());
}

void SmartExpressionEvaluator::clearHistory() {
    history.clear();
}

std::string SmartExpressionEvaluator::getHistoryJson() const {
    std::string json = "[";
    bool first = true;
    for (const auto& r : history) {
        if (!first) json += ",";
        json += "{";
        json += "\"id\":\"" + r.id + "\",";
        json += "\"date\":\"" + r.date + "\",";
        json += "\"time\":\"" + r.time + "\",";
        json += "\"expression\":\"" + r.expression + "\",";
        json += "\"postfix\":\"" + r.postfix + "\",";
        json += "\"answer\":\"" + r.answer + "\",";
        json += "\"success\":" + std::string(r.success ? "true" : "false");
        json += "}";
        first = false;
    }
    json += "]";
    return json;
}

void SmartExpressionEvaluator::loadHistoryFromJson(const std::string& jsonStr) {
    history.clear();
    size_t i = 0;
    while (i < jsonStr.length()) {
        if (jsonStr[i] == '{') {
            HistoryRecord r;
            i++;
            while (i < jsonStr.length() && jsonStr[i] != '}') {
                if (jsonStr[i] == '"') {
                    i++; // skip quote
                    size_t startKey = i;
                    while (i < jsonStr.length() && jsonStr[i] != '"') i++;
                    std::string key = jsonStr.substr(startKey, i - startKey);
                    if (i < jsonStr.length()) i++; // skip quote
                    
                    while (i < jsonStr.length() && jsonStr[i] != ':') i++;
                    if (i < jsonStr.length()) i++; // skip colon
                    
                    while (i < jsonStr.length() && (jsonStr[i] == ' ' || jsonStr[i] == '\t')) i++;
                    
                    if (jsonStr[i] == '"') {
                        i++; // skip quote
                        size_t startVal = i;
                        while (i < jsonStr.length() && jsonStr[i] != '"') i++;
                        std::string val = jsonStr.substr(startVal, i - startVal);
                        if (i < jsonStr.length()) i++; // skip quote
                        
                        if (key == "id") r.id = val;
                        else if (key == "date") r.date = val;
                        else if (key == "time") r.time = val;
                        else if (key == "expression") r.expression = val;
                        else if (key == "postfix") r.postfix = val;
                        else if (key == "answer") r.answer = val;
                    } else {
                        // boolean or number
                        size_t startVal = i;
                        while (i < jsonStr.length() && jsonStr[i] != ',' && jsonStr[i] != '}' && jsonStr[i] != ' ') i++;
                        std::string val = jsonStr.substr(startVal, i - startVal);
                        if (key == "success") {
                            r.success = (val == "true");
                        }
                    }
                }
                if (i < jsonStr.length() && jsonStr[i] == '}') {
                    break;
                }
                i++;
            }
            if (!r.id.empty()) {
                history.push_back(r);
            }
        }
        i++;
    }
}
