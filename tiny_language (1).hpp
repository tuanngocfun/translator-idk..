#ifndef TINY_LANGUAGE_HPP_INCLUDED
#define TINY_LANGUAGE_HPP_INCLUDED

#include <iostream>
#include <exception>
#include <string>
#include <cctype>
#include <fstream>
#include <cstdio>
#include <set>

namespace TINY
{
// Template class to present errors
template<int N>
class Error : std::exception
{
 protected:
    std::string msg;

 public:
    explicit Error(const std::string& msg) : msg(msg) {}

    const char* what() const noexcept
    {
        return msg.c_str();
    }

    friend std::ostream& operator<<(std::ostream& out, const Error& er)
    {
        out << er.msg;
        return out;
    }
};

// Two class instances of Error
using Lexical_Error = Error<0>;
using Syntax_Error = Error<1>;

class Translator
{
 // Enum class to present specific tokens
 enum class Token : char {
    ID, STRING, NUM,

    ASSIGNMENT_SYMBOL, PLUS_SYMBOL, MINUS_SYMBOL, MUL_SYMBOL, DIV_SYMBOL, MOD_SYMBOL,
    GREATER_SYMBOL, LESS_SYMBOL, EQUAL_SYMBOL, GREATER_EQUAL_SYMBOL, LESS_EQUAL_SYMBOL,

    BEGIN_LITERAL, END_LITERAL, PRINT_LITERAL, INPUT_LITERAL, LET_LITERAL,
    IF_LITERAL, ENDIF_LITERAL, WHILE_LITERAL, REPEAT_LITERAL, ENDWHILE_LITERAL,

    NEWLINE, EOFSTREAM,

    // Extra literals I add myself
    ELSE_LITERAL, ELSEIF_LITERAL
 };

 private:
    // Class to look for next characters in stream and transfer them to tokens
    class Lexer
    {
     protected:
        // I/O stream to manipulate
        std::iostream* p_stream;
        // Flag to check if a Lexer owns its manipulated stream or the stream is a reference (which in case, the ownership do not belong to the Lexer)
        bool owns_stream;

        // Data member to hold the currently processed token
        Token cur_token;

        std::string soft_buffer; // Buffer to store text only, ignore whitespace
        std::string hard_buffer; // Buffer to store every read characters, including whitespace. Mainly used to move the stream back to the previous state

     public:
        explicit Lexer(std::iostream& instream) : p_stream(&instream), owns_stream(false) {}
        explicit Lexer(std::iostream* instream) : p_stream(instream), owns_stream(true) {}

        // Each Lexer should work with its own stream
        Lexer(const Lexer&) = delete;
        Lexer(Lexer&&) = delete;

        virtual ~Lexer() { if(owns_stream){ delete p_stream; owns_stream = false; } }

        // Get currently processed token
        Token get_current_token() const { return cur_token; }
        // Get currently processed text saved in soft_buffer
        std::string get_current_text() const { return soft_buffer; }

        // Method to advance the lexer to handle the next token in the stream with optional newline_check: True to also handle newline or False to skip newline
        void advance(bool newline_check) { cur_token = get_token(newline_check); }
        // Convenient overloading when newline is ignored
        void advance() { advance(false); }

        // Method to move back the stream to its previous state. cur_tok and buffer stay the same
        void move_back()
        {
            for(auto r_iter = hard_buffer.rbegin(); r_iter != hard_buffer.rend(); r_iter++)
                p_stream->putback(*r_iter);
        }

