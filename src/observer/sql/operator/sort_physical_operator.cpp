/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/operator/sort_physical_operator.h"

#include <algorithm>

#include "common/log/log.h"
#include "sql/expr/expression.h"

using namespace std;

namespace {

RC copy_tuple(const Tuple &src, ValueListTuple &dst)
{
  const int cell_num = src.cell_num();
  vector<Value>         cells;
  vector<TupleCellSpec> specs;
  cells.reserve(cell_num);
  specs.reserve(cell_num);

  for (int i = 0; i < cell_num; i++) {
    Value         cell;
    TupleCellSpec spec;
    RC            rc = src.cell_at(i, cell);
    if (rc != RC::SUCCESS) {
      return rc;
    }
    rc = src.spec_at(i, spec);
    if (rc != RC::SUCCESS) {
      return rc;
    }
    cells.emplace_back(cell);
    specs.emplace_back(spec);
  }

  dst.set_cells(cells);
  dst.set_names(specs);
  return RC::SUCCESS;
}

RC eval_sort_keys(const Tuple &tuple, const vector<unique_ptr<Expression>> &sort_keys, vector<Value> &keys)
{
  keys.clear();
  keys.reserve(sort_keys.size());
  for (const unique_ptr<Expression> &expr : sort_keys) {
    Value value;
    RC    rc = expr->get_value(tuple, value);
    if (rc != RC::SUCCESS) {
      return rc;
    }
    keys.emplace_back(value);
  }
  return RC::SUCCESS;
}

}  // namespace

SortPhysicalOperator::SortPhysicalOperator(
    vector<unique_ptr<Expression>> &&sort_keys, vector<bool> &&order_by_flags)
    : sort_keys_(std::move(sort_keys)), order_by_flags_(std::move(order_by_flags))
{}

RC SortPhysicalOperator::open(Trx *trx)
{
  if (children_.empty()) {
    return RC::INTERNAL;
  }

  RC rc = children_[0]->open(trx);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  return fetch_and_sort();
}

RC SortPhysicalOperator::fetch_and_sort()
{
  struct SortEntry
  {
    ValueListTuple row;
    vector<Value>  keys;
  };

  vector<SortEntry> entries;
  PhysicalOperator *child = children_[0].get();
  RC                rc      = RC::SUCCESS;

  while (RC::SUCCESS == (rc = child->next())) {
    Tuple *tuple = child->current_tuple();
    if (tuple == nullptr) {
      return RC::INTERNAL;
    }

    SortEntry entry;
    rc = copy_tuple(*tuple, entry.row);
    if (rc != RC::SUCCESS) {
      return rc;
    }
    rc = eval_sort_keys(entry.row, sort_keys_, entry.keys);
    if (rc != RC::SUCCESS) {
      return rc;
    }
    entries.emplace_back(std::move(entry));
  }

  if (rc != RC::RECORD_EOF) {
    return rc;
  }

  auto comparator = [this](const SortEntry &left, const SortEntry &right) -> bool {
    for (size_t i = 0; i < sort_keys_.size(); i++) {
      int cmp = left.keys[i].compare(right.keys[i]);
      if (cmp == 0) {
        continue;
      }
      bool asc = i < order_by_flags_.size() ? order_by_flags_[i] : true;
      return asc ? cmp < 0 : cmp > 0;
    }
    return false;
  };

  std::sort(entries.begin(), entries.end(), comparator);

  sorted_tuples_.clear();
  sorted_tuples_.reserve(entries.size());
  for (SortEntry &entry : entries) {
    sorted_tuples_.emplace_back(std::move(entry.row));
  }
  current_index_ = 0;
  return RC::SUCCESS;
}

RC SortPhysicalOperator::next()
{
  if (current_index_ >= sorted_tuples_.size()) {
    return RC::RECORD_EOF;
  }
  current_tuple_ = sorted_tuples_[current_index_++];
  return RC::SUCCESS;
}

RC SortPhysicalOperator::close()
{
  sorted_tuples_.clear();
  current_index_ = 0;
  if (!children_.empty()) {
    return children_[0]->close();
  }
  return RC::SUCCESS;
}

Tuple *SortPhysicalOperator::current_tuple() { return &current_tuple_; }
