#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include <map>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>

using namespace std;

class Term
{
public:
    virtual int calcValue(const map<string, int> &vars) = 0;
    virtual int degree() = 0;
    virtual ~Term() {}
};

struct TermParseInfo
{
    Term *term;
    bool hasParens;
};

class NumTerm : public Term
{
public:
    int val;
    NumTerm(int v) : val(v) {}
    int calcValue(const map<string, int> &) override { return val; }
    int degree() override { return 0; }
};

class IdTerm : public Term
{
public:
    string var;
    IdTerm(const string &v) : var(v) {}
    int calcValue(const map<string, int> &vars) override
    {
        auto it = vars.find(var);
        return it != vars.end() ? it->second : 0;
    }
    int degree() override { return 1; }
};

class BinTerm : public Term
{
public:
    char opr;
    Term *lhs;
    Term *rhs;
    BinTerm(char o, Term *l, Term *r) : opr(o), lhs(l), rhs(r) {}
    int calcValue(const map<string, int> &vars) override
    {
        int leftVal = lhs->calcValue(vars);
        int rightVal = rhs->calcValue(vars);
        return opr == '+' ? leftVal + rightVal : leftVal - rightVal;
    }
    int degree() override { return max(lhs->degree(), rhs->degree()); }
    ~BinTerm()
    {
        delete lhs;
        delete rhs;
    }
};

class MultTerm : public Term
{
public:
    vector<Term *> components;
    MultTerm(const vector<Term *> &comps) : components(comps) {}
    int calcValue(const map<string, int> &vars) override
    {
        int result = 1;
        for (auto *comp : components)
            result *= comp->calcValue(vars);
        return result;
    }
    int degree() override
    {
        int deg = 0;
        for (auto *comp : components)
            deg += comp->degree();
        return deg;
    }
    ~MultTerm()
    {
        for (auto *comp : components)
            delete comp;
    }
};

class ExpTerm : public Term
{
public:
    Term *baseExpr;
    int expVal;
    ExpTerm(Term *b, int e) : baseExpr(b), expVal(e) {}
    int calcValue(const map<string, int> &vars) override
    {
        int base = baseExpr->calcValue(vars);
        int result = 1;
        for (int i = 0; i < expVal; ++i)
            result *= base;
        return result;
    }
    int degree() override { return baseExpr->degree() * expVal; }
    ~ExpTerm() { delete baseExpr; }
};

struct Arg
{
    enum ArgType
    {
        CONST,
        VAR,
        POLY_EVAL
    } type;
    int const_val;
    string var_name;
    string poly_name;
    vector<Arg *> poly_args;
    int line_no; // Line number information

    Arg() : type(CONST), const_val(0), line_no(0) {}
    ~Arg()
    {
        for (auto *arg : poly_args)
            delete arg;
    }
};

struct PolyEval
{
    string poly_name;
    vector<Arg *> args;
    int line_no;
    ~PolyEval()
    {
        for (auto *arg : args)
            delete arg;
    }
};

struct Instruction
{
    enum InstrType
    {
        Set,
        Get,
        Put
    } instrType;
    int linePos;
    string targetVar;
    vector<string> sourceVars;
    PolyEval *poly_eval = nullptr;
};

class Parser
{
public:
    Parser();
    void runParse();

private:
    LexicalAnalyzer lexer;
    map<string, vector<string>> poly_param_list;
    map<string, vector<int>> poly_def_locations;
    vector<string> poly_def_order;
    map<string, Term *> poly_formulas;
    vector<int> invalid_var_lines;
    vector<int> repeat_lines;
    vector<int> memory_space;
    map<string, int> variable_slots;
    int next_slot;
    vector<int> input_data;
    int input_offset;
    vector<int> job_list;
    vector<int> unknown_poly_lines;
    vector<int> param_count_errors;
    vector<int> unset_var_alerts;
    map<string, bool> var_initialized;
    string current_poly_name;
    vector<function<void()>> exec_tasks;
    vector<Instruction> instructions;

    void flagSyntaxError();
    Token verifyToken(TokenType expected);
    int allocVarSlot(const string &var);
    int retrieveInput();
    int determineDegree(Term *term);

    void parseJobs();
    void parseJobNumbers();
    void parsePolyDefs();
    void parsePolyList();
    void parseSinglePoly();
    void parseParams(vector<string> &params);
    Term *parsePolyFormula(const string &polyName);
    Term *parseFormula();
    Term *parseMinusRight(Term *left);
    TermParseInfo parseMultFactors();
    TermParseInfo parseMonomialList();
    TermParseInfo parseSingleFactor();
    TermParseInfo parseAtomicTerm();
    int parseExponent();

    void parseExecBlock();
    void parseInstrSequence();
    void parseOneInstr();
    void parseGetInstr();
    void parsePutInstr();
    void parseSetInstr();
    void executeGetInstr(string var_name);
    void executePutInstr(string var_name);
    void executeSetInstr(string var_name, PolyEval *pe);
    void collectUsedVars(Arg *arg, vector<string> &used);
    PolyEval *parsePolyExec();
    vector<Arg *> parseExecArgs();
    Arg *parseArgExpr();
    int evaluateArg(Arg *arg);

    void parseInputBlock();
    void parseInputValues();

    void performJobs();
    void detectUnusedSets();
};

#endif // PARSER_H
