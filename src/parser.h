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

#define ARG_RESULT_SUCCESS(result) ((result & ARG_PARSE_OK) != 0)
#define ARG_RESULT_FAILURE(result) ((result & ARG_PARSE_ERROR) != 0)

#define ARG_PARSER_OPT_NONE 0u
#define ARG_PARSER_OPT_USE_FILL_WORDS (1u << 0)
#define ARG_PARSER_OPT_DELIMS_ONLY (1u << 1)
#define ARG_PARSER_OPT_ALLOW_TRAILING_JUNK (1u << 2)
#define ARG_PARSER_OPT_ALLOW_RANDOM_ORDER (1u << 3)
#define ARG_PARSER_OPT_CASE_SENSITIVE (1u << 4)

#define ARG_PARSER_DELIMS_SPACE " \t\r\n"
#define ARG_PARSER_DELIMS_COMMA ","
#define ARG_PARSER_DELIMS_BAR "|"
#define ARG_PARSER_DELIMS_SEMICOLON ";"

typedef enum
{
  ARG_TYPE_STRING = 0,
  ARG_TYPE_INT = 1,
  ARG_TYPE_BOOL = 2,
  ARG_TYPE_FLOAT = 3,
  ARG_TYPE_REST_OF_LINE = 4
} arg_type;

#define ARG_OPT_NONE 0u,
#define ARG_OPT_REQUIRED 1u << 0
#define ARG_OPT_OPTIONAL 1u << 1
#define ARG_OPT_ABBREV 1u << 2
#define ARG_OPT_EXACT 1u << 3
#define ARG_OPT_DEPENDS_ON_PREV 1u << 4
#define ARG_OPT_QUOTE_DOUBLE 1u << 5
#define ARG_OPT_QUOTE_SINGLE 1u << 6
#define ARG_OPT_REGEX 1u << 7

#define ARG_PARSE_NONE 0u
#define ARG_PARSE_OK 1u << 0
#define ARG_PARSE_EOL 1u << 1
#define ARG_PARSE_INFO 1u << 2
#define ARG_PARSE_WARNING 1u << 3
#define ARG_PARSE_ERROR 1u << 4
#define ARG_PARSE_INVALID_LIST 1u << 5
#define ARG_PARSE_MISSING_REQUIRED 1u << 6
#define ARG_PARSE_INVALID_VALUE 1u << 7
#define ARG_PARSE_EXTRA_ARGUMENT 1u << 8
#define ARG_PARSE_NO_MEMORY 1u << 9
#define ARG_PARSE_ARGUMENT_TOO_LONG 1u << 10
#define ARG_PARSE_NOT_FOUND 1u << 11
#define ARG_PARSE_TYPE_MISMATCH 1u << 12
#define ARG_PARSE_MISSING 1u << 13
#define ARG_PARSE_MISSING_NAME 1u << 14
#define ARG_PARSE_MISSING_QUOTE 1u << 15
#define ARG_PARSE_MISSING_DELIMITERS 1u << 16
#define ARG_PARSE_JUNK 1u << 17

typedef unsigned int arg_option;
typedef unsigned int arg_parser_result;

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

  bool is_option_set(unsigned int flags, unsigned int bit) { return (flags & bit) != 0; }

  bool arg_opt_required() { return is_option_set(options, ARG_OPT_REQUIRED); }
  bool arg_opt_optional() { return is_option_set(options, ARG_OPT_OPTIONAL); }
  bool arg_opt_abbrev() { return is_option_set(options, ARG_OPT_ABBREV); }
  bool arg_opt_exact() { return is_option_set(options, ARG_OPT_EXACT); }
  bool arg_opt_depends_on_prev() { return is_option_set(options, ARG_OPT_DEPENDS_ON_PREV); }
  bool arg_opt_quote_single() { return is_option_set(options, ARG_OPT_QUOTE_SINGLE); }
  bool arg_opt_quote_double() { return is_option_set(options, ARG_OPT_QUOTE_DOUBLE); }
  bool arg_opt_regex() { return is_option_set(options, ARG_OPT_REGEX); }
} arg_def;

typedef struct arg_parser_options
{
  unsigned int flags = ARG_PARSER_OPT_USE_FILL_WORDS | ARG_PARSER_OPT_ALLOW_TRAILING_JUNK;
  const char *delimiters = NULL; // Extra delimiter characters; NULL uses whitespace only.
} arg_parser_options;

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

typedef struct arg_parser_error
{
  size_t token_index = 0;
  size_t token_pos = 0;
  char *message = NULL;
  char *value = NULL;
  arg_parser_result code = ARG_PARSE_OK;
} arg_parser_error;

typedef struct arg_parser_output
{
  const arg_def *items;
  size_t count;
  arg_value *values;
  arg_parser_error error;
  arg_parser_output() {};
  ~arg_parser_output() {};
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

arg_parser_result parse_arguments(const char *argument,
                                  const arg_def *list,
                                  const arg_parser_options &options,
                                  arg_parser_output *out);

static inline arg_parser_result parse_arguments(const char *argument,
                                                const arg_def *list,
                                                arg_parser_output *out)
{
  return parse_arguments(argument, list, arg_parser_options(), out);
}
arg_parser_result parse_argument(const char *name,
                                 const arg_parser_output &parsed,
                                 const char **value);

arg_parser_result parse_argument(const char *name,
                                 const arg_parser_output &parsed,
                                 int *value);

arg_parser_result parse_argument(const char *name,
                                 const arg_parser_output &parsed,
                                 bool *value);

arg_parser_result parse_argument(const char *name,
                                 const arg_parser_output &parsed,
                                 float *value);
#endif // DURIS_PARSER_H
