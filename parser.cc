#include "parser.h"
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <fstream>

using namespace std;

Parser::Parser() : lexer(), next_slot(0), input_offset(0)
{
    // Initialize memory with 1000 cells set to 0.
    memory_space.resize(1000, 0);
}

void Parser::flagSyntaxError()
{
    cout << "SYNTAX ERROR !!!!!&%!!\n";
    exit(1);
}

Token Parser::verifyToken(TokenType expected)
{
    Token token = lexer.GetToken();
    if (token.token_type != expected)
        flagSyntaxError();
    return token;
}

int Parser::allocVarSlot(const string &variable)
{
    if (variable_slots.find(variable) == variable_slots.end())
    {
        if (next_slot >= 1000)
        {
            cerr << "Memory overflow\n";
            exit(1);
        }
        variable_slots[variable] = next_slot++;
    }
    return variable_slots[variable];
}

int Parser::retrieveInput()
{
    return (input_offset < input_data.size()) ? input_data[input_offset++] : 0;
}

int Parser::determineDegree(Term *term)
{
    return term->degree();
}

void Parser::runParse()
{
    parseJobs(); // Process TASKS section.
    if (lexer.peek(1).token_type != POLY)
        flagSyntaxError(); // POLY expected after TASKS.

    parsePolyDefs(); // Process polynomial definitions.
    if (lexer.peek(1).token_type != EXECUTE)
        flagSyntaxError(); // EXECUTE expected after POLY.

    parseExecBlock();
    if (lexer.peek(1).token_type != INPUTS)
        flagSyntaxError(); // INPUTS expected after EXECUTE.

    parseInputBlock(); // Process the input block.
    if (lexer.peek(1).token_type != END_OF_FILE)
        flagSyntaxError(); // No extra tokens allowed after INPUTS.

    // Gather duplicate polynomial declaration lines for semantic checking.
    for (const auto &entry : poly_def_locations)
    {
        const std::string &name = entry.first;
        const std::vector<int> &locs = entry.second;
        if (locs.size() > 1)
            repeat_lines.insert(repeat_lines.end(), locs.begin() + 1, locs.end());
    }

    // Check if task 1 is present before printing semantic errors.
    bool task1_present = (find(job_list.begin(), job_list.end(), 1) != job_list.end());

    if (task1_present)
    {
        // Semantic Error Code 1: Duplicate polynomial declarations.
        if (!repeat_lines.empty())
        {
            sort(repeat_lines.begin(), repeat_lines.end());
            cout << "Semantic Error Code 1:";
            for (int line : repeat_lines)
                cout << " " << line;
            cout << endl;
            return;
        }

        // Semantic Error Code 2: Invalid monomial names.
        if (!invalid_var_lines.empty())
        {
            sort(invalid_var_lines.begin(), invalid_var_lines.end());
            cout << "Semantic Error Code 2:";
            for (int line : invalid_var_lines)
                cout << " " << line;
            cout << endl;
            return;
        }

        // Semantic Error Code 3: Evaluation of undeclared polynomials.
        if (!unknown_poly_lines.empty())
        {
            sort(unknown_poly_lines.begin(), unknown_poly_lines.end());
            cout << "Semantic Error Code 3:";
            for (int line : unknown_poly_lines)
                cout << " " << line;
            cout << endl;
            return;
        }

        // Semantic Error Code 4: Incorrect number of arguments.
        if (!param_count_errors.empty())
        {
            sort(param_count_errors.begin(), param_count_errors.end());
            cout << "Semantic Error Code 4:";
            for (int line : param_count_errors)
                cout << " " << line;
            cout << endl;
            return;
        }
    }

    performJobs();

    // Free dynamically allocated polynomial formulas.
    for (auto &entry : poly_formulas)
    {
        delete entry.second;
    }

    for (auto &instr : instructions)
        if (instr.poly_eval)
            delete instr.poly_eval;
}