     private:
        // Main method to process next characters in stream
        Token get_token(bool newline_check)
        {
            // Lambda expression presents the condition to skip all whitespace characters (except newline if attempt to check for newline)
            auto cond = [&](char c) -> bool
            {
                if(newline_check)
                {
                    return std::isspace(c) && c != '\n';
                }
                else
                {
                    return std::isspace(c);
                }
            };

            std::iostream& input = *p_stream;
            soft_buffer.clear();
            hard_buffer.clear();
            // If there are no characters or the input stream is not in good state, return EOFSTREAM immediately
            if(input.eof() || !input.good())
                return Token::EOFSTREAM;

            char c = input.get();

            while(cond(c) && !input.eof())
            {
                hard_buffer += c;
                c = input.get();
            }

            // If there are no characters, return EOFSTREAM
            if(input.eof() || !input.good())
                return Token::EOFSTREAM;

            // The case where the read character is \n when attempt to check for newline, return NEWLINE immediately
            if(c == '\n' && newline_check)
            {
                hard_buffer += c;
                return Token::NEWLINE;
            }

            // Next, we look for an identifier or literal (a special case is MOD literal since it is actually a symbol). Both start with a letter.
            // We handle this by looking for an identifier first. Then, we check what we have found against the literals.

            else if(std::isalpha(c))
            {
                soft_buffer += c;
                hard_buffer += c;
                c = input.get();

                // Look for zero or more letters or digits
                while(std::isalnum(c))
                {
                    soft_buffer += c;
                    hard_buffer += c;
                    c = input.get();
                }

                // The current character doesn't belong to our identifier/function name, and must be put back into the stream.
                input.putback(c);

                if(soft_buffer == "BEGIN")
                    return Token::BEGIN_LITERAL;
                else if(soft_buffer == "END")
                    return Token::END_LITERAL;
                else if(soft_buffer == "PRINT")
                    return Token::PRINT_LITERAL;
                else if(soft_buffer == "INPUT")
                    return Token::INPUT_LITERAL;
                else if(soft_buffer == "LET")
                    return Token::LET_LITERAL;
                else if(soft_buffer == "IF")
                    return Token::IF_LITERAL;
                else if(soft_buffer == "ELSEIF")
                    return Token::ELSEIF_LITERAL;
                else if(soft_buffer == "ELSE")
                    return Token::ELSE_LITERAL;
                else if(soft_buffer == "ENDIF")
                    return Token::ENDIF_LITERAL;
                else if(soft_buffer == "WHILE")
                    return Token::WHILE_LITERAL;
                else if(soft_buffer == "REPEAT")
                    return Token::REPEAT_LITERAL;
                else if(soft_buffer == "ENDWHILE")
                    return Token::ENDWHILE_LITERAL;
                else if(soft_buffer == "mod") // Special case where mod operator is used
                    return Token::MOD_SYMBOL;
                else
                    return Token::ID;
            }

            // Look for a number
            else if(std::isdigit(c) || c == '.')
            {
                // Number of form: n or n.m
                if(std::isdigit(c))
                {
                    soft_buffer += c;
                    hard_buffer += c;
                    c = input.get();
                    while(std::isdigit(c))
                    {
                        soft_buffer += c;
                        hard_buffer += c;
                        c = input.get();
                    }
                    // If the number is a floating point number
                    if(c == '.')
                    {
                        soft_buffer += c;
                        hard_buffer += c;
                        c = input.get();
                        while(std::isdigit(c))
                        {
                            soft_buffer += c;
                            hard_buffer += c;
                            c = input.get();
                        }
                    }
                }
                // Number of form: .m
                else
                {
                    soft_buffer += c;
                    hard_buffer += c;
                    c = input.get();
                    if(!std::isdigit(c))
                    {
                        throw Lexical_Error{"no digits after decimal point"};
                    }

                    while(std::isdigit(c))
                    {
                        soft_buffer += c;
                        hard_buffer += c;
                        c = input.get();
                    }
                }

                // Now handle exponent part (if any)
                // Form of exponent part: E/e+-N (E/e is scientific symbol, +- for positive or negative power (optional), N is the power)
                if(c == 'E' || c == 'e')
                {
                    soft_buffer += c;
                    hard_buffer += c;
                    c = input.get();
                    if(c == '+' || c == '-')
                    {
                        soft_buffer += c;
                        hard_buffer += c;
                        c = input.get();
                    }

                    if(!std::isdigit(c))
                    {
                        throw Lexical_Error{"no digits in exponent part"};
                    }

                    while(std::isdigit(c))
                    {
                        soft_buffer += c;
                        hard_buffer += c;
                        c = input.get();
                    }
                }

                // The current character doesn't belong to our number, and must be put back into the stream.
                input.putback(c);
                return Token::NUM;
            }

            // Now, current character is a symbol

            soft_buffer += c;
            hard_buffer += c;

            // Case '>'
            if(c == '>')
            {
                // Check if the next character is '=', we also take it
                char temp = input.get();
                if(temp == '=')
                {
                    soft_buffer += temp;
                    hard_buffer += temp;
                    return Token::GREATER_EQUAL_SYMBOL;
                }
                // If not, we return this character back to the input and return corresponding value
                else
                {
                    input.putback(temp);
                    return Token::GREATER_SYMBOL;
                }
            }
            // Case '<'
            else if(c == '<')
            {
                // Check if the next character is '=', we also take it
                char temp = input.get();
                if(temp == '=')
                {
                    soft_buffer += temp;
                    hard_buffer += temp;
                    return Token::LESS_EQUAL_SYMBOL;
                }
                // If not, we return this character back to the input and return corresponding value
                else
                {
                    input.putback(temp);
                    return Token::LESS_SYMBOL;
                }
            }
            else if(c == '=')
            {
                // Check if the next character is '=', we also take it
                char temp = input.get();
                if(temp == '=')
                {
                    soft_buffer += temp;
                    hard_buffer += temp;
                    return Token::EQUAL_SYMBOL;
                }
                // If not, we return this character back to the input and return corresponding value
                else
                {
                    input.putback(temp);
                    return Token::ASSIGNMENT_SYMBOL;
                }
            }

            // Switch case to identify arithmetic operator
            switch(c)
            {
            case '+':
                return Token::PLUS_SYMBOL;
            case '-':
                return Token::MINUS_SYMBOL;
            case '*':
                return Token::MUL_SYMBOL;
            case '/':
                return Token::DIV_SYMBOL;
            }

            // Only case left is string, where " is used
            if(c == '\"')
            {
                // Remove " from the soft buffer
                soft_buffer.pop_back();

                c = input.get();
                // Read everything until the next "
                while(c != '\"')
                {
                    // If character is neither digit, char, punctuation nor space
                    if(c != ' ' && !std::isalnum(c) && !std::ispunct(c))
                        // Throw an error
                        throw Lexical_Error{"unexpected character in string " + soft_buffer};

                    soft_buffer += c;
                    hard_buffer += c;
                    c = input.get();
                }

                // Now that c is "
                hard_buffer += c;

                return Token::STRING;
            }

            // Anything else is an error
            throw Lexical_Error{soft_buffer};
        }
    };

