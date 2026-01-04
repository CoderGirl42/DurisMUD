// parser.c

#include "parser.h"

#include "prototypes.h"

#include <ctype.h>
#include <errno.h>
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
  ARG_TOKEN_TOO_LONG = -1
};

enum arg_match_status
{
  ARG_MATCH_NO = 0,
  ARG_MATCH_OK = 1,
  ARG_MATCH_ERROR = -1,
  ARG_MATCH_AMBIGUOUS = -2
};

arg_parser_output::arg_parser_output()
    : items(NULL),
      count(0),
      values(NULL),
      error(NULL)
{
}

arg_parser_output::~arg_parser_output()
{
  free_parsed_arguments(this);
}

static void arg_set_error(arg_parser_output *out, const char *fmt, ...)
{
  char buf[MAX_INPUT_LENGTH];
  va_list args;

  if (!out)
  {
    return;
  }

  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if (out->error)
  {
    free(out->error);
  }
  out->error = strdup(buf);
}

static void arg_release_values(arg_parser_output *out)
{
  size_t i;

  if (!out || !out->values)
  {
    return;
  }

  for (i = 0; i < out->count; i++)
  {
    if (out->values[i].present &&
        out->values[i].type == ARG_TYPE_STRING &&
        out->values[i].as.string)
    {
      free(out->values[i].as.string);
      out->values[i].as.string = NULL;
    }
  }

  free(out->values);
  out->values = NULL;
}

static void arg_release_tokens(char **tokens, size_t count)
{
  size_t i;

  if (!tokens)
  {
    return;
  }

  for (i = 0; i < count; i++)
  {
    free(tokens[i]);
  }

  free(tokens);
}

static bool arg_option_required(unsigned int options)
{
  return (options & ARG_OPT_REQUIRED) != 0;
}

static bool arg_option_optional(unsigned int options)
{
  return (options & ARG_OPT_OPTIONAL) != 0;
}

static bool arg_option_abbrev(unsigned int options)
{
  return (options & ARG_OPT_ABBREV) != 0;
}

static bool arg_option_exact(unsigned int options)
{
  return (options & ARG_OPT_EXACT) != 0;
}

