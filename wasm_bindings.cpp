// wasm_bindings.cpp
#include "evaluator.hpp"
#include <cstdlib>
#include <cstring>
#include <string>

// Global instance of the evaluator
SmartExpressionEvaluator evaluator;

// Buffer used to hold string responses returned to JavaScript
std::string returnBuffer;

extern "C" {

    // Memory management helpers for JavaScript to write/read strings in WASM memory
    void* wasm_malloc(int size) {
        return std::malloc(size);
    }

    void wasm_free(void* ptr) {
        std::free(ptr);
    }

    // Evaluates a math expression and returns results as a JSON string
    const char* evaluate_expression(const char* expr) {
        std::string postfix;
        std::string answer;
        std::string error;
        
        bool success = evaluator.evaluate(expr, postfix, answer, error);
        
        if (success) {
            returnBuffer = "{\"success\":true,\"postfix\":\"" + postfix + "\",\"answer\":\"" + answer + "\",\"error\":\"\"}";
        } else {
            // Simple escaping for JSON
            std::string escError = "";
            for (char c : error) {
                if (c == '"') escError += "\\\"";
                else if (c == '\\') escError += "\\\\";
                else if (c == '\n') escError += "\\n";
                else escError += c;
            }
            returnBuffer = "{\"success\":false,\"postfix\":\"\",\"answer\":\"\",\"error\":\"" + escError + "\"}";
        }
        
        return returnBuffer.c_str();
    }

    // Variable management bindings
    void set_variable(const char* name, double value) {
        evaluator.setVariable(name, value);
    }

    void delete_variable(const char* name) {
        evaluator.deleteVariable(name);
    }

    void clear_variables() {
        evaluator.clearVariables();
    }

    const char* get_variables_json() {
        returnBuffer = evaluator.getVariablesJson();
        return returnBuffer.c_str();
    }

    void load_variables_from_json(const char* jsonStr) {
        evaluator.loadVariablesFromJson(jsonStr);
    }

    // History bindings
    void add_history_record(const char* id, const char* date, const char* time, 
                            const char* expression, const char* postfix, 
                            const char* answer, bool success) {
        HistoryRecord record;
        record.id = id;
        record.date = date;
        record.time = time;
        record.expression = expression;
        record.postfix = postfix;
        record.answer = answer;
        record.success = success;
        evaluator.addHistoryRecord(record);
    }

    void delete_history_record(const char* id) {
        evaluator.deleteHistoryRecord(id);
    }

    void clear_history() {
        evaluator.clearHistory();
    }

    const char* get_history_json() {
        returnBuffer = evaluator.getHistoryJson();
        return returnBuffer.c_str();
    }

    void load_history_from_json(const char* jsonStr) {
        evaluator.loadHistoryFromJson(jsonStr);
    }
}