 protected:
    // A translator owns a lexer
    Lexer* p_lexer;

    // Variable to keep track of all declared variables
    std::set<std::string> id_set;

 public:
    Translator() : p_lexer(nullptr), id_set() {}

    // Each translator should use its own resources
    Translator(const Translator&) = delete;
    Translator(Translator&&) = delete;

    // Main method for outside world to interact with objects of this class
    bool operator()(std::string file_path)
    {
        if(file_path.size() < 4 || !std::ifstream(file_path))
        {
            std::cerr << "Invalid file path" << std::endl;
            return false;
        }

        std::string infile_name(file_path.begin(), file_path.end() - 4);
        std::string infile_extension(file_path.end() - 4, file_path.end());
        if(infile_extension != ".txt")
        {
            std::cerr << "Invalid file extension" << std::endl;
            return false;
        }
        std::string outfile_path = infile_name + ".cpp";

        std::fstream input_file(file_path);
        std::ofstream output_file(outfile_path, std::ios::trunc);
        p_lexer = new Lexer(input_file);

        bool result = program(output_file);
        if(result == false)
        {
            remove(outfile_path.c_str());
        }

        delete p_lexer;
        p_lexer = nullptr;
        id_set.clear();

        return result;
    }

 private:
    /* The lexer always advances first (either normal advance or newline_check advance) and the current token in the lexer is processed after that */

