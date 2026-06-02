/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2022/6/6.
//

#include "sql/stmt/select_stmt.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "sql/optimizer/logical_plan_generator.h"
#include "sql/stmt/filter_stmt.h"
#include "storage/db/db.h"
#include "storage/table/table.h"
#include "sql/parser/expression_binder.h"
#include "sql/expr/expression.h"

using namespace std;
using namespace common;

SelectStmt::~SelectStmt()
{
  if (nullptr != filter_stmt_) {
    delete filter_stmt_;
    filter_stmt_ = nullptr;
  }
}

RC SelectStmt::create(Db *db, SelectSqlNode &select_sql, Stmt *&stmt)
{
  if (nullptr == db) {
    LOG_WARN("invalid argument. db is null");
    return RC::INVALID_ARGUMENT;
  }

  BinderContext binder_context;

  // collect tables in `from` statement
  vector<Table *>                tables;
  unordered_map<string, Table *> table_map;
  for (size_t i = 0; i < select_sql.relations.size(); i++) {
    const char *table_name = select_sql.relations[i].c_str();
    if (nullptr == table_name) {
      LOG_WARN("invalid argument. relation name is null. index=%d", i);
      return RC::INVALID_ARGUMENT;
    }

    Table *table = db->find_table(table_name);
    if (nullptr == table) {
      LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
      return RC::SCHEMA_TABLE_NOT_EXIST;
    }

    binder_context.add_table(table);
    tables.push_back(table);
    table_map.insert({table_name, table});
  }

  // collect query fields in `select` statement
  vector<unique_ptr<Expression>> bound_expressions;
  ExpressionBinder expression_binder(binder_context, db);
  
  for (unique_ptr<Expression> &expression : select_sql.expressions) {
    RC rc = expression_binder.bind_expression(expression, bound_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  vector<unique_ptr<Expression>> group_by_expressions;
  for (unique_ptr<Expression> &expression : select_sql.group_by) {
    RC rc = expression_binder.bind_expression(expression, group_by_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  vector<unique_ptr<Expression>> order_by_expressions;
  vector<bool>                   order_by_flags;
  for (OrderBySqlNode &order_node : select_sql.order_by) {
    vector<unique_ptr<Expression>> bound_order_expressions;
    RC                             rc = expression_binder.bind_expression(order_node.expression, bound_order_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind order by expression failed. rc=%s", strrc(rc));
      return rc;
    }
    if (bound_order_expressions.size() != 1) {
      LOG_WARN("invalid order by expression number: %d", bound_order_expressions.size());
      return RC::INVALID_ARGUMENT;
    }
    order_by_expressions.emplace_back(std::move(bound_order_expressions[0]));
    order_by_flags.push_back(order_node.asc);
  }

  unique_ptr<Expression> having_expression;
  if (!select_sql.having.empty()) {
    vector<unique_ptr<Expression>> bound_having;
    for (unique_ptr<Expression> &expr : select_sql.having) {
      vector<unique_ptr<Expression>> bound;
      RC                             rc = expression_binder.bind_expression(expr, bound);
      if (OB_FAIL(rc)) {
        LOG_INFO("bind having expression failed. rc=%s", strrc(rc));
        return rc;
      }
      if (bound.size() != 1) {
        LOG_WARN("invalid having expression number: %d", bound.size());
        return RC::INVALID_ARGUMENT;
      }
      bound_having.emplace_back(std::move(bound[0]));
    }
    if (bound_having.size() == 1) {
      having_expression = std::move(bound_having[0]);
    } else {
      having_expression = make_unique<ConjunctionExpr>(ConjunctionExpr::Type::AND, bound_having);
    }
  }

  Table *default_table = nullptr;
  if (tables.size() == 1) {
    default_table = tables[0];
  }

  if (!select_sql.join_conditions.empty() &&
      select_sql.join_conditions.size() + 1 != select_sql.relations.size()) {
    LOG_WARN("join on conditions count mismatch. relations=%zu, join_conditions=%zu",
        select_sql.relations.size(),
        select_sql.join_conditions.size());
    return RC::INVALID_ARGUMENT;
  }

  vector<vector<unique_ptr<Expression>>> join_predicates;
  for (const vector<ConditionSqlNode> &join_conds : select_sql.join_conditions) {
    FilterStmt                            *join_filter = nullptr;
    RC                                     jrc         = FilterStmt::create(db,
        default_table,
        &table_map,
        join_conds.data(),
        static_cast<int>(join_conds.size()),
        join_filter);
    if (jrc != RC::SUCCESS) {
      LOG_WARN("cannot construct join filter stmt");
      return jrc;
    }

    vector<unique_ptr<Expression>> cmp_exprs;
    jrc = LogicalPlanGenerator::create_comparison_expressions(join_filter, cmp_exprs);
    delete join_filter;
    if (jrc != RC::SUCCESS) {
      return jrc;
    }
    join_predicates.push_back(std::move(cmp_exprs));
  }

  // create filter in `where` statement
  FilterStmt            *filter_stmt      = nullptr;
  unique_ptr<Expression> where_expression;
  if (!select_sql.filter_exprs.empty()) {
    vector<unique_ptr<Expression>> bound_filters;
    for (unique_ptr<Expression> &expression : select_sql.filter_exprs) {
      vector<unique_ptr<Expression>> bound;
      RC                             rc = expression_binder.bind_expression(expression, bound);
      if (OB_FAIL(rc)) {
        LOG_INFO("bind where expression failed. rc=%s", strrc(rc));
        return rc;
      }
      if (bound.size() != 1) {
        LOG_WARN("invalid where expression number: %d", bound.size());
        return RC::INVALID_ARGUMENT;
      }
      bound_filters.emplace_back(std::move(bound[0]));
    }
    if (bound_filters.size() == 1) {
      where_expression = std::move(bound_filters[0]);
    } else {
      where_expression = make_unique<ConjunctionExpr>(ConjunctionExpr::Type::AND, bound_filters);
    }
  } else {
    RC rc = FilterStmt::create(db,
        default_table,
        &table_map,
        select_sql.conditions.data(),
        static_cast<int>(select_sql.conditions.size()),
        filter_stmt);
    if (rc != RC::SUCCESS) {
      LOG_WARN("cannot construct filter stmt");
      return rc;
    }
  }

  // everything alright
  SelectStmt *select_stmt = new SelectStmt();

  select_stmt->tables_.swap(tables);
  select_stmt->query_expressions_.swap(bound_expressions);
  select_stmt->filter_stmt_      = filter_stmt;
  select_stmt->where_expression_ = std::move(where_expression);
  select_stmt->group_by_.swap(group_by_expressions);
  select_stmt->join_predicates_.swap(join_predicates);
  select_stmt->order_by_.swap(order_by_expressions);
  select_stmt->order_by_flags_.swap(order_by_flags);
  select_stmt->having_expression_ = std::move(having_expression);
  stmt                      = select_stmt;
  return RC::SUCCESS;
}
