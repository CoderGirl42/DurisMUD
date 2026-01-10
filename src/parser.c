// parser.c

#include "parser.h"

#include "prototypes.h"

#include <ctype.h>
#include <errno.h>
#include <new>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum arg_token_status
{
  ARG_TOKEN_NONE = 0,
  ARG_TOKEN_OK = 1,
  ARG_TOKEN_ERR_TOO_LONG = -1,
  ARG_TOKEN_ERR_INVALID = -2,
  ARG_TOKEN_ERR_UNTERMINATED = -3,
  ARG_TOKEN_ERR_JUNK_AFTER_QUOTE = -4
};

enum arg_match_status
{
  ARG_MATCH_NO = 0,
  ARG_MATCH_OK = 1,
  ARG_MATCH_ERROR = -1,
  ARG_MATCH_AMBIGUOUS = -2
};

struct bool_word
{
  const char *word;
  bool value;
};

static const struct bool_word bool_words[] = {
    {"true", true},
    {"false", false},
    {"yes", true},
    {"no", false},
    {"on", true},
    {"off", false}};

static bool arg_is_delim_char(char ch, bool whitespace_delims, const char *delims);

class ArgToken
{
public:
  ArgToken() : value_(NULL), pos_(0), end_(0), index_(0){};

  ArgToken(const char *value, size_t pos, size_t end, size_t index) : pos_(pos), end_(end), index_(index)
  {
    if (value)
    {
      value_ = strdup(value);
    }
    else
    {
      value_ = NULL;
    }
  };

  ~ArgToken()
  {
    if (value_)
    {
      // free(value_);
    }
  };

  const char *value() const { return value_; }

  size_t pos() const { return pos_; }
  size_t end() const { return end_; }
  size_t index() const { return index_; }
  size_t length() const { return end_ - pos_ + 1; }

  bool is_quoted() const
  {
    return (value_ && value_[0] == '"') || (value_ && value_[0] == '\'');
  }

  char quote_char() const
  {
    if (!is_quoted())
    {
      return '\0';
    }

    return value_[0];
  }

  bool is_whitespace()
  {
    if (!value_)
    {
      return false;
    }

    const char *p = value_;
    while (*p)
    {
      if (!isspace((unsigned char)*p))
      {
        return false;
      }
      p++;
    }

    return true;
  }

  bool is_float()
  {
    if (!value_)
    {
      return false;
    }

    const char *p = value_;
    bool has_decimal = false;

    if (*p == '-' || *p == '+')
    {
      p++;
    }

    while (*p)
    {
      if (*p == '.')
      {
        if (has_decimal)
        {
          return false;
        }
        has_decimal = true;
      }
      else if (!isdigit((unsigned char)*p))
      {
        return false;
      }
      p++;
    }

    return has_decimal;
  }

  bool is_integer()
  {
    if (!value_)
    {
      return false;
    }

    const char *p = value_;

    if (*p == '-' || *p == '+')
    {
      p++;
    }

    if (!*p)
    {
      return false;
    }

    while (*p)
    {
      if (!isdigit((unsigned char)*p))
      {
        return false;
      }
      p++;
    }

    return true;
  }

  bool is_boolean()
  {
    if (!value_)
    {
      return false;
    }

    size_t i;
    for (i = 0; i < (sizeof(bool_words) / sizeof(bool_words[0])); i++)
    {
      if (strcmp(value_, bool_words[i].word) == 0)
      {
        return true;
      }
    }

    return false;
  }

  bool is_string_value()
  {
    return value_ != NULL && !is_integer() && !is_float() && !is_boolean();
  }

  void set_error(arg_parser_result error_code, const char *fmt, ...)
  {
    va_list args;
    char buffer[256];

    errors_.emplace_back();
    arg_parser_error &error = errors_.back();

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    error.token_index = index_;
    error.token_pos = pos_;
    error.message = strdup(buffer);
    error.value = value_ ? strdup(value_) : NULL;
    error.code = error_code;

    wizlog(MINLVLIMMORTAL, "ArgToken error at token %zu (pos %zu; value %s): %s", index_, pos_, value_ ? value_ : "NULL", buffer);
  }

  std::vector<arg_parser_error> get_errors() const
  {
    return errors_;
  }

private:
  char *value_;
  size_t pos_;
  size_t end_;
  size_t index_;

  std::vector<arg_parser_error> errors_;
};

class ArgParser
{
public:
  ArgParser(const arg_def *definitions, const arg_parser_options &options);

  ~ArgParser();

  arg_parser_result parse(const char *argument, arg_parser_output *out);

  void set_error(arg_parser_result error_code, const char *fmt, ...)
  {
    va_list args;
    char buffer[256];

    errors_.emplace_back();
    arg_parser_error &error = errors_.back();

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    error.message = strdup(buffer);
    error.code = error_code;

    wizlog(MINLVLIMMORTAL, "ArgParser error %d: %s", error_code, buffer);
  }