    // Main method to handle the program
    // <program>	::= 'BEGIN' <newlines> <statements> <newlines> 'END'
    bool program(std::ofstream& file)
    {
        Token current_token;
        // Special variable used for the case where no character follow END literal, which means the final token will be EOFSTREAM although the text is END
        std::string temp_text;

        try
        {
            ////////////////////////////////////////////////////////
            // include standard library and using namespace std
            file << "#include <iostream>" << "\n" << "\n"
                 << "using namespace std;" << "\n" << std::endl;
            ////////////////////////////////////////////////////////
            p_lexer->advance();

            current_token = p_lexer->get_current_token();
            // a BEGIN is a must
            if(current_token != Token::BEGIN_LITERAL)
            {
                throw Syntax_Error{"Cannot find the beginning of the program"};
            }

            ////////////////////////////
            // create main()
            file << "int main(int argc, char *argv[])" << "\n"
                 << "{" << std::endl;
            ////////////////////////////

            newlines("BEGIN");
            p_lexer->advance();

            // In case of the program's having no body, statements is simply empty so the current token now is END_LITERAL directly
            // Otherwise, proceed normally

            if(p_lexer->get_current_token() != Token::END_LITERAL)
            {
                statements(file, "\t");

                temp_text = p_lexer->get_current_text();
                p_lexer->advance();
            }

            current_token = p_lexer->get_current_token();
            // an END is a must
            if((current_token != Token::END_LITERAL) && !(current_token == Token::EOFSTREAM && temp_text == "END"))
            {
                throw Syntax_Error{"Cannot find the end of the program"};
            }
            p_lexer->advance();

            ///////////////////////////////////
            // create end of main
            file << "\t" << "return 0;" << "\n"
                 << "}" << std::endl;
            ///////////////////////////////////

            current_token = p_lexer->get_current_token();
            // END must be the end of program
            if(current_token != Token::EOFSTREAM)
            {
                throw Syntax_Error{"Unexpected tokens after END"};
            }

            return true;
        }
        catch(Lexical_Error& er)
        {
            std::cerr << "Lexical Error: " << er << std::endl;
            return false;
        }
        catch(Syntax_Error& er)
        {
            std::cerr << "Syntax Error: " << er << std::endl;
            return false;
        }
    }

    // Helper method with lexer newline_check advance to look for newline
    void newlines(std::string name)
    {
        p_lexer->advance(true);
        if(p_lexer->get_current_token() != Token::NEWLINE)
        {
            throw Syntax_Error{name + " must be followed by a newline"};
        }
    }

    // Method to handle statements. In this method, the lexer only advances to the last available 'newlines'
    // <statements>	::= <print_statement><newline><statements>|<input_statement><newline><statements>
    //                  |<let_statement><newline><statements>|<if_statement><newline><statements>|<while_statement><newline><statements>|empty
    void statements(std::ofstream& file, std::string prefix)
    {
        while(true)
        {
            switch(p_lexer->get_current_token())
            {
            case Token::PRINT_LITERAL:
                print_statement(file, prefix);
                newlines("print_statement");

                p_lexer->advance(); // Move past the newline

                break;

            case Token::INPUT_LITERAL:
                input_statement(file, prefix);
                newlines("input_statement");

                p_lexer->advance(); // Move past the newline

                break;

            case Token::LET_LITERAL:
                let_statement(file, prefix);
                newlines("let_statement");

                p_lexer->advance(); // Move past the newline

                break;

            case Token::IF_LITERAL:
                if_statement(file, prefix);
                newlines("if_statement");

                p_lexer->advance(); // Move past the newline

                break;

            case Token::WHILE_LITERAL:
                while_statement(file, prefix);
                newlines("print_statement");

                p_lexer->advance(); // Move past the newline

                break;

            default:
                p_lexer->move_back();   // Move back stream to its previous state because when flow of code reachs here
                                        // The lexer has advanced to the next token already
                                        // but we only want to advance to the last available newline ("next to" the next token)
                return;
            }
        }
    }

