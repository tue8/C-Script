#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "object.h"

#ifdef DEBUG_PRINT_CODE
#include "assem.h"
#endif

struct Parser
{
  struct Token curr;
  struct Token previous;
  bool had_err;
  bool panic_mode;
};

enum Precedence
{
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,        // or
  PREC_AND,       // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,      // + -
  PREC_FACTOR,    // * /
  PREC_UNARY,     // ! -
  PREC_CALL,      // . ()
  PREC_PRIMARY
};

typedef void (*ParseFn)(bool can_assign);

struct ParseRule
{
  ParseFn prefix;
  ParseFn infix;
  enum Precedence precedence;
};

struct Local
{
  struct Token name;
  int depth;
};

struct Compiler
{
  struct Local locals[UINT8_COUNT];
  int local_count;
  int scope_depth;
  bool is_in_loop;
  int break_offset[UINT8_COUNT];  
  int break_offset_count;
};

struct Parser parser;
struct Compiler *current = NULL;
struct Chunk *compiling_chunk;

static struct Chunk *curr_chunk()
{
  return compiling_chunk;
}

static void error_at(struct Token *token, const char *message)
{
  if (parser.panic_mode) return;
  parser.panic_mode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF)
    fprintf(stderr, " at end");
  else if (token->type == TOKEN_ERROR)
  {}
  else
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  fprintf(stderr, ": %s\n", message);
  parser.had_err = true;
}

static void error(const char *message)
{
  error_at(&parser.previous, message);
}

static void error_at_curr(const char *message)
{
  error_at(&parser.curr, message);
}

static void advance()
{
  parser.previous = parser.curr;

  for (;;)
  {
    parser.curr = scan_token();
    if (parser.curr.type != TOKEN_ERROR)
      break;
    error_at_curr(parser.curr.start);
  }
}

static void consume(enum TokenType type, const char *message)
{
  if (parser.curr.type == type)
  {
    advance();
    return;
  }

  error_at_curr(message);
}

static bool check(enum TokenType type)
{
  return parser.curr.type == type;
}

static bool match(enum TokenType type)
{
  if (!check(type))
    return false;
  advance();
  return true;
}