  std::vector<arg_parser_error> get_errors() const
  {
    return errors_;
  }

private:
  const arg_def *definitions_;
  std::vector<ArgToken> tokens_;
  arg_parser_options options_;
  std::vector<arg_parser_error> errors_;

  bool is_option_set(unsigned int bit)
  {
    return (options_.flags & bit) != 0;
  }

  void set_option(unsigned int bit)
  {
    if (!is_option_set(bit))
      options_.flags |= bit;
  }

  void clear_option(unsigned int bit)
  {
    if (is_option_set(bit))
      options_.flags &= ~bit;
  }

  bool arg_opt_use_fill_words() { return is_option_set(ARG_PARSER_OPT_USE_FILL_WORDS); }
  bool arg_opt_allow_trailing_junk() { return is_option_set(ARG_PARSER_OPT_ALLOW_TRAILING_JUNK); }
  bool arg_opt_delims_only() { return is_option_set(ARG_PARSER_OPT_DELIMS_ONLY); }
  bool arg_opt_whitespace_delims() { return !is_option_set(ARG_PARSER_OPT_DELIMS_ONLY); }
  bool arg_opt_allow_random_order() { return is_option_set(ARG_PARSER_OPT_ALLOW_RANDOM_ORDER); }
  bool arg_opt_case_sensitive() { return is_option_set(ARG_PARSER_OPT_CASE_SENSITIVE); }

  arg_parser_result next_token(const char *cursor, int pos, ArgToken &out_token);

  arg_parser_result validate_configuration();
  arg_parser_result tokenize_arguments(const char *argument);
  arg_parser_result match_arguments(arg_parser_output * out);
  arg_parser_result validate_arguments(arg_parser_output * out);
  arg_parser_result finalize_arguments(arg_parser_output * out);

  bool is_delim(char ch)
  {
    if (arg_opt_whitespace_delims() && isspace((unsigned char)ch))
    {
      return true;
    }

    if (options_.delimiters && strchr(options_.delimiters, ch))
    {
      return true;
    }

    return false;
  }

  bool is_singlequote(char ch)
  {
    return ch == '\'';
  }

  bool is_doublequote(char ch)
  {
    return ch == '"';
  }

  bool is_quote(char ch)
  {
    return is_singlequote(ch) || is_doublequote(ch);
  }

  bool is_eol(char ch)
  {
    return ch == '\0';
  }
};

static void arg_set_error(arg_parser_output *out, const char *fmt, ...);

ArgParser::ArgParser(const arg_def *definitions, const arg_parser_options &options)
{
  definitions_ = definitions;
  options_ = options;
};

ArgParser::~ArgParser() {

};

arg_parser_result ArgParser::parse(const char *argument, arg_parser_output *out)
{

  arg_parser_result result = ARG_PARSE_NONE;

  result = validate_configuration();
  if (ARG_RESULT_FAILURE(result))
  {
    return result;
  }

  // Tokenize
  result = tokenize_arguments(argument);
  if (ARG_RESULT_FAILURE(result))
  {
    return result;
  }

  // Match tokens against definitions
  result = match_arguments(out);
  if (ARG_RESULT_FAILURE(result))
  {
    return result;
  }

  // validate required arguments
  result = validate_arguments(out);
  if (ARG_RESULT_FAILURE(result))
  {
    return result;
  }

  // finalize output
  result = finalize_arguments(out);
  if (ARG_RESULT_FAILURE(result))
  {
    return result;
  }

  // todo: change to not failed when complete
  return ARG_PARSE_NOT_FOUND;
}

