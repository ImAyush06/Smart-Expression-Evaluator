// evaluator.hpp
#ifndef EVALUATOR_HPP
#define EVALUATOR_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <stack>

struct HistoryRecord {
    std::string id;
    std::string date;
    std::string time;
    std::string expression;
    std::string postfix;
    std::string answer;
    bool success;
};

class SmartExpressionEvaluator {
private:
    std::unordered_map<std::string, double> variables;
    std::vector<HistoryRecord> history;

    // Internal helper structures and methods
    struct Token {
        enum Type {
            NUMBER,
            VARIABLE,
            FUNCTION,
            OPERATOR,
            LPAREN,
            RPAREN
        };
        Type type;
        std::string value;
    };

    std::vector<Token> tokenize(const std::string& expr, std::string& errorMsg);
    bool isOperator(char c) const;
    bool isOperator(const std::string& op) const;
    int getPrecedence(const std::string& op) const;
    bool isLeftAssociative(const std::string& op) const;
    double applyOperator(const std::string& op, double a, double b, std::string& errorMsg);
    double applyUnaryOperator(const std::string& op, double a, std::string& errorMsg);
    double applyFunction(const std::string& func, double arg, std::string& errorMsg);
    unsigned long long factorialRecursive(int n); // Recursion requirement

public:
    SmartExpressionEvaluator();

    // Expression evaluation
    bool evaluate(const std::string& expr, std::string& outPostfix, std::string& outAnswer, std::string& outError);

    // Variable management
    void setVariable(const std::string& name, double value);
    void deleteVariable(const std::string& name);
    void clearVariables();
    std::unordered_map<std::string, double> getVariables() const;
    std::string getVariablesJson() const;
    void loadVariablesFromJson(const std::string& jsonStr);

    // History management
    void addHistoryRecord(const HistoryRecord& record);
    std::vector<HistoryRecord> getHistory() const;
    void deleteHistoryRecord(const std::string& id);
    void clearHistory();
    std::string getHistoryJson() const;
    void loadHistoryFromJson(const std::string& jsonStr);
};

#endif // EVALUATOR_HPP