void Parser::parseJobs()
{
    verifyToken(TASKS);
    if (lexer.peek(1).token_type != NUM)
        flagSyntaxError(); // TASKS must be followed by a number.
    parseJobNumbers();
}

void Parser::parseJobNumbers()
{
    Token num = verifyToken(NUM);
    job_list.push_back(atoi(num.lexeme.c_str()));
    if (lexer.peek(1).token_type == NUM)
        parseJobNumbers();
    else if (lexer.peek(1).token_type != POLY)
        flagSyntaxError();
}

void Parser::parsePolyDefs()
{
    verifyToken(POLY);
    parsePolyList();
}

void Parser::parsePolyList()
{
    if (lexer.peek(1).token_type != ID)
        flagSyntaxError(); // POLY must be followed by an identifier.
    parseSinglePoly();
    if (lexer.peek(1).token_type == ID)
        parsePolyList();
    else if (lexer.peek(1).token_type != EXECUTE)
        flagSyntaxError();
}

void Parser::parseSinglePoly()
{
    Token polyId = verifyToken(ID);
    vector<string> params;
    if (lexer.peek(1).token_type == LPAREN)
    {
        verifyToken(LPAREN);
        parseParams(params);
        verifyToken(RPAREN);
    }
    else
    {
        params.push_back("x");
    }
    poly_param_list[polyId.lexeme] = params;
    poly_def_order.push_back(polyId.lexeme);
    poly_def_locations[polyId.lexeme].push_back(polyId.line_no);
    verifyToken(EQUAL);
    current_poly_name = polyId.lexeme;
    Term *formula = parsePolyFormula(polyId.lexeme);
    poly_formulas[polyId.lexeme] = formula;
    verifyToken(SEMICOLON);
}

void Parser::parseParams(vector<string> &params)
{
    Token param = verifyToken(ID);
    params.push_back(param.lexeme);
    if (lexer.peek(1).token_type == COMMA)
    {
        verifyToken(COMMA);
        parseParams(params);
    }
}

Term *Parser::parsePolyFormula(const string &)
{
    return parseFormula();
}

Term *Parser::parseFormula()
{
    // Parse the initial term.
    Term *expr = parseMultFactors().term;

    // Process additional terms with the '+' operator.
    while (lexer.peek(1).token_type == PLUS)
    {
        Token op = lexer.GetToken(); // Consume '+'
        Term *right = parseMultFactors().term;
        expr = new BinTerm('+', expr, right);
    }

    // Handle '-' operator.
    if (lexer.peek(1).token_type == MINUS)
    {
        expr = parseMinusRight(expr);
    }

    return expr;
}

Term *Parser::parseMinusRight(Term *left)
{
    // Consume '-' and parse the rest of the expression.
    Token minusToken = lexer.GetToken();
    Term *right = parseFormula();
    return new BinTerm('-', left, right);
}

TermParseInfo Parser::parseMultFactors()
{
    Token t = lexer.peek(1);
    TermParseInfo res;
    if (t.token_type == NUM)
    {
        Token coeff = lexer.GetToken();
        int coeff_val = atoi(coeff.lexeme.c_str());
        Term *coeff_term = new NumTerm(coeff_val);
        Token next = lexer.peek(1);
        if (next.token_type == ID || next.token_type == LPAREN)
        {
            TermParseInfo mon_list = parseMonomialList();
            res.term = new MultTerm({coeff_term, mon_list.term});
        }
        else
        {
            res.term = coeff_term;
        }
        res.hasParens = false;
    }
    else if (t.token_type == ID || t.token_type == LPAREN)
    {
        res = parseMonomialList();
    }
    else
    {
        flagSyntaxError();
    }
    return res;
}

TermParseInfo Parser::parseMonomialList()
{
    TermParseInfo res = parseSingleFactor();
    vector<Term *> factors = {res.term};
    while (lexer.peek(1).token_type == ID || lexer.peek(1).token_type == LPAREN)
    {
        TermParseInfo next = parseSingleFactor();
        factors.push_back(next.term);
    }
    res.term = (factors.size() > 1) ? new MultTerm(factors) : factors[0];
    res.hasParens = false;
    return res;
}