arg_parser_result ArgParser::validate_configuration()
{
  size_t count = 0;
  const arg_def *prev = NULL;
  arg_parser_result result = ARG_PARSE_NONE;
  bool case_sensitive = arg_opt_case_sensitive();

  if (!definitions_)
  {
    set_error(ARG_PARSE_INVALID_LIST | ARG_PARSE_ERROR, "No argument definitions provided");

    return ARG_PARSE_INVALID_LIST | ARG_PARSE_ERROR;
  }

  if (arg_opt_delims_only() && (!options_.delimiters || !*options_.delimiters))
  {
    set_error(ARG_PARSE_MISSING_DELIMITERS | ARG_PARSE_WARNING, "Delimiter-only mode requires delimiters, defaulting to whitespace");

    clear_option(ARG_PARSER_OPT_DELIMS_ONLY);

    result |= ARG_PARSE_MISSING_DELIMITERS | ARG_PARSE_WARNING;
  }

  for (const arg_def *def = definitions_; def && def->name; ++def)
  {
    arg_def *def_opts = const_cast<arg_def *>(def);

    if (!*def->name)
    {
      set_error(ARG_PARSE_MISSING_NAME | ARG_PARSE_ERROR, "Argument name is empty");

      result |= ARG_PARSE_MISSING_NAME | ARG_PARSE_ERROR;
    }

    if (def_opts->arg_opt_required() && def_opts->arg_opt_optional())
    {
      set_error(ARG_PARSE_INVALID_LIST | ARG_PARSE_ERROR, "Argument '%s' cannot be both required and optional", def->name);
    }

    if (!def_opts->arg_opt_required() && !def_opts->arg_opt_optional())
    {
      set_error(ARG_PARSE_INVALID_LIST | ARG_PARSE_ERROR, "Argument '%s' must be required or optional", def->name);
    }

    if (def_opts->arg_opt_depends_on_prev())
    {
      if (!prev)
      {
        set_error(ARG_PARSE_INVALID_LIST | ARG_PARSE_ERROR, "Argument '%s' depends on previous but is first", def->name);
      }
    }

    switch (def->type)
    {
    case ARG_TYPE_STRING:
      if (def_opts->arg_opt_abbrev() && def_opts->arg_opt_exact())
      {
        set_error(ARG_PARSE_INVALID_LIST | ARG_PARSE_WARNING, "Argument '%s' cannot be both abbreviated and exact, defaulting to exact", def->name);
      }

      if (def_opts->arg_opt_abbrev() && def_opts->arg_opt_regex())
      {
        set_error(ARG_PARSE_INVALID_LIST | ARG_PARSE_WARNING, "Argument '%s' cannot be both abbreviated and regex", def->name);
      }

      if (def_opts->arg_opt_exact() && def_opts->arg_opt_regex())
      {
        set_error(ARG_PARSE_INVALID_LIST | ARG_PARSE_WARNING, "Argument '%s' cannot be both exact and regex", def->name);
      }

      if (def_opts->arg_opt_quote_single() && def_opts->arg_opt_quote_double())
      {
        set_error(ARG_PARSE_INVALID_LIST | ARG_PARSE_WARNING, "Argument '%s' cannot be both single-quoted and double-quoted", def->name);
      }

      if ((!def->spec.string.pattern || !*def->spec.string.pattern))
      {
        set_error(ARG_PARSE_INVALID_LIST | ARG_PARSE_WARNING, "Argument '%s' is missing a pattern", def->name);
      }
      break;
    case ARG_TYPE_INT:
      if (def->spec.integer.min > def->spec.integer.max)
      {
        set_error(ARG_PARSE_INVALID_LIST | ARG_PARSE_WARNING, "Argument '%s' has invalid integer range", def->name);
      }
      break;
    case ARG_TYPE_FLOAT:
      if (def->spec.floating.min > def->spec.floating.max)
      {
        set_error(ARG_PARSE_INVALID_LIST | ARG_PARSE_WARNING, "Argument '%s' has invalid float range", def->name);
      }
      break;
    case ARG_TYPE_BOOL:
      break;
    case ARG_TYPE_REST_OF_LINE:
      break;
    default:
      set_error(ARG_PARSE_INVALID_LIST | ARG_PARSE_ERROR, "Argument '%s' has invalid type", def->name);
    }

    for (const arg_def *dup = definitions_; dup < def; ++dup)
    {
      if (!dup->name)
        break;

      bool same = case_sensitive ? (strcmp(dup->name, def->name) == 0)
                                 : (strcasecmp(dup->name, def->name) == 0);
      if (same)
      {
        set_error(ARG_PARSE_INVALID_LIST | ARG_PARSE_ERROR, "Duplicate argument name '%s'", def->name);
      }
    }

    prev = def;
    ++count;
  }

  if (count == 0)
  {
    set_error(ARG_PARSE_INVALID_LIST | ARG_PARSE_ERROR, "No argument definitions provided");
  }

  if (!ARG_RESULT_FAILURE(result))
  {
    result |= ARG_PARSE_OK;
  }

  return result;
}

arg_parser_result ArgParser::tokenize_arguments(const char *argument)
{
  size_t pos = 0;
  const char *cursor = argument;

  while (true)
  {
    ArgToken token;

    arg_parser_result result = next_token(cursor, pos, token);

    if (ARG_RESULT_FAILURE(result))
      return result;

    if (result == ARG_PARSE_EOL)
      break;

    cursor += (token.pos() - pos) + token.length();

    pos += (token.pos() - pos) + token.length();
  }

  return ARG_PARSE_OK;
}

arg_parser_result ArgParser::next_token(const char *cursor, int pos, ArgToken &out_token)
{
  if (!cursor)
    return ARG_PARSE_ERROR | ARG_PARSE_INVALID_VALUE;

  if (is_eol(*cursor))
    return ARG_PARSE_OK | ARG_PARSE_EOL;

  const char *scan = cursor;

  while (!is_eol(*scan) && is_delim(*scan))
  {
    ++scan;
    ++pos;
  }

  char quote_char = '\0';
  const char *start = scan;
  bool is_quoted = is_quote(*scan);
  bool is_quote_complete = false;

  if (is_quoted)
  {
    quote_char = *scan;
    ++scan;
  }

  while (!is_eol(*scan) && (is_quoted ? *scan != quote_char : !is_delim(*scan)))
    ++scan;

  if (is_quoted && *scan == quote_char)
  {
    is_quote_complete = true;
    ++scan;
  }

  size_t token_length = scan - start;
  char *token_value = (char *)malloc(token_length + 1);

  if (token_value)
  {
    strncpy(token_value, start, token_length);
    token_value[token_length] = '\0';
  }

  tokens_.emplace_back(token_value ? token_value : "", pos, pos + token_length - 1, tokens_.size());

  out_token = tokens_.back();

  if (!token_value)
    out_token.set_error(ARG_PARSE_NO_MEMORY | ARG_PARSE_ERROR, "Out of memory allocating token, length %zu", token_length);

  if (is_quoted && !is_quote_complete)
    out_token.set_error(ARG_PARSE_MISSING_QUOTE | ARG_PARSE_ERROR, "Unterminated quoted string, missing closing %c", quote_char);

  if (is_quoted && is_quote_complete && !is_delim((*scan + 1)) && !is_eol(*(scan + 1)))
    out_token.set_error(ARG_PARSE_JUNK | ARG_PARSE_WARNING, "Junk after closing quote: '%c'", *scan);

  if (token_value)
    free(token_value);

  return ARG_PARSE_OK;
}