    // Method to handle print statements. In this method, the lexer only advances to the last component (string or ID)
    // <print_statement>	::= 'PRINT' <string>|'PRINT' id
    void print_statement(std::ofstream& file, std::string prefix)
    {
        p_lexer->advance(); // move past PRINT literal

        file << prefix << "cout << ";

        // Look for STRING or ID
        switch(p_lexer->get_current_token())
        {
        case Token::STRING:
            file << '\"' << p_lexer->get_current_text() << '\"' << ';' << std::endl;
            return;
        case Token::ID:
            if(id_set.find(p_lexer->get_current_text()) == id_set.end())
            {
                throw Syntax_Error{"Attempt to print an undeclared identifier"};
            }

            file << p_lexer->get_current_text() << ';' << std::endl;
            return;

        default:
            // anything else is an error
            throw Syntax_Error{"Unexpected tokens after PRINT"};
        }
    }

    // Method to handle input statements. In this method, the lexer only advances to the ID
    // <input_statement>	::= 'INPUT' <id>
    void input_statement(std::ofstream& file, std::string prefix)
    {
        p_lexer->advance(); // move past INPUT literal

        // Look for an ID
        if(p_lexer->get_current_token() == Token::ID)
        {
            // If variables has not been declared, declare it
            if(id_set.find(p_lexer->get_current_text()) == id_set.end())
            {
                file << prefix << "int " << p_lexer->get_current_text() << ';' << "\n";
                // Assign it to the set of already declared variables
                id_set.insert(p_lexer->get_current_text());
            }

            file << prefix << "cin >> " << p_lexer->get_current_text() << ';' << std::endl;
            return;
        }
        else
        {
            // anything else is an error
            throw Syntax_Error{"Unexpected tokens after INPUT"};
        }
    }

    // Method to handle let statements. In this method, the lexer only advances to the assignment
    // <let_statement>	::= 'LET' <assignment>
    void let_statement(std::ofstream& file, std::string prefix)
    {
        file << prefix;

        p_lexer->advance(); // move past LET literal

        // If variables has not been declared, declare it
        if(id_set.find(p_lexer->get_current_text()) == id_set.end())
        {
            file << "int ";
            // Assign it to the set of already declared variables
            id_set.insert(p_lexer->get_current_text());
        }

        assignment(file, "");
    }

    // Method to handle if statements. In this method, the lexer only advances to ENDIF
    // <if_statement>	:= 'IF' <condition> <newline> <statements> <newline> 'ENDIF'
    void if_statement(std::ofstream& file, std::string prefix)
    {
        file << prefix << "if(";

        Token current_token;
        p_lexer->advance(); // move past IF literal

        condition(file, "");
        newlines("if_statement's condition");

        file << ")" << "\n"
             << prefix << "{" << "\n";

        p_lexer->advance(); // move past newline literal
        statements(file, prefix + "\t");

        // I think the following looking for endline should be removed since statements() has already look for endline

        /*
        newlines("if_statements");
        */

        file << prefix << "}" << std::endl;

        p_lexer->advance();
        current_token = p_lexer->get_current_token();

        ////////////////////Extra lines to handle ELSE and ELSEIF that I added myself///////////////

        // any number of ELSEIFs can follow an IF
        while(current_token == Token::ELSEIF_LITERAL)
        {
            file << prefix << "else if(";

            p_lexer->advance(); // move past ELSEIF literal

            condition(file, "");
            newlines("elseif_statement's condition");

            file << ")" << "\n"
                 << prefix << "{" << "\n";

            p_lexer->advance(); // move past newline literal
            statements(file, prefix + "\t");

            file << prefix << "}" << std::endl;

            p_lexer->advance();
            current_token = p_lexer->get_current_token();
        }

        // after that, an ELSE is optional
        if(current_token == Token::ELSE_LITERAL)
        {
            newlines("ELSE");

            file << prefix << "else" << "\n"
                 << prefix << "{" << "\n";

            p_lexer->advance(); // move past newline literal
            statements(file, prefix + "\t");

            file << prefix << "}" << std::endl;

            p_lexer->advance(); // move past the statement
            current_token = p_lexer->get_current_token();
        }

        /////////////////////////////////////////////////////////////////////////////////////////////

        // an ENDIF is a must
        if(current_token != Token::ENDIF_LITERAL)
        {
            throw Syntax_Error{"Cannot find the end of if_statement"};
        }
    }

