#include "common.h"
#include "chunk.h"
#include "disassem.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "table.h"

static void repl()
{
  char line[1024];
  for (;;)
  {
    printf("> ");
    if (!fgets(line, sizeof(line), stdin))
    {
      printf("\n");
      break;
    }
    if (strcmp(line, "exit\n") == 0)
      break;
    interpret(line);
  }
}

static char *read_file(const char *path)
{
  FILE *file = fopen(path, "rb");
  if (file == NULL)
  {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }
  fseek(file, 0L, SEEK_END);
  size_t file_sz = ftell(file);
  rewind(file);
  char *buffer = (char *)malloc(file_sz + 1);
  if (buffer == NULL)
  {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  size_t bytes_read = fread(buffer, sizeof(char), file_sz, file);
  if (bytes_read < file_sz)
  {
    fprintf(stderr, "Could read file \"%s\".\n", path);
    exit(74);
  }
  buffer[bytes_read] = '\0';
  fclose(file);
  return buffer;
}

static void run_file(const char *path)
{
  char *src = read_file(path);
  enum InterpretResult result = interpret(src);
  free(src);

  if (result == INTERPRET_COMPILE_ERR) exit(65);
  if (result == INTERPRET_RUNTIME_ERR) exit(70);
}

int main(int argc, char **argv)
{
  init_vm();
  
  if (argc == 1)
    repl();
  else if (argc == 2)
    run_file(argv[1]);
  else
  {
    fprintf(stderr, "Usage: clox [path]\n");
    exit(64);
  }

  free_vm();
  return 0;
}