arg_parser_result ArgParser::match_arguments(arg_parser_output *out)
{
  return ARG_PARSE_OK;
}

arg_parser_result ArgParser::validate_arguments(arg_parser_output *out)
{
  return ARG_PARSE_OK;
}

arg_parser_result ArgParser::finalize_arguments(arg_parser_output *out)
{
  return ARG_PARSE_OK;
}

static bool arg_is_delim_char(char ch, bool whitespace_delims, const char *delims)
{
  if (whitespace_delims && isspace((unsigned char)ch))
  {
    return true;
  }

  if (delims && strchr(delims, ch))
  {
    return true;
  }

  return false;
}

static bool arg_prefix_match(const char *prefix,
                             const char *word,
                             bool case_sensitive)
{
  size_t i;

  if (!prefix || !word)
  {
    return false;
  }

  for (i = 0; prefix[i]; i++)
  {
    char a = prefix[i];
    char b = word[i];

    if (!b)
    {
      return false;
    }

    if (!case_sensitive)
    {
      a = (char)tolower((unsigned char)a);
      b = (char)tolower((unsigned char)b);
    }

    if (a != b)
    {
      return false;
    }
  }

  return true;
}

static bool arg_word_equal(const char *a, const char *b, bool case_sensitive)
{
  if (case_sensitive)
  {
    return strcmp(a, b) == 0;
  }
  return strcasecmp(a, b) == 0;
}

static int arg_regex_full_match(const char *pattern,
                                const char *value,
                                bool case_sensitive,
                                char *err_buf,
                                size_t err_buf_size)
{
  regex_t regex;
  regmatch_t match;
  int flags = REG_EXTENDED;
  int rc;

  if (!pattern || !value)
  {
    return ARG_MATCH_NO;
  }

  if (!case_sensitive)
  {
    flags |= REG_ICASE;
  }

  rc = regcomp(&regex, pattern, flags);
  if (rc != 0)
  {
    if (err_buf && err_buf_size > 0)
    {
      regerror(rc, &regex, err_buf, err_buf_size);
    }
    return ARG_MATCH_ERROR;
  }

  rc = regexec(&regex, value, 1, &match, 0);
  if (rc != 0)
  {
    regfree(&regex);
    return (rc == REG_NOMATCH) ? ARG_MATCH_NO : ARG_MATCH_ERROR;
  }

  regfree(&regex);
  if (match.rm_so == 0 && match.rm_eo == (regoff_t)strlen(value))
  {
    return ARG_MATCH_OK;
  }

  return ARG_MATCH_NO;
}

static int arg_match_string(const arg_def *def,
                            const char *token,
                            bool was_quoted,
                            bool force_case_sensitive,
                            char *normalized,
                            size_t normalized_size,
                            char *err_buf,
                            size_t err_buf_size)
{
  const char *pattern;
  bool allow_abbrev;
  bool case_sensitive;

  if (!def || !token || !normalized || normalized_size == 0)
  {
    return ARG_MATCH_ERROR;
  }

  pattern = def->spec.string.pattern ? def->spec.string.pattern : ARG_PATTERN_WORD;
  // allow_abbrev = arg_option_abbrev(def->options);
  // case_sensitive = force_case_sensitive || arg_option_exact(def->options);

  if (was_quoted)
  {
    if (!def->spec.string.pattern ||
        strcmp(def->spec.string.pattern, ARG_PATTERN_WORD) == 0)
    {
      if (strlen(token) >= normalized_size)
      {
        return ARG_MATCH_ERROR;
      }
      memcpy(normalized, token, strlen(token) + 1);
      return ARG_MATCH_OK;
    }
  }

  if (allow_abbrev)
  {
    if (!def->spec.string.pattern || !*def->spec.string.pattern)
    {
      return ARG_MATCH_ERROR;
    }

    if (!arg_prefix_match(token, pattern, case_sensitive))
    {
      return ARG_MATCH_NO;
    }

    if (strlen(pattern) >= normalized_size)
    {
      return ARG_MATCH_ERROR;
    }

    memcpy(normalized, pattern, strlen(pattern) + 1);
    return ARG_MATCH_OK;
  }

  if (err_buf && err_buf_size > 0)
  {
    err_buf[0] = '\0';
  }

  switch (arg_regex_full_match(pattern, token, case_sensitive, err_buf, err_buf_size))
  {
  case ARG_MATCH_OK:
    strncpy(normalized, token, normalized_size);
    normalized[normalized_size - 1] = '\0';
    return ARG_MATCH_OK;
  case ARG_MATCH_NO:
    return ARG_MATCH_NO;
  default:
    return ARG_MATCH_ERROR;
  }
}

