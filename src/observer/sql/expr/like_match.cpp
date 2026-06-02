/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/expr/like_match.h"

bool like_match(const std::string &text, const std::string &pattern)
{
  const char *s = text.c_str();
  const char *p = pattern.c_str();

  while (*p) {
    if (*p == '%') {
      p++;
      if (*p == '\0') {
        while (*s && *s != '\'') {
          s++;
        }
        return *s == '\0';
      }
      while (*s) {
        if (*s == '\'') {
          return false;
        }
        if (like_match(std::string(s), std::string(p))) {
          return true;
        }
        s++;
      }
      return like_match(std::string(s), std::string(p));
    }

    if (*p == '_') {
      if (*s == '\0' || *s == '\'') {
        return false;
      }
      s++;
      p++;
      continue;
    }

    if (*s != *p) {
      return false;
    }
    if (*s == '\0') {
      return false;
    }
    s++;
    p++;
  }

  return *s == '\0';
}