static bool arg_option_depends_on_prev(unsigned int options)
{
  return (options & ARG_OPT_DEPENDS_ON_PREV) != 0;
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

static int arg_next_token(const char *argument,
                          bool use_fill_words,
                          bool whitespace_delims,
                          const char *delims,
                          char *out,
                          size_t out_size,
                          const char **next)
{
  const char *cursor = argument;

  if (!out || out_size == 0)
  {
    return ARG_TOKEN_TOO_LONG;
  }

  if (!cursor)
  {
    if (next)
    {
      *next = NULL;
    }
    return ARG_TOKEN_NONE;
  }

  for (;;)
  {
    size_t begin = 0;
    size_t look_at = 0;

    out[0] = '\0';

    while (cursor[begin] && arg_is_delim_char(cursor[begin], whitespace_delims, delims))
    {
      begin++;
    }

    if (!cursor[begin])
    {
      if (next)
      {
        *next = cursor + begin;
      }
      return ARG_TOKEN_NONE;
    }

    while (cursor[begin + look_at] &&
           !arg_is_delim_char(cursor[begin + look_at], whitespace_delims, delims))
    {
      if (look_at + 1 >= out_size)
      {
        out[0] = '\0';
        if (next)
        {
          *next = NULL;
        }
        return ARG_TOKEN_TOO_LONG;
      }

      out[look_at] = cursor[begin + look_at];
      look_at++;
    }

    out[look_at] = '\0';

    {
      const char *new_cursor = cursor + begin + look_at;

      if (next)
      {
        *next = new_cursor;
      }

      if (use_fill_words)
      {
        char check[MAX_INPUT_LENGTH];

        strncpy(check, out, sizeof(check));
        check[sizeof(check) - 1] = '\0';

        if (fill_word(check))
        {
          cursor = new_cursor;
          continue;
        }
      }
    }

    return ARG_TOKEN_OK;
  }
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
  allow_abbrev = arg_option_abbrev(def->options);
  case_sensitive = arg_option_exact(def->options);

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

  if (value < def->spec.floating.min || value > def->spec.floating.max)
  {
    return ARG_MATCH_NO;
  }

  *out_value = value;
  return ARG_MATCH_OK;
}

static int arg_match_bool(const arg_def *def,
                          const char *token,
                          bool *out_value)
{
  struct bool_word
  {
    const char *word;
    bool value;
  };
  const struct bool_word words[] = {
      {"true", true},
      {"false", false},
      {"yes", true},
      {"no", false},
      {"on", true},
      {"off", false}};
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

  allow_abbrev = arg_option_abbrev(def->options);
  case_sensitive = arg_option_exact(def->options);

  for (i = 0; i < (sizeof(words) / sizeof(words[0])); i++)
  {
    if (allow_abbrev)
    {
      if (arg_prefix_match(token, words[i].word, case_sensitive))
      {
        value = words[i].value;
        matches++;
      }
    }
    else
    {
      if (arg_word_equal(token, words[i].word, case_sensitive))
      {
        value = words[i].value;
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

static arg_parse_code arg_validate_list(const arg_list *list,
                                        arg_parser_output *out)
{
  size_t i;

  if (!list)
  {
    arg_set_error(out, "Argument list is null.");
    return ARG_PARSE_INVALID_LIST;
  }

  if (list->count > 0 && !list->items)
  {
    arg_set_error(out, "Argument list items are null.");
    return ARG_PARSE_INVALID_LIST;
  }

  for (i = 0; i < list->count; i++)
  {
    size_t j;
    const arg_def *def = &list->items[i];
    bool required = arg_option_required(def->options);
    bool optional = arg_option_optional(def->options);

    if (!def->name || !*def->name)
    {
      arg_set_error(out, "Argument %lu has no name.", (unsigned long)i);
      return ARG_PARSE_INVALID_LIST;
    }

    if (required == optional)
    {
      arg_set_error(out,
                    "Argument %s must be marked required or optional.",
                    def->name);
      return ARG_PARSE_INVALID_LIST;
    }

    if (arg_option_depends_on_prev(def->options) && i == 0)
    {
      arg_set_error(out,
                    "Argument %s cannot depend on a previous argument.",
                    def->name);
      return ARG_PARSE_INVALID_LIST;
    }

    for (j = 0; j < i; j++)
    {
      if (strcasecmp(def->name, list->items[j].name) == 0)
      {
        arg_set_error(out, "Duplicate argument name: %s.", def->name);
        return ARG_PARSE_INVALID_LIST;
      }
    }

    switch (def->type)
    {
    case ARG_TYPE_STRING:
    {
      if (arg_option_abbrev(def->options))
      {
        const char *pattern = def->spec.string.pattern;
        const char *p;

        if (!pattern || !*pattern)
        {
          arg_set_error(out,
                        "Abbrev option requires a pattern for %s.",
                        def->name);
          return ARG_PARSE_INVALID_LIST;
        }

        for (p = pattern; *p; p++)
        {
          if (isspace((unsigned char)*p))
          {
            arg_set_error(out,
                          "Abbrev pattern for %s must be a single token.",
                          def->name);
            return ARG_PARSE_INVALID_LIST;
          }
        }
      }
      else if (def->spec.string.pattern)
      {
        regex_t regex;
        int rc = regcomp(&regex, def->spec.string.pattern, REG_EXTENDED);
        if (rc != 0)
        {
          char buf[MAX_INPUT_LENGTH];

          regerror(rc, &regex, buf, sizeof(buf));
          arg_set_error(out, "Invalid regex for %s: %s.", def->name, buf);
          return ARG_PARSE_INVALID_LIST;
        }
        regfree(&regex);
      }
      break;
    }

    case ARG_TYPE_INT:
    {
      if (def->spec.integer.min > def->spec.integer.max)
      {
        arg_set_error(out, "Invalid integer range for %s.", def->name);
        return ARG_PARSE_INVALID_LIST;
      }
      break;
    }

    case ARG_TYPE_FLOAT:
    {
      if (def->spec.floating.min > def->spec.floating.max)
      {
        arg_set_error(out, "Invalid float range for %s.", def->name);
        return ARG_PARSE_INVALID_LIST;
      }
      break;
    }

    case ARG_TYPE_BOOL:
      break;

    default:
      arg_set_error(out, "Unknown argument type for %s.", def->name);
      return ARG_PARSE_INVALID_LIST;
    }
  }

  return ARG_PARSE_OK;
}

arg_parse_code parse_arguments(const char *argument,
                               const arg_list *list,
                               const arg_parser_options *options,
                               arg_parser_output &out)
{
  const char *cursor = argument ? argument : "";
  unsigned int flags = options ? options->flags : ARG_PARSER_OPT_NONE;
  bool use_fill_words = (flags & ARG_PARSER_OPT_USE_FILL_WORDS) != 0;
  bool allow_trailing = (flags & ARG_PARSER_OPT_ALLOW_TRAILING_JUNK) != 0;
  bool allow_random_order = (flags & ARG_PARSER_OPT_ALLOW_RANDOM_ORDER) != 0;
  size_t i;
  arg_parse_code rc;
  const char *delims = options ? options->delimiters : NULL;
  bool whitespace_delims = (flags & ARG_PARSER_OPT_DELIMS_ONLY) == 0;

  free_parsed_arguments(&out);
  out.items = NULL;
  out.count = 0;
  out.values = NULL;
  out.error = NULL;

  rc = arg_validate_list(list, &out);
  if (rc != ARG_PARSE_OK)
  {
    return rc;
  }

  if (!whitespace_delims && (!delims || !*delims))
  {
    arg_set_error(&out, "Delimiter options require a delimiter set.");
    return ARG_PARSE_INVALID_LIST;
  }

  if (list)
  {
    out.items = list->items;
    out.count = list->count;
  }

  if (out.count > 0)
  {
    out.values = (arg_value *)calloc(out.count, sizeof(*out.values));
    if (!out.values)
    {
      arg_set_error(&out, "Out of memory.");
      return ARG_PARSE_NO_MEMORY;
    }
  }

  if (allow_random_order)
  {
    const char *token_cursor = cursor;
    char **tokens = NULL;
    bool *used = NULL;
    size_t token_count = 0;
    size_t token_cap = 0;
    arg_parse_code rc_random = ARG_PARSE_OK;

    for (;;)
    {
      char token[MAX_INPUT_LENGTH];
      const char *next = NULL;
      int status = arg_next_token(token_cursor,
                                  use_fill_words,
                                  whitespace_delims,
                                  delims,
                                  token,
                                  sizeof(token),
                                  &next);

      if (status == ARG_TOKEN_TOO_LONG)
      {
        arg_set_error(&out, "Argument too long.");
        rc_random = ARG_PARSE_ARGUMENT_TOO_LONG;
        goto random_cleanup;
      }

      if (status == ARG_TOKEN_NONE)
      {
        break;
      }

      {
        char *dup = strdup(token);

        if (!dup)
        {
          arg_set_error(&out, "Out of memory.");
          rc_random = ARG_PARSE_NO_MEMORY;
          goto random_cleanup;
        }

        if (token_count == token_cap)
        {
          size_t new_cap = token_cap ? token_cap * 2 : 4;
          char **new_tokens = (char **)realloc(tokens, new_cap * sizeof(*tokens));

          if (!new_tokens)
          {
            free(dup);
            arg_set_error(&out, "Out of memory.");
            rc_random = ARG_PARSE_NO_MEMORY;
            goto random_cleanup;
          }

          tokens = new_tokens;
          token_cap = new_cap;
        }

        tokens[token_count++] = dup;
      }

      token_cursor = next;
    }

    if (token_count > 0)
    {
      used = (bool *)calloc(token_count, sizeof(*used));
      if (!used)
      {
        arg_set_error(&out, "Out of memory.");
        rc_random = ARG_PARSE_NO_MEMORY;
        goto random_cleanup;
      }
    }

    for (i = 0; i < out.count; i++)
    {
      const arg_def *def = &out.items[i];
      bool depends_on_prev = arg_option_depends_on_prev(def->options);
      bool matched = false;
      size_t t;

      if (depends_on_prev)
      {
        if (i == 0 || !out.values[i - 1].present)
        {
          continue;
        }
      }

      out.values[i].type = def->type;

      for (t = 0; t < token_count; t++)
      {
        char normalized[MAX_INPUT_LENGTH];
        char regex_err[MAX_INPUT_LENGTH];
        int match_status;

        if (used[t])
        {
          continue;
        }

        switch (def->type)
        {
        case ARG_TYPE_STRING:
        {
          regex_err[0] = '\0';
          match_status = arg_match_string(def,
                                          tokens[t],
                                          normalized,
                                          sizeof(normalized),
                                          regex_err,
                                          sizeof(regex_err));
          if (match_status == ARG_MATCH_ERROR)
          {
            arg_set_error(&out,
                          "Invalid string pattern for %s: %s.",
                          def->name,
                          regex_err[0] ? regex_err : "Invalid pattern");
            rc_random = ARG_PARSE_INVALID_LIST;
            goto random_cleanup;
          }
          if (match_status == ARG_MATCH_AMBIGUOUS)
          {
            arg_set_error(&out,
                          "Ambiguous abbreviation for %s: %s.",
                          def->name,
                          tokens[t]);
            rc_random = ARG_PARSE_INVALID_VALUE;
            goto random_cleanup;
          }
          if (match_status == ARG_MATCH_NO)
          {
            continue;
          }

          out.values[i].present = true;
          out.values[i].as.string = strdup(normalized);
          if (!out.values[i].as.string)
          {
            arg_set_error(&out, "Out of memory.");
            rc_random = ARG_PARSE_NO_MEMORY;
            goto random_cleanup;
          }

          used[t] = true;
          matched = true;
          break;
        }

        case ARG_TYPE_INT:
        {
          match_status = arg_match_int(def, tokens[t], &out.values[i].as.integer);

          if (match_status == ARG_MATCH_ERROR)
          {
            arg_set_error(&out, "Invalid integer range for %s.", def->name);
            rc_random = ARG_PARSE_INVALID_LIST;
            goto random_cleanup;
          }
          if (match_status == ARG_MATCH_NO)
          {
            continue;
          }

          out.values[i].present = true;
          used[t] = true;
          matched = true;
          break;
        }

        case ARG_TYPE_FLOAT:
        {
          match_status = arg_match_float(def, tokens[t], &out.values[i].as.floating);

          if (match_status == ARG_MATCH_ERROR)
          {
            arg_set_error(&out, "Invalid float range for %s.", def->name);
            rc_random = ARG_PARSE_INVALID_LIST;
            goto random_cleanup;
          }
          if (match_status == ARG_MATCH_NO)
          {
            continue;
          }

          out.values[i].present = true;
          used[t] = true;
          matched = true;
          break;
        }

        case ARG_TYPE_BOOL:
        {
          match_status = arg_match_bool(def, tokens[t], &out.values[i].as.boolean);

          if (match_status == ARG_MATCH_AMBIGUOUS)
          {
            arg_set_error(&out,
                          "Ambiguous boolean for %s: %s.",
                          def->name,
                          tokens[t]);
            rc_random = ARG_PARSE_INVALID_VALUE;
            goto random_cleanup;
          }
          if (match_status == ARG_MATCH_ERROR)
          {
            arg_set_error(&out, "Invalid boolean list for %s.", def->name);
            rc_random = ARG_PARSE_INVALID_LIST;
            goto random_cleanup;
          }
          if (match_status == ARG_MATCH_NO)
          {
            continue;
          }

          out.values[i].present = true;
          used[t] = true;
          matched = true;
          break;
        }

        default:
          arg_set_error(&out, "Unknown argument type for %s.", def->name);
          rc_random = ARG_PARSE_INVALID_LIST;
          goto random_cleanup;
        }

        if (matched)
        {
          break;
        }
      }

      if (!matched)
      {
        if (arg_option_required(def->options))
        {
          const char *bad_token = NULL;

          for (t = 0; t < token_count; t++)
          {
            if (!used[t])
            {
              bad_token = tokens[t];
              break;
            }
          }

          if (bad_token)
          {
            arg_set_error(&out, "Invalid value for %s: %s.", def->name, bad_token);
            rc_random = ARG_PARSE_INVALID_VALUE;
          }
          else
          {
            arg_set_error(&out, "Missing required argument: %s.", def->name);
            rc_random = ARG_PARSE_MISSING_REQUIRED;
          }
          goto random_cleanup;
        }
      }
    }

    if (list && !allow_trailing)
    {
      size_t t;

      for (t = 0; t < token_count; t++)
      {
        if (!used[t])
        {
          arg_set_error(&out, "Unexpected argument: %s.", tokens[t]);
          rc_random = ARG_PARSE_EXTRA_ARGUMENT;
          goto random_cleanup;
        }
      }
    }

    rc_random = ARG_PARSE_OK;

  random_cleanup:
    if (used)
    {
      free(used);
    }
    arg_release_tokens(tokens, token_count);

    if (rc_random != ARG_PARSE_OK)
    {
      arg_release_values(&out);
      return rc_random;
    }

    return ARG_PARSE_OK;
  }

  for (i = 0; i < out.count; i++)
  {
    const arg_def *def = &out.items[i];
    const char *next = NULL;
    char token[MAX_INPUT_LENGTH];
    char normalized[MAX_INPUT_LENGTH];
    char regex_err[MAX_INPUT_LENGTH];
    int status;
    bool depends_on_prev = arg_option_depends_on_prev(def->options);

    if (depends_on_prev)
    {
      if (i == 0 || !out.values[i - 1].present)
      {
        continue;
      }
    }

    out.values[i].type = def->type;

    status = arg_next_token(cursor,
                            use_fill_words,
                            whitespace_delims,
                            delims,
                            token,
                            sizeof(token),
                            &next);
    if (status == ARG_TOKEN_TOO_LONG)
    {
      arg_set_error(&out, "Argument too long.");
      arg_release_values(&out);
      return ARG_PARSE_ARGUMENT_TOO_LONG;
    }
    if (status == ARG_TOKEN_NONE)
    {
      if (arg_option_required(def->options))
      {
        arg_set_error(&out, "Missing required argument: %s.", def->name);
        arg_release_values(&out);
        return ARG_PARSE_MISSING_REQUIRED;
      }
      continue;
    }

    switch (def->type)
    {
    case ARG_TYPE_STRING:
    {
      int match_status;

      regex_err[0] = '\0';
      match_status = arg_match_string(def,
                                      token,
                                      normalized,
                                      sizeof(normalized),
                                      regex_err,
                                      sizeof(regex_err));
      if (match_status == ARG_MATCH_ERROR)
      {
        arg_set_error(&out,
                      "Invalid string pattern for %s: %s.",
                      def->name,
                      regex_err[0] ? regex_err : "Invalid pattern");
        arg_release_values(&out);
        return ARG_PARSE_INVALID_LIST;
      }
      if (match_status == ARG_MATCH_AMBIGUOUS)
      {
        arg_set_error(&out,
                      "Ambiguous abbreviation for %s: %s.",
                      def->name,
                      token);
        arg_release_values(&out);
        return ARG_PARSE_INVALID_VALUE;
      }
      if (match_status == ARG_MATCH_NO)
      {
        if (arg_option_required(def->options))
        {
          arg_set_error(&out, "Invalid value for %s: %s.", def->name, token);
          arg_release_values(&out);
          return ARG_PARSE_INVALID_VALUE;
        }
        continue;
      }

      out.values[i].present = true;
      out.values[i].as.string = strdup(normalized);
      if (!out.values[i].as.string)
      {
        arg_set_error(&out, "Out of memory.");
        arg_release_values(&out);
        return ARG_PARSE_NO_MEMORY;
      }

      cursor = next;
      break;
    }

    case ARG_TYPE_INT:
    {
      int match_status = arg_match_int(def, token, &out.values[i].as.integer);

      if (match_status == ARG_MATCH_ERROR)
      {
        arg_set_error(&out, "Invalid integer range for %s.", def->name);
        arg_release_values(&out);
        return ARG_PARSE_INVALID_LIST;
      }
      if (match_status == ARG_MATCH_NO)
      {
        if (arg_option_required(def->options))
        {
          arg_set_error(&out, "Invalid value for %s: %s.", def->name, token);
          arg_release_values(&out);
          return ARG_PARSE_INVALID_VALUE;
        }
        continue;
      }

      out.values[i].present = true;
      cursor = next;
      break;
    }

    case ARG_TYPE_FLOAT:
    {
      int match_status = arg_match_float(def, token, &out.values[i].as.floating);

      if (match_status == ARG_MATCH_ERROR)
      {
        arg_set_error(&out, "Invalid float range for %s.", def->name);
        arg_release_values(&out);
        return ARG_PARSE_INVALID_LIST;
      }
      if (match_status == ARG_MATCH_NO)
      {
        if (arg_option_required(def->options))
        {
          arg_set_error(&out, "Invalid value for %s: %s.", def->name, token);
          arg_release_values(&out);
          return ARG_PARSE_INVALID_VALUE;
        }
        continue;
      }

      out.values[i].present = true;
      cursor = next;
      break;
    }

    case ARG_TYPE_BOOL:
    {
      int match_status = arg_match_bool(def, token, &out.values[i].as.boolean);

      if (match_status == ARG_MATCH_AMBIGUOUS)
      {
        arg_set_error(&out,
                      "Ambiguous boolean for %s: %s.",
                      def->name,
                      token);
        arg_release_values(&out);
        return ARG_PARSE_INVALID_VALUE;
      }
      if (match_status == ARG_MATCH_ERROR)
      {
        arg_set_error(&out, "Invalid boolean list for %s.", def->name);
        arg_release_values(&out);
        return ARG_PARSE_INVALID_LIST;
      }
      if (match_status == ARG_MATCH_NO)
      {
        if (arg_option_required(def->options))
        {
          arg_set_error(&out, "Invalid value for %s: %s.", def->name, token);
          arg_release_values(&out);
          return ARG_PARSE_INVALID_VALUE;
        }
        continue;
      }

      out.values[i].present = true;
      cursor = next;
      break;
    }

    default:
      arg_set_error(&out, "Unknown argument type for %s.", def->name);
      arg_release_values(&out);
      return ARG_PARSE_INVALID_LIST;
    }
  }

  if (list && !allow_trailing)
  {
    const char *next = NULL;
    char token[MAX_INPUT_LENGTH];
    int status = arg_next_token(cursor,
                                use_fill_words,
                                whitespace_delims,
                                delims,
                                token,
                                sizeof(token),
                                &next);

    if (status == ARG_TOKEN_TOO_LONG)
    {
      arg_set_error(&out, "Argument too long.");
      arg_release_values(&out);
      return ARG_PARSE_ARGUMENT_TOO_LONG;
    }

    if (status == ARG_TOKEN_OK)
    {
      arg_set_error(&out, "Unexpected argument: %s.", token);
      arg_release_values(&out);
      return ARG_PARSE_EXTRA_ARGUMENT;
    }
  }

  return ARG_PARSE_OK;
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

arg_parse_code parse_argument(const char *name,
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

arg_parse_code parse_argument(const char *name,
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

arg_parse_code parse_argument(const char *name,
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

arg_parse_code parse_argument(const char *name,
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

void free_parsed_arguments(arg_parser_output *out)
{
  if (!out)
  {
    return;
  }

  arg_release_values(out);

  if (out->error)
  {
    free(out->error);
    out->error = NULL;
  }

  out->items = NULL;
  out->count = 0;
}