static int arg_match_int(const arg_def *def,
                         const char *token,
                         int *out_value)
{
  char *end = NULL;
  long value;

  if (!def || !token || !out_value)
  {
    return ARG_MATCH_ERROR;
  }

  if (def->spec.integer.min > def->spec.integer.max)
  {
    return ARG_MATCH_ERROR;
  }

  errno = 0;
  value = strtol(token, &end, 10);
  if (errno == ERANGE || end == token || *end != '\0')
  {
    return ARG_MATCH_NO;
  }

  if (value < def->spec.integer.min || value > def->spec.integer.max)
  {
    return ARG_MATCH_NO;
  }

  if (value < INT_MIN || value > INT_MAX)
  {
    return ARG_MATCH_NO;
  }

  *out_value = (int)value;
  return ARG_MATCH_OK;
}

static int arg_match_float(const arg_def *def,
                           const char *token,
                           float *out_value)
{
  char *end = NULL;
  float value;

  if (!def || !token || !out_value)
  {
    return ARG_MATCH_ERROR;
  }

  if (def->spec.floating.min > def->spec.floating.max)
  {
    return ARG_MATCH_ERROR;
  }

  errno = 0;
  value = strtof(token, &end);
  if (errno == ERANGE || end == token || *end != '\0')
  {
    return ARG_MATCH_NO;
  }

  if (value != value)
  {
    // Reject NaN.
    return ARG_MATCH_NO;
  }

  if (value < def->spec.floating.min || value > def->spec.floating.max)
  {
    return ARG_MATCH_NO;
  }

  *out_value = value;
  return ARG_MATCH_OK;
}

static int arg_match_bool(const arg_def *def,
                          const char *token,
                          bool force_case_sensitive,
                          bool *out_value)
{
  size_t i;
  int matches = 0;
  bool allow_abbrev;
  bool case_sensitive;
  bool value = false;

  if (!def || !token || !out_value)
  {
    return ARG_MATCH_ERROR;
  }

  if (strcmp(token, "1") == 0)
  {
    *out_value = true;
    return ARG_MATCH_OK;
  }

  if (strcmp(token, "0") == 0)
  {
    *out_value = false;
    return ARG_MATCH_OK;
  }

  // allow_abbrev = arg_option_abbrev(def->options);
  // case_sensitive = force_case_sensitive || arg_option_exact(def->options);

  for (i = 0; i < (sizeof(bool_words) / sizeof(bool_words[0])); i++)
  {
    if (allow_abbrev)
    {
      if (arg_prefix_match(token, bool_words[i].word, case_sensitive))
      {
        value = bool_words[i].value;
        matches++;
      }
    }
    else
    {
      if (arg_word_equal(token, bool_words[i].word, case_sensitive))
      {
        value = bool_words[i].value;
        matches++;
      }
    }
  }

  if (matches == 1)
  {
    *out_value = value;
    return ARG_MATCH_OK;
  }

  if (matches > 1)
  {
    return ARG_MATCH_AMBIGUOUS;
  }

  return ARG_MATCH_NO;
}

arg_parser_result parse_arguments(const char *argument,
                                  const arg_def *list,
                                  const arg_parser_options &options,
                                  arg_parser_output *out)
{

  ArgParser parser(list, options);
  return parser.parse(argument, out);
}

static const arg_value *arg_find_value(const arg_parser_output *out,
                                       const char *name)
{
  size_t i;

  if (!out || !out->items || !out->values || !name)
  {
    return NULL;
  }

  for (i = 0; i < out->count; i++)
  {
    const char *def_name = out->items[i].name;

    if (!def_name)
    {
      continue;
    }

    if (strcasecmp(def_name, name) == 0)
    {
      return &out->values[i];
    }
  }

  return NULL;
}

arg_parser_result parse_argument(const char *name,
                                 const arg_parser_output &parsed,
                                 const char **value)
{
  const arg_value *found = arg_find_value(&parsed, name);

  if (!found)
  {
    return ARG_PARSE_NOT_FOUND;
  }

  if (found->type != ARG_TYPE_STRING)
  {
    return ARG_PARSE_TYPE_MISMATCH;
  }

  if (!found->present)
  {
    return ARG_PARSE_MISSING;
  }

  if (value)
  {
    *value = found->as.string;
  }

  return ARG_PARSE_OK;
}