TermParseInfo Parser::parseSingleFactor()
{
    TermParseInfo res = parseAtomicTerm();
    if (lexer.peek(1).token_type == POWER)
    {
        lexer.GetToken();
        Token exp = verifyToken(NUM);
        int exp_val = atoi(exp.lexeme.c_str());
        res.term = new ExpTerm(res.term, exp_val);
    }
    return res;
}

TermParseInfo Parser::parseAtomicTerm()
{
    TermParseInfo res;
    Token tok = lexer.peek(1);
    if (tok.token_type == NUM)
    {
        tok = lexer.GetToken();
        res.term = new NumTerm(atoi(tok.lexeme.c_str()));
    }
    else if (tok.token_type == ID)
    {
        tok = lexer.GetToken();
        if (find(poly_param_list[current_poly_name].begin(), poly_param_list[current_poly_name].end(), tok.lexeme) ==
            poly_param_list[current_poly_name].end())
        {
            invalid_var_lines.push_back(tok.line_no);
        }
        res.term = new IdTerm(tok.lexeme);
    }
    else if (tok.token_type == LPAREN)
    {
        lexer.GetToken();
        res.term = parseFormula();
        verifyToken(RPAREN);
        res.hasParens = true;
    }
    else
    {
        flagSyntaxError();
    }
    res.hasParens = false;
    return res;
}

int Parser::parseExponent()
{
    Token num = verifyToken(NUM);
    return atoi(num.lexeme.c_str());
}

void Parser::parseExecBlock()
{
    Token exec_token = verifyToken(EXECUTE);
    if (lexer.peek(1).token_type != INPUT && lexer.peek(1).token_type != OUTPUT && lexer.peek(1).token_type != ID)
        flagSyntaxError(); // EXECUTE must be followed by a valid statement.
    parseInstrSequence();
}

void Parser::parseInstrSequence()
{
    parseOneInstr();
    Token next = lexer.peek(1);
    if (next.token_type == INPUT || next.token_type == OUTPUT || next.token_type == ID)
        parseInstrSequence();
    else if (next.token_type != INPUTS)
        flagSyntaxError();
}

void Parser::parseOneInstr()
{
    Token next = lexer.peek(1);
    if (next.token_type == INPUT)
        parseGetInstr();
    else if (next.token_type == OUTPUT)
        parsePutInstr();
    else if (next.token_type == ID)
        parseSetInstr();
    else
        flagSyntaxError();
}

void Parser::parseGetInstr()
{
    verifyToken(INPUT);
    Token var = verifyToken(ID);
    Instruction instr;
    instr.instrType = Instruction::Get;
    instr.linePos = var.line_no;
    instr.targetVar = var.lexeme;

    instructions.push_back(instr);
    exec_tasks.push_back(bind(&Parser::executeGetInstr, this, var.lexeme));
    verifyToken(SEMICOLON);
}

void Parser::parsePutInstr()
{
    verifyToken(OUTPUT);
    Token var = verifyToken(ID);
    Instruction instr;
    instr.instrType = Instruction::Put;
    instr.linePos = var.line_no;
    instr.sourceVars.push_back(var.lexeme);

    instructions.push_back(instr);
    exec_tasks.push_back(bind(&Parser::executePutInstr, this, var.lexeme));
    verifyToken(SEMICOLON);
}

void Parser::parseSetInstr()
{
    Token var = verifyToken(ID);
    verifyToken(EQUAL);
    PolyEval *pe = parsePolyExec();
    Instruction instr;
    instr.instrType = Instruction::Set;
    instr.linePos = var.line_no;
    instr.targetVar = var.lexeme;
    instr.poly_eval = pe;

    instructions.push_back(instr);
    exec_tasks.push_back(bind(&Parser::executeSetInstr, this, var.lexeme, pe));
    verifyToken(SEMICOLON);
}