    // Method to handle while statements. In this method, the lexer only advances to ENDWHILE
    // <while_statement>	:= 'WHILE' <condition> ‘REPEAT’<newline> <statements> <newline> 'ENDWHILE'
    void while_statement(std::ofstream& file, std::string prefix)
    {
        file << prefix << "while(";

        Token current_token;
        p_lexer->advance(); // move past WHILE literal

        condition(file, "");

        file << ")" << "\n"
             << prefix << "{" << "\n";

        // Now check for REPEAT literal, this literal must be on the same line as WHILE literal
        // Therefore, if we advance the lexer with newline_check and cannot find REPEAT literal, this results in a syntax error

        p_lexer->advance(true);
        current_token = p_lexer->get_current_token();
        // a REPEAT is a must
        if(current_token != Token::REPEAT_LITERAL)
        {
            throw Syntax_Error{"a WHILE literal and a REPEAT literal must be on the same line"};
        }

        newlines("REPEAT");

        p_lexer->advance(); // move past newline literal
        statements(file, prefix + "\t");

        // I think the following looking for endline should be removed since statements() has already look for endline

        /*
        newlines("while_statements");
        */

        file << prefix << "}" << std::endl;

        p_lexer->advance();
        current_token = p_lexer->get_current_token();
        // an ENDWHILE is a must
        if(current_token != Token::ENDWHILE_LITERAL)
        {
            throw Syntax_Error{"Cannot find the end of while_statement"};
        }
    }

    // Method to handle assignment. In this method, the lexer only advances to the expression
    // <assignment>	::= <id> = <expression>
    void assignment(std::ofstream& file, std::string prefix)
    {
        if(p_lexer->get_current_token() != Token::ID)
        {
            throw Syntax_Error{"Target of assignment must be an identifier"};
        }
        else if(id_set.find(p_lexer->get_current_text()) == id_set.end())
        {
            throw Syntax_Error{"Attempt to assign to an undeclared identifier"};
        }

        file << prefix << p_lexer->get_current_text();

        p_lexer->advance();
        // '=' is a must
        if(p_lexer->get_current_token() != Token::ASSIGNMENT_SYMBOL)
        {
            throw Syntax_Error{"Unexpected token in assignment"};
        }

        file << " = ";

        p_lexer->advance(); // Move past assignment symbol
        expression(file, "");
        file << ';' << std::endl;
    }

