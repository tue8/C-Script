#include "scanner.h"
#include <string.h>
#include <stdbool.h>

struct Scanner
{
  const char *start;
  const char *curr;
  int line;
};

struct Scanner scanner;

void init_scanner(const char *src)
{
  scanner.start = src;
  scanner.curr = src;
  scanner.line = 1;
}

static bool is_alpha(char c)
{
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
    c == '_';
}

static bool is_digit(char c)
{
  return c >= '0' && c <= '9';
}

static bool is_at_end()
{
  return *scanner.curr == '\0';
}

static char advance()
{
  return *scanner.curr++;
}

static char peek()
{
  return *scanner.curr;
}

static char peek_next()
{
  if (is_at_end()) return '\0';
  return scanner.curr[1];
}

static bool match(char expected)
{
  if (is_at_end())
    return false;
  if (*scanner.curr != expected)
    return false;
  scanner.curr++;
  return true;
}

static struct Token make_token(enum TokenType type)
{
  struct Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.curr - scanner.start);
  token.line = scanner.line;
  return token;
}

static struct Token err_token(const char *msg)
{
  struct Token token;
  token.type = TOKEN_ERROR;
  token.start = msg;
  token.length = (int)strlen(msg);
  token.line = scanner.line;
  return token;
}

static void skip_whitespace()
{
  for (;;)
  {
    char c = peek();
    switch (c)
    {
      case ' ':
      case '\r':
      case '\t':
        advance();
        break;
      case '\n':
        scanner.line++;
        advance();
        break;
      case '/':
        if (peek_next() == '/')
          while (peek() != '\n' && !is_at_end())
            advance();
        else
          return;
        break;
      default:
        return;
    }
  }
}

static enum TokenType check_keyword(int start, int length,
                const char* rest, enum TokenType type)
{
  if (scanner.curr - scanner.start == start + length &&
      memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

static struct Token number()
{
  while (is_digit(peek()))
    advance();
  if (peek() == '.' && is_digit(peek_next()))
  {
    advance();
    while (is_digit(peek()))
      advance();
  }
  return make_token(TOKEN_NUMBER);
}

static struct Token string()
{
  while (peek() != '"' && !is_at_end())
  {
    if (peek() == '\n')
      scanner.line++;
    advance();
  }

  if (is_at_end())
    return err_token("Unterminated string.");

  advance();
  return make_token(TOKEN_STRING);
}

static enum TokenType identifier_type()
{
  switch (*scanner.start)
  {
    case 'a': return check_keyword(1, 2, "nd", TOKEN_AND);
    case 'b': return check_keyword(1, 4, "reak", TOKEN_BREAK);
    case 'c': return check_keyword(1, 4, "lass", TOKEN_CLASS);
    case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE);
    case 'f':
      if (scanner.curr - scanner.start > 1)
      {
        switch (*(scanner.start + 1))
        {
          case 'a': return check_keyword(2, 3, "lse", TOKEN_FALSE);
          case 'o': return check_keyword(2, 1, "r", TOKEN_FOR);
          case 'u': return check_keyword(2, 1, "n", TOKEN_FUN);
              }
          }
            break;
    case 'i': return check_keyword(1, 1, "f", TOKEN_IF);
    case 'n': return check_keyword(1, 2, "il", TOKEN_NIL);
    case 'o': return check_keyword(1, 1, "r", TOKEN_OR);
    case 'p': return check_keyword(1, 4, "rint", TOKEN_PRINT);
    case 'r': return check_keyword(1, 5, "eturn", TOKEN_RETURN);
    case 's': return check_keyword(1, 4, "uper", TOKEN_SUPER);
    case 't':
      if (scanner.curr - scanner.start > 1)
      {
        switch (*(scanner.start + 1))
        {
          case 'h': return check_keyword(2, 2, "is", TOKEN_THIS);
          case 'r': return check_keyword(2, 2, "ue", TOKEN_TRUE);
              }
          }
            break;
    case 'v': return check_keyword(1, 2, "ar", TOKEN_VAR);
    case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE);
  }
  return TOKEN_IDENTIFIER;
}

static struct Token identifier()
{
  while (is_alpha(peek()) || is_digit(peek()))
    advance();
  return make_token(identifier_type());
}



struct Token scan_token()
{
  skip_whitespace();
  scanner.start = scanner.curr;
  if (is_at_end())
    return make_token(TOKEN_EOF);
  char c = advance();
  if (is_alpha(c))
    return identifier();
  if (is_digit(c))
    return number();
  switch (c)
  {
    case '(': return make_token(TOKEN_LEFT_PAREN);
    case ')': return make_token(TOKEN_RIGHT_PAREN);
    case '{': return make_token(TOKEN_LEFT_BRACE);
    case '}': return make_token(TOKEN_RIGHT_BRACE);
    case ';': return make_token(TOKEN_SEMICOLON);
    case ',': return make_token(TOKEN_COMMA);
    case '.': return make_token(TOKEN_DOT);
    case '-': return make_token(TOKEN_MINUS);
    case '+': return make_token(TOKEN_PLUS);
    case '/': return make_token(TOKEN_SLASH);
    case '*': return make_token(TOKEN_STAR);
    case '!':
        return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=':
        return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<':
        return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>':
        return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '"':
        return string();
  }
  return err_token("Unexpected character.");
}