PolyEval *Parser::parsePolyExec()
{
    PolyEval *pe = new PolyEval();
    Token polyId = verifyToken(ID);
    pe->poly_name = polyId.lexeme;
    pe->line_no = polyId.line_no;
    if (poly_def_locations.find(pe->poly_name) == poly_def_locations.end())
    {
        unknown_poly_lines.push_back(pe->line_no);
    }
    verifyToken(LPAREN);
    pe->args = parseExecArgs();
    verifyToken(RPAREN);
    if (poly_param_list.count(pe->poly_name) && pe->args.size() != poly_param_list[pe->poly_name].size())
    {
        param_count_errors.push_back(pe->line_no);
    }
    return pe;
}

vector<Arg *> Parser::parseExecArgs()
{
    vector<Arg *> args;
    if (lexer.peek(1).token_type == RPAREN)
        flagSyntaxError();
    args.push_back(parseArgExpr());
    while (lexer.peek(1).token_type == COMMA)
    {
        verifyToken(COMMA);
        args.push_back(parseArgExpr());
    }
    return args;
}

Arg *Parser::parseArgExpr()
{
    Arg *arg = new Arg();
    Token tok = lexer.peek(1);
    if (tok.token_type == NUM)
    {
        tok = lexer.GetToken();
        arg->type = Arg::CONST;
        arg->const_val = atoi(tok.lexeme.c_str());
        arg->line_no = tok.line_no;
    }
    else if (tok.token_type == ID)
    {
        Token nextTok = lexer.peek(2);
        if (nextTok.token_type == LPAREN)
        {
            tok = lexer.GetToken();
            arg->type = Arg::POLY_EVAL;
            arg->poly_name = tok.lexeme;
            arg->line_no = tok.line_no;
            if (poly_def_locations.find(arg->poly_name) == poly_def_locations.end())
            {
                unknown_poly_lines.push_back(tok.line_no);
            }
            verifyToken(LPAREN);
            arg->poly_args = parseExecArgs();
            verifyToken(RPAREN);
            if (poly_param_list.count(arg->poly_name) &&
                arg->poly_args.size() != poly_param_list[arg->poly_name].size())
            {
                param_count_errors.push_back(tok.line_no);
            }
        }
        else
        {
            tok = lexer.GetToken();
            arg->type = Arg::VAR;
            arg->var_name = tok.lexeme;
            arg->line_no = tok.line_no;
        }
    }
    else
    {
        flagSyntaxError();
    }
    return arg;
}

void Parser::parseInputBlock()
{
    verifyToken(INPUTS);
    if (lexer.peek(1).token_type != NUM)
        flagSyntaxError();
    parseInputValues();
}

void Parser::parseInputValues()
{
    Token num = verifyToken(NUM);
    input_data.push_back(atoi(num.lexeme.c_str()));
    if (lexer.peek(1).token_type == NUM)
        parseInputValues();
    else if (lexer.peek(1).token_type != END_OF_FILE)
        flagSyntaxError();
}

void Parser::performJobs()
{
    vector<int> tasks = job_list;
    sort(tasks.begin(), tasks.end());
    tasks.erase(unique(tasks.begin(), tasks.end()), tasks.end());

    bool task2_listed = (find(tasks.begin(), tasks.end(), 2) != tasks.end());
    stringstream task2_output;
    streambuf *old_cout = nullptr;
    if (task2_listed)
    {
        old_cout = cout.rdbuf(task2_output.rdbuf());
    }
    else
    {
        static ofstream nullStream("/dev/null");
        old_cout = cout.rdbuf(nullStream.rdbuf());
    }

    for (auto &task : exec_tasks)
        task();

    cout.rdbuf(old_cout);
    if (task2_listed)
    {
        cout << task2_output.str();
    }

    for (int task : tasks)
    {
        if (task == 1 || task == 2)
            continue;
        if (task == 3 && !unset_var_alerts.empty())
        {
            sort(unset_var_alerts.begin(), unset_var_alerts.end());
            cout << "Warning Code 1:";
            for (int line : unset_var_alerts)
                cout << " " << line;
            cout << endl;
        }
        if (task == 4)
            detectUnusedSets();
        if (task == 5)
        {
            for (const auto &poly : poly_def_order)
            {
                cout << poly << ": " << determineDegree(poly_formulas[poly]) << endl;
            }
        }
    }
}