static void emit_byte(uint8_t byte)
{
  write_chunk(curr_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2)
{
  emit_byte(byte1);
  emit_byte(byte2);
}

static void emit_jl(int loop_start)
{
  emit_byte(OP_JL);
  
  int offset = curr_chunk()->count - loop_start + 2;
  if (offset > UINT16_MAX)
    error("Loop body too large.");
  emit_byte((offset >> 8) & 0xff);
  emit_byte(offset & 0xff);
}

static int emit_jmp(uint8_t instruction)
{
  emit_byte(instruction);
  emit_byte(0xff);
  emit_byte(0xff);
  return curr_chunk()->count - 2;
}

static void emit_return()
{
  emit_byte(OP_RETURN);
}

static uint8_t make_constant(Value value)
{
  int constant = add_constant(curr_chunk(), value);
  if (constant > UINT8_MAX)
  {
    error("Too many constants in one chunk");
    return 0;
  }

  return (uint8_t)constant;
}

static void emit_constant(Value value)
{
  emit_bytes(OP_CONSTANT, make_constant(value));
}

static void patch_jmp(int offset)
{
  if (offset < 0)
    return;
  /* -2 means omitting 2 bytes from the jmp instruction that we will execute */
  int jmp = curr_chunk()->count - offset - 2;
  if (jmp > UINT16_MAX)
    error("Too much code to jump over.");

  curr_chunk()->code[offset] = (jmp >> 8) & 0xff;
  curr_chunk()->code[offset + 1] = jmp & 0xff;
}

static void init_compiler(struct Compiler *compiler)
{
  compiler->local_count = 0;
  compiler->scope_depth = 0;
  compiler->is_in_loop = false;
  compiler->break_offset_count = -1;
  current = compiler;
}

static void end_compiler()
{
  emit_return();
#ifdef DEBUG_PRINT_CODE
  if (!parser.had_err)
{
    disassem_chunk(curr_chunk(), "code");
  }
#endif
}

static void begin_loop()
{
  current->is_in_loop = true;
  current->break_offset_count++;
  current->break_offset[current->break_offset_count] = -1;
}

static void end_loop()
{
  patch_jmp(current->break_offset[current->break_offset_count]);
  current->break_offset_count--;
  current->is_in_loop = false;
}

static void begin_scope()
{
  current->scope_depth++;
}

static void end_scope()
{
  current->scope_depth--;
  while (current->local_count > 0 &&
         current->locals[current->local_count - 1].depth >
         current->scope_depth)
  {
    emit_byte(OP_POP);
    current->local_count--;
  }
}

static void expression();
static void statement();
static void declaration();
static struct ParseRule *get_rule(enum TokenType type);
static void parse_precedence(enum Precedence precedence);

static void binary(bool can_assign)
{
  enum TokenType operator_type = parser.previous.type;
  struct ParseRule *rule = get_rule(operator_type);
  parse_precedence((enum Precedence)(rule->precedence + 1));

  switch (operator_type)
  {
    /* some of these are purely syntactic sugar */
    /* a != b is just a == !(b) */
    case TOKEN_BANG_EQUAL:    emit_bytes(OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL:   emit_byte(OP_EQUAL); break;
    case TOKEN_GREATER:       emit_byte(OP_GREATER); break;
    case TOKEN_GREATER_EQUAL: emit_bytes(OP_LESS, OP_NOT); break;
    case TOKEN_LESS:          emit_byte(OP_LESS); break;
    case TOKEN_LESS_EQUAL:    emit_bytes(OP_GREATER, OP_NOT); break;
    case TOKEN_PLUS:  emit_byte(OP_ADD); break;
    case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
    case TOKEN_STAR:  emit_byte(OP_MULTIPLY); break;
    case TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
    default:
      return; // Unreachable.
  }
}

static void literal(bool can_assign)
{
  switch (parser.previous.type)
  {
    case TOKEN_FALSE: emit_byte(OP_FALSE); break;
    case TOKEN_NIL:   emit_byte(OP_NIL);   break;
    case TOKEN_TRUE:  emit_byte(OP_TRUE);  break;
    default: return;
  }
}

static void grouping(bool can_assign)
{
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void number(bool can_assign)
{
  double value = strtod(parser.previous.start, NULL);
  emit_constant(NUMBER_VAL(value));
}

static void or_(bool can_assign)
{
  int else_jmp = emit_jmp(OP_JNT);
  int end_jmp = emit_jmp(OP_JMP);
  patch_jmp(else_jmp);
  emit_byte(OP_POP);
  parse_precedence(PREC_OR);
  patch_jmp(end_jmp);
}

static void and_(bool can_assign)
{
  int end_jmp = emit_jmp(OP_JNT);
  /*
   * we dont need the value on the stack if
   * left hand expression not false then the
   * value of 'and' depends on the right expression
   */
  emit_byte(OP_POP);
  parse_precedence(PREC_AND);
  patch_jmp(end_jmp);
}

static void string(bool can_assign)
{
  struct ObjString *obj_str = copy_str(parser.previous.start + 1,
                                       parser.previous.length - 2);
  emit_constant(OBJ_VAL((struct Obj *)obj_str));
}

static uint8_t identifier_constant(const struct Token *name)
{
  return make_constant(OBJ_VAL(copy_str(name->start, name->length)));
}

static bool identifiers_equal(struct Token *a, struct Token *b)
{
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(struct Compiler *compiler, struct Token *name)
{
  for (int i = compiler->local_count - 1; i >= 0; i--)
  {
    struct Local *local = &compiler->locals[i];
    if (identifiers_equal(name, &local->name))
    {
      if (local->depth == -1)
        error("Can't read local variable in its own initializer.");
      return i;
    }
  }

  return -1;
}

static void add_local(struct Token name)
{
  if (current->local_count == UINT8_COUNT)
  {
    error("Too many local variables in function.");
    return;
  }

  struct Local *local = &current->locals[current->local_count++];
  local->name = name;
  local->depth = -1;
}

static void declare_variable()
{
  if (current->scope_depth == 0)
    return;
  struct Token *name = &parser.previous;
  for (int i = current->local_count - 1; i >= 0; i--)
  {
    struct Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scope_depth)
      break;

    if (identifiers_equal(name, &local->name))
      error("A variable with this name is already in the scope.");
  }
  add_local(*name);
}

static void named_variable(struct Token name, bool can_assign)
{
  uint8_t get_op, set_op;
  int arg = resolve_local(current, &name);

  if (arg != -1)
  {
    get_op = OP_GETLOCAL;
    set_op = OP_SETLOCAL;
  }
  else
  {
    arg = identifier_constant(&name);
    get_op = OP_GETGLOBAL;
    set_op = OP_SETGLOBAL;
  }

  if (match(TOKEN_EQUAL) && can_assign)
  {
    expression();
    emit_bytes(set_op, (uint8_t)arg);
  }
  else
    emit_bytes(get_op, (uint8_t)arg);
}

static void variable(bool can_assign)
{
  named_variable(parser.previous, can_assign);
}

static void unary(bool can_assign)
{
  enum TokenType operator_type = parser.previous.type;

  parse_precedence(PREC_UNARY);

  switch (operator_type)
  {
    case TOKEN_BANG:
        emit_byte(OP_NOT);
        break;
    case TOKEN_MINUS:
        emit_byte(OP_NEGATE);
        break;
    default:
        return;
  }
}

struct ParseRule rules[] =
{
    [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
    [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
    [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static void parse_precedence(enum Precedence precedence)
{
  advance();
  ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
  if (prefix_rule == NULL)
  {
    error("Expect expression.");
    return;
  }

  bool can_assign = precedence <= PREC_ASSIGNMENT;
  prefix_rule(can_assign);

  while (precedence <= get_rule(parser.curr.type)->precedence)
  {
    advance();
    ParseFn infix_rule = get_rule(parser.previous.type)->infix;
    infix_rule(can_assign);
  }

  /* there is no parsing function associated with = so we skip that loop */
  if (can_assign && match(TOKEN_EQUAL))
  {
    error("Invalid assignment target.");
  }
}



static uint8_t parse_variable(const char *err_msg)
{
  consume(TOKEN_IDENTIFIER, err_msg);
  declare_variable();
  if (current->scope_depth > 0)
    return 0;
  return identifier_constant(&parser.previous);
}

static void mark_initialized()
{
  current->locals[current->local_count - 1].depth = current->scope_depth;
}

static void define_variable(uint8_t global)
{
  if (current->scope_depth > 0)
  {
    mark_initialized();
    return;
  }
  
  emit_bytes(OP_DEFINEGLOBAL, global);
}



static struct ParseRule *get_rule(enum TokenType type)
{
  return &rules[type];
}

static void expression()
{
  parse_precedence(PREC_ASSIGNMENT);
}

static void block()
{
  while (!check(TOKEN_RIGHT_BRACE) &&
         !check(TOKEN_EOF))
    declaration();
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block");
}

static void var_declaration()
{
  uint8_t global = parse_variable("Expect variable name.");
  
  if (match(TOKEN_EQUAL))
    expression();
  else
    emit_byte(OP_NIL);

  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
  define_variable(global);
}

static void synchronize()
{
  parser.panic_mode = false;

  while (parser.curr.type != TOKEN_EOF)
  {
    if (parser.previous.type == TOKEN_SEMICOLON)
      return;
    switch (parser.curr.type)
    {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;
      default:
        ; // Do nothing.
    }

    advance();
  }
}

static void print_stmt()
{
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emit_byte(OP_PRINT);
}

static void while_stmt()
{
  begin_loop();
  /* constantly check for condition */
  int loop_start = curr_chunk()->count;
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int exit_jmp = emit_jmp(OP_JNT);

  emit_byte(OP_POP);
  statement();
  emit_jl(loop_start);

  patch_jmp(exit_jmp);
  emit_byte(OP_POP);
  end_loop();
}

static void expression_stmt()
{
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emit_byte(OP_POP);
}

static void for_stmt()
{
  begin_loop();
  begin_scope();
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  if (match(TOKEN_SEMICOLON))
  {
  }
  else if (match(TOKEN_VAR))
    var_declaration();
  else
    expression_stmt();

  int loop_start = curr_chunk()->count;
  int exit_jmp = -1;
  if (!match(TOKEN_SEMICOLON))
  {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';'.");
    
    exit_jmp = emit_jmp(OP_JNT);
    emit_byte(OP_POP);
  }

  if (!match(TOKEN_RIGHT_PAREN))
  {
    /* jump over increment, we will execute this after we are done executing the body */
    int body_jmp = emit_jmp(OP_JMP);
    int increment_start = curr_chunk()->count;
    expression();
    emit_byte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
    
    emit_jl(loop_start);
    
    loop_start = increment_start;
    patch_jmp(body_jmp); 
  }
  statement();
  emit_jl(loop_start);
  if (exit_jmp != -1)
  {
    patch_jmp(exit_jmp);
    emit_byte(OP_POP);
  }
  end_scope();
  end_loop();
}

static void break_stmt()
{
  consume(TOKEN_SEMICOLON, "Expected ';' after 'break'.");
  if (current->is_in_loop == false)
    error("'break' can only be placed inside a loop.");
  current->break_offset[current->break_offset_count] = emit_jmp(OP_JMP);
}

static void if_stmt()
{
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int jmp_ova_then = emit_jmp(OP_JNT);
/* condition met */
  emit_byte(OP_POP);
  statement();
  int jmp_ova_else = emit_jmp(OP_JMP);

  patch_jmp(jmp_ova_then);

/* condition not met */
  emit_byte(OP_POP);
  if (match(TOKEN_ELSE))
    statement();

  patch_jmp(jmp_ova_else);
}

static void declaration()
{
  if (match(TOKEN_VAR))
    var_declaration();
  else
    statement();

  if (parser.panic_mode)
    synchronize();
}

static void statement()
{
  if (match(TOKEN_PRINT))
    print_stmt();
  else if (match(TOKEN_LEFT_BRACE))
  {
    begin_scope();
    block();
    end_scope();
  }
  else if (match(TOKEN_IF))
    if_stmt();
  else if (match(TOKEN_WHILE))
    while_stmt();
  else if (match(TOKEN_FOR))
    for_stmt();
  else if (match(TOKEN_BREAK))
    break_stmt();
  else
    expression_stmt();
}



bool compile(const char *src, struct Chunk *chunk)
{
  init_scanner(src);
  struct Compiler compiler;
  init_compiler(&compiler);
  compiling_chunk = chunk;

  parser.had_err = false;
  parser.panic_mode = false;

  advance();
  while (!match(TOKEN_EOF))
    declaration();
  end_compiler();
  return !parser.had_err;
}
