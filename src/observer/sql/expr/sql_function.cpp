/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/expr/sql_function.h"

#include <cmath>

#include "common/lang/string.h"
#include "sql/expr/tuple.h"

using namespace std;

namespace {

int char_actual_length(const Value &val)
{
  if (val.attr_type() != AttrType::CHARS) {
    return -1;
  }
  const char *s   = val.data();
  int           len = val.length();
  while (len > 0 && s[len - 1] == ' ') {
    len--;
  }
  return len;
}

const char *month_name(int month)
{
  static const char *names[] = {
      "",
      "January",
      "February",
      "March",
      "April",
      "May",
      "June",
      "July",
      "August",
      "September",
      "October",
      "November",
      "December",
  };
  if (month < 1 || month > 12) {
    return "";
  }
  return names[month];
}

string day_suffix(int day)
{
  if (day >= 11 && day <= 13) {
    return "th";
  }
  switch (day % 10) {
    case 1: return "st";
    case 2: return "nd";
    case 3: return "rd";
    default: return "th";
  }
}

RC format_date_value(int encoded, const string &fmt, string &out)
{
  int year  = encoded / 10000;
  int month = (encoded % 10000) / 100;
  int day   = encoded % 100;

  out.clear();
  for (size_t i = 0; i < fmt.size(); i++) {
    if (fmt[i] != '%' || i + 1 >= fmt.size()) {
      out.push_back(fmt[i]);
      continue;
    }
    char spec = fmt[++i];
    switch (spec) {
      case 'Y': {
        char buf[16];
        snprintf(buf, sizeof(buf), "%04d", year);
        out += buf;
      } break;
      case 'y': {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d", year % 100);
        out += buf;
      } break;
      case 'm': {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d", month);
        out += buf;
      } break;
      case 'd': {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d", day);
        out += buf;
      } break;
      case 'D': {
        out += to_string(day);
        out += day_suffix(day);
      } break;
      case 'M': {
        out += month_name(month);
      } break;
      default: out.push_back(spec); break;
    }
  }
  return RC::SUCCESS;
}

}  // namespace

RC sql_func_length(const Tuple & /*tuple*/, const Value &param, Value &result)
{
  int len = char_actual_length(param);
  if (len < 0) {
    return RC::INVALID_ARGUMENT;
  }
  result.set_int(len);
  return RC::SUCCESS;
}

RC sql_func_round(const Tuple & /*tuple*/, const Value &param, Value &result)
{
  if (param.attr_type() != AttrType::FLOATS) {
    return RC::INVALID_ARGUMENT;
  }
  float v = param.get_float();
  result.set_float(std::round(v));
  return RC::SUCCESS;
}

RC sql_func_date_format(const Tuple & /*tuple*/, const Value &date_param, const Value &format_param, Value &result)
{
  if (date_param.attr_type() != AttrType::DATES) {
    return RC::INVALID_ARGUMENT;
  }
  if (format_param.attr_type() != AttrType::CHARS) {
    return RC::INVALID_ARGUMENT;
  }

  string formatted;
  RC     rc = format_date_value(date_param.get_int(), format_param.get_string(), formatted);
  if (rc != RC::SUCCESS) {
    return rc;
  }
  result.set_string(formatted.c_str());
  return RC::SUCCESS;
}