void Parser::detectUnusedSets()
{
    map<string, bool> live;
    vector<int> unusedLines;
    for (int i = instructions.size() - 1; i >= 0; --i)
    {
        const auto &instr = instructions[i];
        if (instr.instrType == Instruction::Put)
        {
            for (const auto &var : instr.sourceVars)
            {
                live[var] = true;
            }
        }
        else if (instr.instrType == Instruction::Get)
        {
            live[instr.targetVar] = false;
        }
        else if (instr.instrType == Instruction::Set)
        {
            if (!live[instr.targetVar])
            {
                unusedLines.push_back(instr.linePos);
            }
            live[instr.targetVar] = false;
            for (const auto &var : instr.sourceVars)
            {
                live[var] = true;
            }
        }
    }
    if (!unusedLines.empty())
    {
        sort(unusedLines.begin(), unusedLines.end());
        cout << "Warning Code 2:";
        for (int line : unusedLines)
        {
            cout << " " << line;
        }
        cout << endl;
    }
}

void Parser::executeGetInstr(string var_name)
{
    int value = retrieveInput();
    int slot = allocVarSlot(var_name);
    memory_space[slot] = value;
    var_initialized[var_name] = true;
}

void Parser::executePutInstr(string var_name)
{
    int slot = allocVarSlot(var_name);
    cout << memory_space[slot] << endl;
}

int Parser::evaluateArg(Arg *arg)
{
    if (arg->type == Arg::CONST)
        return arg->const_val;
    if (arg->type == Arg::VAR)
    {
        if (!var_initialized[arg->var_name])
            unset_var_alerts.push_back(arg->line_no);
        return memory_space[allocVarSlot(arg->var_name)];
    }
    if (arg->type == Arg::POLY_EVAL)
    {
        vector<int> arg_values;
        for (auto *sub_arg : arg->poly_args)
            arg_values.push_back(evaluateArg(sub_arg));
        map<string, int> local_env;
        const auto &params = poly_param_list[arg->poly_name];
        for (size_t i = 0; i < params.size(); ++i)
            local_env[params[i]] = arg_values[i];
        return poly_formulas[arg->poly_name]->calcValue(local_env);
    }
    return 0;
}

void Parser::executeSetInstr(string var_name, PolyEval *pe)
{
    vector<int> arg_values;
    vector<string> vars_used;
    for (auto *arg : pe->args)
    {
        int val = evaluateArg(arg);
        arg_values.push_back(val);
        collectUsedVars(arg, vars_used);
    }
    map<string, int> local_env;
    const auto &params = poly_param_list[pe->poly_name];
    for (size_t i = 0; i < params.size(); ++i)
    {
        local_env[params[i]] = arg_values[i];
    }
    int value = poly_formulas[pe->poly_name]->calcValue(local_env);
    int slot = allocVarSlot(var_name);
    memory_space[slot] = value;
    var_initialized[var_name] = true;

    for (auto &instr : instructions)
    {
        if (instr.instrType == Instruction::Set && instr.targetVar == var_name && instr.linePos == pe->line_no)
        {
            instr.sourceVars = vars_used;
            break;
        }
    }
}

void Parser::collectUsedVars(Arg *arg, vector<string> &used)
{
    if (arg->type == Arg::VAR)
    {
        used.push_back(arg->var_name);
    }
    else if (arg->type == Arg::POLY_EVAL)
    {
        for (auto *sub_arg : arg->poly_args)
        {
            collectUsedVars(sub_arg, used);
        }
    }
}

int main()
{
    Parser parser;
    parser.runParse();
    return 0;
}