    // Method to handle expressions. In this method, the lexer only advances to the last available 'exp'
    // <expression> 	::= ( <id>|<num> ) <exp>| <exp> '+' <exp>| <exp> '-' <exp>| <exp> '*' <exp>| <exp> '/' <exp>| <exp> 'mod' <exp>
    void expression(std::ofstream& file, std::string prefix)
    {
        file << prefix;

        exp(file, "");

        p_lexer->advance(); // Move past the exp

        Token current_token = p_lexer->get_current_token();
        switch(current_token)
        {
        case Token::PLUS_SYMBOL:
            file << " + ";
            p_lexer->advance(); // Move past the symbol
            exp(file, "");
            return;

        case Token::MINUS_SYMBOL:
            file << " - ";
            p_lexer->advance(); // Move past the symbol
            exp(file, "");
            return;

        case Token::MUL_SYMBOL:
            file << " * ";
            p_lexer->advance(); // Move past the symbol
            exp(file, "");
            return;

        case Token::DIV_SYMBOL:
            file << " / ";
            p_lexer->advance(); // Move past the symbol
            exp(file, "");
            return;

        case Token::MOD_SYMBOL:
            file << " % ";
            p_lexer->advance(); // Move past the symbol
            exp(file, "");
            return;

        default:
            p_lexer->move_back();   // Move back stream to its previous state because when flow of code reachs here
                                    // The lexer has advanced past the last 'exp'
                                    // but we only want the lexer to advance to the last 'exp' (not past the last 'exp')
            return;
        }
    }

    // Method to handle 'exp'. In this method, the lexer only advances to the last component of 'exp'
    // <exp>	:= <id>|<number>
    void exp(std::ofstream& file, std::string prefix)
    {
        file << prefix;

        if(p_lexer->get_current_token() == Token::ID)
        {
            if(id_set.find(p_lexer->get_current_text()) == id_set.end())
            {
                throw Syntax_Error{"Attempt to handle an undeclared identifier in exp"};
            }

            file << p_lexer->get_current_text();
        }
        else
        {
            number(file, "");
        }
    }

    // Method to handle numbers. In this method, the lexer only advances to the 'num' token
    // <number>	::= '-'<num>|'+'<num>| <num>
    void number(std::ofstream& file, std::string prefix)
    {
        file << prefix;

        Token current_token = p_lexer->get_current_token();
        // Look for -/+ followed by a NUM, or only NUM itself
        switch(current_token)
        {
        case Token::MINUS_SYMBOL:
            file << "-";

            p_lexer->advance();
            current_token = p_lexer->get_current_token();
            // NUM is a must
            if(current_token == Token::NUM)
            {
                file << p_lexer->get_current_text();
                return;
            }
            else
            {
                throw Syntax_Error{"Unexpected tokens in number"};
            }

        case Token::PLUS_SYMBOL:
            file << "+";

            p_lexer->advance();
            current_token = p_lexer->get_current_token();
            // NUM is a must
            if(current_token == Token::NUM)
            {
                file << p_lexer->get_current_text();
                return;
            }
            else
            {
                throw Syntax_Error{"Unexpected tokens in number"};
            }

        case Token::NUM:
            file << p_lexer->get_current_text();
            return;

        default:
            // anything else is an error
            throw Syntax_Error{"Unexpected tokens in number"};
        }
    }

    // Method to handle condition. In this method, the lexer only advances to the last expression
    // <condition>	::= <expression> <compare> <expression>
    void condition(std::ofstream& file, std::string prefix)
    {
        file << prefix;

        expression(file, "");

        p_lexer->advance();
        Token current_token = p_lexer->get_current_token();
        // Look for comparison symbol
        switch(current_token)
        {
        case Token::GREATER_SYMBOL:
            file << " > ";
            break;

        case Token::LESS_SYMBOL:
            file << " < ";
            break;

        case Token::GREATER_EQUAL_SYMBOL:
            file << " >= ";
            break;
        case Token::LESS_EQUAL_SYMBOL:
            file << " <= ";
            break;

        case Token::EQUAL_SYMBOL:
            file << " == ";
            break;

        default:
            // anything else is an error
            throw Syntax_Error{"Unexpected tokens in condition"};
        }

        p_lexer->advance(); // move past the symbol

        expression(file, "");
    }
};
}

#endif // TINY_LANGUAGE_HPP_INCLUDED
