#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdarg>

enum class DiagLevel {
    Note,
    Warning,
    Error,
    Fatal,
};

inline const char* diag_level_name(DiagLevel level) {
    switch (level) {
        case DiagLevel::Note:    return "note";
        case DiagLevel::Warning: return "warning";
        case DiagLevel::Error:   return "error";
        case DiagLevel::Fatal:   return "fatal error";
    }
    return "unknown";
}

inline std::vector<std::string> split_lines(const std::string& source) {
    std::vector<std::string> lines;
    std::istringstream ss(source);
    std::string line;
    while (std::getline(ss, line))
        lines.push_back(line);
    return lines;
}

inline int edit_distance(const std::string& a, const std::string& b) {
    int m = (int)a.size(), n = (int)b.size();
    std::vector<int> dp(n + 1);
    for (int j = 0; j <= n; j++) dp[j] = j;
    for (int i = 1; i <= m; i++) {
        int prev = dp[0];
        dp[0] = i;
        for (int j = 1; j <= n; j++) {
            int temp = dp[j];
            dp[j] = std::min({prev + (a[i-1] == b[j-1] ? 0 : 1), dp[j] + 1, dp[j-1] + 1});
            prev = temp;
        }
    }
    return dp[n];
}

inline std::string find_closest_match(const std::string& input, const std::vector<std::string>& candidates, int max_dist = 3) {
    int best_dist = 999;
    std::string best;
    for (const auto& c : candidates) {
        int d = edit_distance(input, c);
        if (d < best_dist && d <= max_dist) {
            best_dist = d;
            best = c;
        }
    }
    return best;
}

struct SourceLine {
    int line_no;
    std::string text;
};

struct Diagnostic {
    DiagLevel    level;
    int          line;
    int          column;
    int          end_column;
    std::string  file;
    std::string  message;
    std::string  suggestion;

    void print(const std::vector<SourceLine>* source_lines = nullptr) const {
        const char* color_red   = "\033[31m";
        const char* color_yellow = "\033[33m";
        const char* color_cyan  = "\033[36m";
        const char* color_bold  = "\033[1m";
        const char* color_reset = "\033[0m";

        const char* prefix = "";
        const char* tag = "";
        switch (level) {
            case DiagLevel::Error:
            case DiagLevel::Fatal:   prefix = color_red;   tag = "error";   break;
            case DiagLevel::Warning: prefix = color_yellow; tag = "warning"; break;
            case DiagLevel::Note:    prefix = color_cyan;   tag = "note";    break;
        }

        std::cerr << prefix << color_bold << tag << color_reset;
        if (!file.empty())
            std::cerr << " in " << file;
        if (line > 0) {
            std::cerr << " at " << prefix << color_bold << "line " << line << color_reset;
            if (column > 0)
                std::cerr << ":" << column;
        }
        std::cerr << "\n";

        /* Show source line with caret */
        if (source_lines && line > 0 && line <= (int)source_lines->size()) {
            const auto& src = (*source_lines)[line - 1];
            std::cerr << "  " << src.text << "\n";
            if (column > 0) {
                std::cerr << "  ";
                int col = column - 1;
                for (int i = 0; i < col; i++) std::cerr << ' ';
                int width = (end_column > column) ? (end_column - column) : 1;
                std::cerr << color_red;
                for (int i = 0; i < width; i++) std::cerr << '~';
                std::cerr << color_reset << "\n";
            }
        }

        std::cerr << "  " << prefix << message << color_reset << "\n";
        if (!suggestion.empty()) {
            std::cerr << color_cyan << "  help: " << color_reset
                      << suggestion << "\n";
        }
    }
};

class DiagnosticEngine {
public:
    void set_source(const std::string& source) {
        source_lines_.clear();
        std::istringstream ss(source);
        std::string line;
        int n = 1;
        while (std::getline(ss, line))
            source_lines_.push_back({n++, line});
    }

    void report(DiagLevel level, int line, int column, int end_column,
                const std::string& msg, const std::string& suggestion = "") {
        diagnostics_.push_back({level, line, column, end_column,
                                current_file_, msg, suggestion});
        if (level == DiagLevel::Fatal) {
            diagnostics_.back().print(&source_lines_);
            std::cerr << "\033[1;31mcompilation terminated.\033[0m\n";
            exit(1);
        }
    }

    void report(DiagLevel level, int line, const std::string& msg,
                const std::string& suggestion = "") {
        report(level, line, 0, 0, msg, suggestion);
    }

    void error(int line, const std::string& msg, const std::string& suggestion = "") {
        report(DiagLevel::Error, line, 0, 0, msg, suggestion);
    }

    void error_at(int line, int col, const std::string& msg,
                  const std::string& suggestion = "") {
        report(DiagLevel::Error, line, col, col + 1, msg, suggestion);
    }

    void warn(int line, const std::string& msg, const std::string& suggestion = "") {
        report(DiagLevel::Warning, line, 0, 0, msg, suggestion);
    }

    void note(int line, const std::string& msg) {
        report(DiagLevel::Note, line, 0, 0, msg);
    }

    void set_file(const std::string& file) { current_file_ = file; }

    bool has_errors() const {
        for (auto& d : diagnostics_)
            if (d.level == DiagLevel::Error || d.level == DiagLevel::Fatal)
                return true;
        return false;
    }

    int error_count() const {
        int count = 0;
        for (auto& d : diagnostics_)
            if (d.level == DiagLevel::Error || d.level == DiagLevel::Fatal)
                count++;
        return count;
    }

    void print_all() const {
        for (auto& d : diagnostics_)
            d.print(&source_lines_);
    }

    void clear() {
        diagnostics_.clear();
        source_lines_.clear();
    }

    const std::vector<Diagnostic>& all() const { return diagnostics_; }

private:
    std::vector<Diagnostic> diagnostics_;
    std::vector<SourceLine> source_lines_;
    std::string current_file_;
};

inline DiagnosticEngine& global_diag() {
    static DiagnosticEngine engine;
    return engine;
}

struct CompileError {
    int line;
    std::string message;

    CompileError(const std::string& msg, int ln)
        : line(ln), message(msg) {
        global_diag().error(ln, msg);
    }

    void print() const {
        std::cerr << "error at line " << line << ": " << message << "\n";
    }
};