arg_parser_result parse_argument(const char *name,
                                 const arg_parser_output &parsed,
                                 int *value)
{
  const arg_value *found = arg_find_value(&parsed, name);

  if (!found)
  {
    return ARG_PARSE_NOT_FOUND;
  }

  if (found->type != ARG_TYPE_INT)
  {
    return ARG_PARSE_TYPE_MISMATCH;
  }

  if (!found->present)
  {
    return ARG_PARSE_MISSING;
  }

  if (value)
  {
    *value = found->as.integer;
  }

  return ARG_PARSE_OK;
}

arg_parser_result parse_argument(const char *name,
                                 const arg_parser_output &parsed,
                                 bool *value)
{
  const arg_value *found = arg_find_value(&parsed, name);

  if (!found)
  {
    return ARG_PARSE_NOT_FOUND;
  }

  if (found->type != ARG_TYPE_BOOL)
  {
    return ARG_PARSE_TYPE_MISMATCH;
  }

  if (!found->present)
  {
    return ARG_PARSE_MISSING;
  }

  if (value)
  {
    *value = found->as.boolean;
  }

  return ARG_PARSE_OK;
}

arg_parser_result parse_argument(const char *name,
                                 const arg_parser_output &parsed,
                                 float *value)
{
  const arg_value *found = arg_find_value(&parsed, name);

  if (!found)
  {
    return ARG_PARSE_NOT_FOUND;
  }

  if (found->type != ARG_TYPE_FLOAT)
  {
    return ARG_PARSE_TYPE_MISMATCH;
  }

  if (!found->present)
  {
    return ARG_PARSE_MISSING;
  }

  if (value)
  {
    *value = found->as.floating;
  }

  return ARG_PARSE_OK;
}

// #define PARSER_UNIT_TEST
#ifdef PARSER_UNIT_TEST
#include <assert.h>

static arg_parser_result arg_parse_with_list(const char *argument,
                                             const arg_def *defs,
                                             size_t count,
                                             const arg_parser_options *options,
                                             arg_parser_output &out)
{
  const arg_list list = {defs, count};
  return parse_arguments(argument, &list, options, out);
}

static void arg_expect_result(const char *argument,
                              const arg_def *defs,
                              size_t count,
                              const arg_parser_options *options,
                              arg_parser_result expected)
{
  arg_parser_output parsed;
  arg_parser_result rc = arg_parse_with_list(argument, defs, count, options, parsed);
  assert(rc == expected);
}

