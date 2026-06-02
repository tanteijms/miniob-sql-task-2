/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "common/type/date_type.h"

#include "common/lang/comparator.h"
#include "common/lang/sstream.h"
#include "common/log/log.h"
#include "common/value.h"
#include "storage/common/column.h"

namespace {

bool is_leap_year(int year)
{
  return (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0);
}

int days_in_month(int year, int month)
{
  static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) {
    return 0;
  }
  if (month == 2 && is_leap_year(year)) {
    return 29;
  }
  return days[month];
}

}  // namespace

bool DateType::parse_date(const char *str, int &encoded)
{
  if (str == nullptr) {
    return false;
  }

  int year = 0;
  int month = 0;
  int day = 0;
  if (sscanf(str, "%d-%d-%d", &year, &month, &day) != 3) {
    return false;
  }
  if (month < 1 || month > 12 || day < 1) {
    return false;
  }
  if (day > days_in_month(year, month)) {
    return false;
  }

  encoded = year * 10000 + month * 100 + day;
  return true;
}

string DateType::format_date(int encoded)
{
  int year  = encoded / 10000;
  int month = (encoded % 10000) / 100;
  int day   = encoded % 100;
  stringstream ss;
  ss << year << '-' << month << '-' << day;
  return ss.str();
}

int DateType::compare(const Value &left, const Value &right) const
{
  ASSERT(left.attr_type() == AttrType::DATES, "left type is not date");

  int left_days = left.get_int();

  if (right.attr_type() == AttrType::DATES) {
    int right_days = right.get_int();
    return common::compare_int((void *)&left_days, (void *)&right_days);
  }

  if (right.attr_type() == AttrType::CHARS) {
    int right_days = 0;
    if (!parse_date(right.get_string().c_str(), right_days)) {
      return INT32_MAX;
    }
    return common::compare_int((void *)&left_days, (void *)&right_days);
  }

  return INT32_MAX;
}

int DateType::compare(const Column &left, const Column &right, int left_idx, int right_idx) const
{
  ASSERT(left.attr_type() == AttrType::DATES, "left type is not date");
  ASSERT(right.attr_type() == AttrType::DATES, "right type is not date");
  return common::compare_int((void *)&((int *)left.data())[left_idx], (void *)&((int *)right.data())[right_idx]);
}

RC DateType::cast_to(const Value &val, AttrType type, Value &result) const
{
  switch (type) {
    case AttrType::CHARS: {
      string s = format_date(val.get_int());
      result.set_string(s.c_str());
      return RC::SUCCESS;
    }
    default: {
      LOG_WARN("unsupported cast from date to type %d", type);
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }
  }
}

RC DateType::to_string(const Value &val, string &result) const
{
  result = format_date(val.get_int());
  return RC::SUCCESS;
}
