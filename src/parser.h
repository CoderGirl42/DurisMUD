// parser.h

#ifndef DURIS_PARSER_H
#define DURIS_PARSER_H

#include "structs.h"

#include <float.h>
#include <limits.h>
#include <stddef.h>

#define ARG_PATTERN_WORD "[^[:space:]]+"

#define ARG_INT_MIN INT_MIN
#define ARG_INT_MAX INT_MAX
#define ARG_FLOAT_MIN (-FLT_MAX)
#define ARG_FLOAT_MAX FLT_MAX

typedef enum
{
  ARG_TYPE_STRING = 0,
  ARG_TYPE_INT = 1,
  ARG_TYPE_BOOL = 2,
  ARG_TYPE_FLOAT = 3
} arg_type;

typedef enum
{
  ARG_OPT_NONE = 0,
  ARG_OPT_REQUIRED = 1 << 0,
  ARG_OPT_OPTIONAL = 1 << 1,
  ARG_OPT_ABBREV = 1 << 2,
  ARG_OPT_EXACT = 1 << 3,
  ARG_OPT_DEPENDS_ON_PREV = 1 << 4
} arg_option;

typedef struct arg_def
{
  const char *name;
  arg_type type;
  unsigned int options;
  union
  {
    struct
    {
      const char *pattern;
    } string;
    struct
    {
      int min;
      int max;
    } integer;
    struct
    {
      float min;
      float max;
    } floating;
  } spec;
} arg_def;

typedef struct arg_list
{
  const arg_def *items;
  size_t count;
} arg_list;

typedef struct arg_parser_options
{
  unsigned int flags;
  const char *delimiters; // Extra delimiter characters; NULL uses whitespace only.
} arg_parser_options;

#define ARG_PARSER_OPT_NONE 0u
#define ARG_PARSER_OPT_USE_FILL_WORDS (1u << 0)
#define ARG_PARSER_OPT_DELIMS_ONLY (1u << 1)
#define ARG_PARSER_OPT_ALLOW_TRAILING_JUNK (1u << 2)
#define ARG_PARSER_OPT_ALLOW_RANDOM_ORDER (1u << 3)

#define ARG_PARSER_DELIMS_SPACE " \t\r\n"
#define ARG_PARSER_DELIMS_COMMA ","
#define ARG_PARSER_DELIMS_BAR "|"
#define ARG_PARSER_DELIMS_SEMICOLON ";"

typedef enum
{
  ARG_PARSE_OK = 0,
  ARG_PARSE_INVALID_LIST = 1,
  ARG_PARSE_MISSING_REQUIRED = 2,
  ARG_PARSE_INVALID_VALUE = 3,
  ARG_PARSE_EXTRA_ARGUMENT = 4,
  ARG_PARSE_NO_MEMORY = 5,
  ARG_PARSE_ARGUMENT_TOO_LONG = 6,
  ARG_PARSE_NOT_FOUND = 7,
  ARG_PARSE_TYPE_MISMATCH = 8,
  ARG_PARSE_MISSING = 9
} arg_parse_code;

typedef struct arg_value
{
  bool present;
  arg_type type;
  union
  {
    char *string;
    int integer;
    bool boolean;
    float floating;
  } as;
} arg_value;

typedef struct arg_parser_output
{
  const arg_def *items;
  size_t count;
  arg_value *values;
  char *error;
  arg_parser_output();
  ~arg_parser_output();
  arg_parser_output(const arg_parser_output &) = delete;
  arg_parser_output &operator=(const arg_parser_output &) = delete;
} arg_parser_output;

static inline arg_def define_argument(const char *name,
                                      const char *pattern,
                                      unsigned int options)
{
  arg_def def;

  def.name = name;
  def.type = ARG_TYPE_STRING;
  def.options = options;
  def.spec.string.pattern = pattern;

  return def;
}

static inline arg_def define_argument(const char *name,
                                      int min,
                                      int max,
                                      unsigned int options)
{
  arg_def def;

  def.name = name;
  def.type = ARG_TYPE_INT;
  def.options = options;
  def.spec.integer.min = min;
  def.spec.integer.max = max;

  return def;
}

static inline arg_def define_argument(const char *name,
                                      unsigned int options)
{
  arg_def def;

  def.name = name;
  def.type = ARG_TYPE_BOOL;
  def.options = options;
  def.spec.integer.min = 0;
  def.spec.integer.max = 0;

  return def;
}

static inline arg_def define_argument(const char *name,
                                      float min,
                                      float max,
                                      unsigned int options)
{
  arg_def def;

  def.name = name;
  def.type = ARG_TYPE_FLOAT;
  def.options = options;
  def.spec.floating.min = min;
  def.spec.floating.max = max;

  return def;
}

arg_parse_code parse_arguments(const char *argument,
                               const arg_list *list,
                               const arg_parser_options *options,
                               arg_parser_output &out);

static inline arg_parser_options default_arg_parser_options()
{
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_USE_FILL_WORDS | ARG_PARSER_OPT_ALLOW_TRAILING_JUNK;
  opts.delimiters = NULL;
  return opts;
}

template <size_t N>
static inline arg_parse_code parse_arguments(const char *argument,
                                             const arg_def (&defs)[N],
                                             const arg_parser_options *options,
                                             arg_parser_output &out)
{
  const arg_list list = {defs, N};
  return parse_arguments(argument, &list, options, out);
}

static inline arg_parse_code parse_arguments(const char *argument,
                                             const arg_list *list,
                                             arg_parser_output &out)
{
  arg_parser_options opts = default_arg_parser_options();
  return parse_arguments(argument, list, &opts, out);
}

template <size_t N>
static inline arg_parse_code parse_arguments(const char *argument,
                                             const arg_def (&defs)[N],
                                             arg_parser_output &out)
{
  arg_parser_options opts = default_arg_parser_options();
  return parse_arguments(argument, defs, &opts, out);
}

arg_parse_code parse_argument(const char *name,
                              const arg_parser_output &parsed,
                              const char **value);

arg_parse_code parse_argument(const char *name,
                              const arg_parser_output &parsed,
                              int *value);

arg_parse_code parse_argument(const char *name,
                              const arg_parser_output &parsed,
                              bool *value);

arg_parse_code parse_argument(const char *name,
                              const arg_parser_output &parsed,
                              float *value);

void free_parsed_arguments(arg_parser_output *out);

#endif // DURIS_PARSER_H