static void parser_unit_test_basic_required_optional(void)
{
  const arg_def defs[] = {
      define_argument("cmd", ARG_PATTERN_WORD, ARG_OPT_REQUIRED),
      define_argument("count", 1, 10, ARG_OPT_OPTIONAL),
      define_argument("flag", ARG_OPT_OPTIONAL),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;

  arg_parser_output parsed;
  arg_parser_result rc = parse_arguments("foo 5 yes", defs, opts, &parsed);
  assert(rc == ARG_PARSE_OK);

  const char *cmd = NULL;
  int count = 0;
  bool flag = false;
  assert(parse_argument("cmd", parsed, &cmd) == ARG_PARSE_OK);
  assert(strcmp(cmd, "foo") == 0);
  assert(parse_argument("count", parsed, &count) == ARG_PARSE_OK);
  assert(count == 5);
  assert(parse_argument("flag", parsed, &flag) == ARG_PARSE_OK);
  assert(flag == true);
}

static void parser_unit_test_required_only(void)
{
  const arg_def defs[] = {
      define_argument("cmd", ARG_PATTERN_WORD, ARG_OPT_REQUIRED),
      define_argument("count", 1, 10, ARG_OPT_OPTIONAL),
      define_argument("flag", ARG_OPT_OPTIONAL),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;

  arg_parser_output parsed;
  arg_parser_result rc = parse_arguments("foo", defs, &opts, parsed);
  assert(rc == ARG_PARSE_OK);

  const char *cmd = NULL;
  assert(parse_argument("cmd", parsed, &cmd) == ARG_PARSE_OK);
  assert(strcmp(cmd, "foo") == 0);
  assert(parse_argument("count", parsed, (int *)NULL) == ARG_PARSE_MISSING);
  assert(parse_argument("flag", parsed, (bool *)NULL) == ARG_PARSE_MISSING);
}

static void parser_unit_test_invalid_literal_string(void)
{
  const arg_def defs[] = {
      define_argument("cmd", "foo", ARG_OPT_REQUIRED),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;
  arg_expect_result("bar", defs, 1, &opts, ARG_PARSE_INVALID_VALUE);
}

static void parser_unit_test_missing_required(void)
{
  const arg_def defs[] = {
      define_argument("count", 0, 3, ARG_OPT_REQUIRED),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;
  arg_expect_result("", defs, 1, &opts, ARG_PARSE_MISSING_REQUIRED);
}

static void parser_unit_test_extra_argument(void)
{
  const arg_def defs[] = {
      define_argument("count", 0, 10, ARG_OPT_REQUIRED),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;
  arg_expect_result("1 2", defs, 1, &opts, ARG_PARSE_EXTRA_ARGUMENT);
}

static void parser_unit_test_unterminated_quote_double(void)
{
  const arg_def defs[] = {
      define_argument("msg", ARG_PATTERN_WORD, ARG_OPT_REQUIRED | ARG_OPT_QUOTE_DOUBLE),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;
  arg_expect_result("\"foo", defs, 1, &opts, ARG_PARSE_INVALID_VALUE);
}

static void parser_unit_test_quoted_double_ok(void)
{
  const arg_def defs[] = {
      define_argument("msg", ARG_PATTERN_WORD, ARG_OPT_REQUIRED | ARG_OPT_QUOTE_DOUBLE),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;

  arg_parser_output parsed;
  arg_parser_result rc = parse_arguments("\"foo bar\"", defs, &opts, parsed);
  assert(rc == ARG_PARSE_OK);

  const char *msg = NULL;
  assert(parse_argument("msg", parsed, &msg) == ARG_PARSE_OK);
  assert(strcmp(msg, "foo bar") == 0);
}

static void parser_unit_test_quoted_single_ok(void)
{
  const arg_def defs[] = {
      define_argument("msg", ARG_PATTERN_WORD, ARG_OPT_REQUIRED | ARG_OPT_QUOTE_SINGLE),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;

  arg_parser_output parsed;
  arg_parser_result rc = parse_arguments("'foo bar'", defs, &opts, parsed);
  assert(rc == ARG_PARSE_OK);

  const char *msg = NULL;
  assert(parse_argument("msg", parsed, &msg) == ARG_PARSE_OK);
  assert(strcmp(msg, "foo bar") == 0);
}

static void parser_unit_test_junk_after_quote(void)
{
  const arg_def defs[] = {
      define_argument("msg", ARG_PATTERN_WORD, ARG_OPT_REQUIRED | ARG_OPT_QUOTE_DOUBLE),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;
  arg_expect_result("\"foo\"bar", defs, 1, &opts, ARG_PARSE_INVALID_VALUE);
}

static void parser_unit_test_case_insensitive_ok(void)
{
  const arg_def defs[] = {
      define_argument("word", "Foo", ARG_OPT_REQUIRED),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;
  arg_expect_result("foo", defs, 1, &opts, ARG_PARSE_OK);
}

static void parser_unit_test_case_sensitive_invalid(void)
{
  const arg_def defs[] = {
      define_argument("word", "Foo", ARG_OPT_REQUIRED),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_CASE_SENSITIVE;
  opts.delimiters = NULL;
  arg_expect_result("foo", defs, 1, &opts, ARG_PARSE_INVALID_VALUE);
}

static void parser_unit_test_abbrev_ok(void)
{
  const arg_def defs[] = {
      define_argument("dir", "north", ARG_OPT_REQUIRED | ARG_OPT_ABBREV),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;

  arg_parser_output parsed;
  arg_parser_result rc = parse_arguments("n", defs, &opts, parsed);
  assert(rc == ARG_PARSE_OK);

  const char *dir = NULL;
  assert(parse_argument("dir", parsed, &dir) == ARG_PARSE_OK);
  assert(strcmp(dir, "north") == 0);
}

static void parser_unit_test_abbrev_invalid(void)
{
  const arg_def defs[] = {
      define_argument("flag", ARG_OPT_REQUIRED | ARG_OPT_ABBREV),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;
  arg_expect_result("o", defs, 1, &opts, ARG_PARSE_INVALID_VALUE);
}

static void parser_unit_test_float_out_of_range(void)
{
  const arg_def defs[] = {
      define_argument("ratio", 0.0f, 1.0f, ARG_OPT_REQUIRED),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;
  arg_expect_result("1.5", defs, 1, &opts, ARG_PARSE_INVALID_VALUE);
}

static void parser_unit_test_random_order_ok(void)
{
  const arg_def defs[] = {
      define_argument("num", 0, 10, ARG_OPT_REQUIRED),
      define_argument("flag", ARG_OPT_REQUIRED),
      define_argument("word", ARG_PATTERN_WORD, ARG_OPT_REQUIRED),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_ALLOW_RANDOM_ORDER;
  opts.delimiters = NULL;

  arg_parser_output parsed;
  arg_parser_result rc = parse_arguments("yes 3 foo", defs, &opts, parsed);
  assert(rc == ARG_PARSE_OK);

  int num = 0;
  bool flag = false;
  const char *word = NULL;
  assert(parse_argument("num", parsed, &num) == ARG_PARSE_OK);
  assert(parse_argument("flag", parsed, &flag) == ARG_PARSE_OK);
  assert(parse_argument("word", parsed, &word) == ARG_PARSE_OK);
  assert(num == 3);
  assert(flag == true);
  assert(strcmp(word, "foo") == 0);
}

static void parser_unit_test_random_order_extra_argument(void)
{
  const arg_def defs[] = {
      define_argument("a", 0, 10, ARG_OPT_REQUIRED),
      define_argument("b", 0, 10, ARG_OPT_REQUIRED),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_ALLOW_RANDOM_ORDER;
  opts.delimiters = NULL;
  arg_expect_result("1 2 3", defs, 2, &opts, ARG_PARSE_EXTRA_ARGUMENT);
}

static void parser_unit_test_depends_on_prev_ok(void)
{
  const arg_def defs[] = {
      define_argument("a", 0, 10, ARG_OPT_OPTIONAL),
      define_argument("b", ARG_PATTERN_WORD, ARG_OPT_OPTIONAL | ARG_OPT_DEPENDS_ON_PREV),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;

  arg_parser_output parsed;
  arg_parser_result rc = parse_arguments("5 foo", defs, &opts, parsed);
  assert(rc == ARG_PARSE_OK);

  int a = 0;
  const char *b = NULL;
  assert(parse_argument("a", parsed, &a) == ARG_PARSE_OK);
  assert(parse_argument("b", parsed, &b) == ARG_PARSE_OK);
  assert(a == 5);
  assert(strcmp(b, "foo") == 0);
}

static void parser_unit_test_depends_on_prev_invalid(void)
{
  const arg_def defs[] = {
      define_argument("a", 0, 10, ARG_OPT_OPTIONAL),
      define_argument("b", ARG_PATTERN_WORD, ARG_OPT_OPTIONAL | ARG_OPT_DEPENDS_ON_PREV),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;
  arg_expect_result("foo", defs, 2, &opts, ARG_PARSE_EXTRA_ARGUMENT);
}

static void parser_unit_test_delims_only_ok(void)
{
  const arg_def defs[] = {
      define_argument("a", 0, 10, ARG_OPT_REQUIRED),
      define_argument("b", 0, 10, ARG_OPT_REQUIRED),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_DELIMS_ONLY;
  opts.delimiters = ",";

  arg_parser_output parsed;
  arg_parser_result rc = parse_arguments("1,2", defs, &opts, parsed);
  assert(rc == ARG_PARSE_OK);

  int a = 0;
  int b = 0;
  assert(parse_argument("a", parsed, &a) == ARG_PARSE_OK);
  assert(parse_argument("b", parsed, &b) == ARG_PARSE_OK);
  assert(a == 1);
  assert(b == 2);
}

static void parser_unit_test_delims_only_missing_set(void)
{
  const arg_def defs[] = {
      define_argument("a", 0, 10, ARG_OPT_REQUIRED),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_DELIMS_ONLY;
  opts.delimiters = NULL;
  arg_expect_result("1", defs, 1, &opts, ARG_PARSE_INVALID_LIST);
}

static void parser_unit_test_allow_trailing_junk_ok(void)
{
  const arg_def defs[] = {
      define_argument("a", 0, 10, ARG_OPT_REQUIRED),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_ALLOW_TRAILING_JUNK;
  opts.delimiters = NULL;

  arg_parser_output parsed;
  arg_parser_result rc = parse_arguments("1 extra", defs, &opts, parsed);
  assert(rc == ARG_PARSE_OK);

  int a = 0;
  assert(parse_argument("a", parsed, &a) == ARG_PARSE_OK);
  assert(a == 1);
}

static void parser_unit_test_invalid_list_required_optional(void)
{
  const arg_def defs[] = {
      define_argument("bad", ARG_PATTERN_WORD, ARG_OPT_REQUIRED | ARG_OPT_OPTIONAL),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;
  arg_expect_result("foo", defs, 1, &opts, ARG_PARSE_INVALID_LIST);
}

static void parser_unit_test_argument_too_long(void)
{
  char long_token[MAX_INPUT_LENGTH + 1];
  memset(long_token, 'a', sizeof(long_token) - 1);
  long_token[sizeof(long_token) - 1] = '\0';

  const arg_def defs[] = {
      define_argument("long", ARG_PATTERN_WORD, ARG_OPT_REQUIRED),
  };
  arg_parser_options opts;
  opts.flags = ARG_PARSER_OPT_NONE;
  opts.delimiters = NULL;
  arg_expect_result(long_token, defs, 1, &opts, ARG_PARSE_ARGUMENT_TOO_LONG);
}

void parser_unit_test(void)
{
  parser_unit_test_basic_required_optional();
  parser_unit_test_required_only();
  parser_unit_test_invalid_literal_string();
  parser_unit_test_missing_required();
  parser_unit_test_extra_argument();
  parser_unit_test_unterminated_quote_double();
  parser_unit_test_quoted_double_ok();
  parser_unit_test_quoted_single_ok();
  parser_unit_test_junk_after_quote();
  parser_unit_test_case_insensitive_ok();
  parser_unit_test_case_sensitive_invalid();
  parser_unit_test_abbrev_ok();
  parser_unit_test_abbrev_invalid();
  parser_unit_test_float_out_of_range();
  parser_unit_test_random_order_ok();
  parser_unit_test_random_order_extra_argument();
  parser_unit_test_depends_on_prev_ok();
  parser_unit_test_depends_on_prev_invalid();
  parser_unit_test_delims_only_ok();
  parser_unit_test_delims_only_missing_set();
  parser_unit_test_allow_trailing_junk_ok();
  parser_unit_test_invalid_list_required_optional();
  parser_unit_test_argument_too_long();
}
#endif
