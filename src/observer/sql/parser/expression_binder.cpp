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
// Created by Wangyunlai on 2024/05/29.
//

#include "common/log/log.h"
#include "common/lang/string.h"
#include "common/lang/ranges.h"
#include "sql/parser/expression_binder.h"
#include "sql/expr/expression_iterator.h"
#include "sql/expr/subquery_expr.h"
#include "sql/stmt/select_stmt.h"
#include "storage/db/db.h"

using namespace common;

Table *BinderContext::find_table(const char *table_name) const
{
  auto pred = [table_name](Table *table) { return 0 == strcasecmp(table_name, table->name()); };
  auto iter = ranges::find_if(query_tables_, pred);
  if (iter != query_tables_.end()) {
    return *iter;
  }
  if (parent_ != nullptr) {
    return parent_->find_table(table_name);
  }
  return nullptr;
}

bool BinderContext::is_local_table(Table *table) const
{
  auto iter = ranges::find(query_tables_, table);
  return iter != query_tables_.end();
}

////////////////////////////////////////////////////////////////////////////////
static void wildcard_fields(Table *table, vector<unique_ptr<Expression>> &expressions)
{
  const TableMeta &table_meta = table->table_meta();
  const int        field_num  = table_meta.field_num();
  for (int i = table_meta.sys_field_num(); i < field_num; i++) {
    Field      field(table, table_meta.field(i));
    FieldExpr *field_expr = new FieldExpr(field);
    field_expr->set_name(field.field_name());
    expressions.emplace_back(field_expr);
  }
}

RC ExpressionBinder::bind_expression(unique_ptr<Expression> &expr, vector<unique_ptr<Expression>> &bound_expressions)
{
  if (nullptr == expr) {
    return RC::SUCCESS;
  }

  switch (expr->type()) {
    case ExprType::STAR: {
      return bind_star_expression(expr, bound_expressions);
    } break;

    case ExprType::UNBOUND_FIELD: {
      return bind_unbound_field_expression(expr, bound_expressions);
    } break;

    case ExprType::UNBOUND_AGGREGATION: {
      return bind_aggregate_expression(expr, bound_expressions);
    } break;

    case ExprType::UNBOUND_FUNCTION: {
      return bind_function_expression(expr, bound_expressions);
    } break;

    case ExprType::UNBOUND_SUBQUERY: {
      return bind_subquery_expression(expr, bound_expressions);
    } break;

    case ExprType::IN_SUBQUERY: {
      return bind_in_subquery_expression(expr, bound_expressions);
    } break;

    case ExprType::SUBQUERY: {
      bound_expressions.emplace_back(std::move(expr));
      return RC::SUCCESS;
    } break;

    case ExprType::FUNCTION: {
      bound_expressions.emplace_back(std::move(expr));
      return RC::SUCCESS;
    } break;

    case ExprType::FIELD: {
      return bind_field_expression(expr, bound_expressions);
    } break;

    case ExprType::VALUE: {
      return bind_value_expression(expr, bound_expressions);
    } break;

    case ExprType::CAST: {
      return bind_cast_expression(expr, bound_expressions);
    } break;

    case ExprType::COMPARISON: {
      return bind_comparison_expression(expr, bound_expressions);
    } break;

    case ExprType::CONJUNCTION: {
      return bind_conjunction_expression(expr, bound_expressions);
    } break;

    case ExprType::ARITHMETIC: {
      return bind_arithmetic_expression(expr, bound_expressions);
    } break;

    case ExprType::AGGREGATION: {
      ASSERT(false, "shouldn't be here");
    } break;

    default: {
      LOG_WARN("unknown expression type: %d", static_cast<int>(expr->type()));
      return RC::INTERNAL;
    }
  }
  return RC::INTERNAL;
}

RC ExpressionBinder::bind_star_expression(
    unique_ptr<Expression> &expr, vector<unique_ptr<Expression>> &bound_expressions)
{
  if (nullptr == expr) {
    return RC::SUCCESS;
  }

  auto star_expr = static_cast<StarExpr *>(expr.get());

  vector<Table *> tables_to_wildcard;

  const char *table_name = star_expr->table_name();
  if (!is_blank(table_name) && 0 != strcmp(table_name, "*")) {
    Table *table = context_.find_table(table_name);
    if (nullptr == table) {
      LOG_INFO("no such table in from list: %s", table_name);
      return RC::SCHEMA_TABLE_NOT_EXIST;
    }

    tables_to_wildcard.push_back(table);
  } else {
    const vector<Table *> &all_tables = context_.query_tables();
    tables_to_wildcard.insert(tables_to_wildcard.end(), all_tables.begin(), all_tables.end());
  }

  for (Table *table : tables_to_wildcard) {
    wildcard_fields(table, bound_expressions);
  }

  return RC::SUCCESS;
}

RC ExpressionBinder::bind_unbound_field_expression(
    unique_ptr<Expression> &expr, vector<unique_ptr<Expression>> &bound_expressions)
{
  if (nullptr == expr) {
    return RC::SUCCESS;
  }

  auto unbound_field_expr = static_cast<UnboundFieldExpr *>(expr.get());

  const char *table_name = unbound_field_expr->table_name();
  const char *field_name = unbound_field_expr->field_name();

  Table *table = nullptr;
  if (is_blank(table_name)) {
    if (context_.query_tables().size() != 1) {
      LOG_INFO("cannot determine table for field: %s", field_name);
      return RC::SCHEMA_TABLE_NOT_EXIST;
    }

    table = context_.query_tables()[0];
  } else {
    table = context_.find_table(table_name);
    if (nullptr == table) {
      LOG_INFO("no such table in from list: %s", table_name);
      return RC::SCHEMA_TABLE_NOT_EXIST;
    }
  }

  if (0 == strcmp(field_name, "*")) {
    wildcard_fields(table, bound_expressions);
  } else {
    const FieldMeta *field_meta = table->table_meta().field(field_name);
    if (nullptr == field_meta) {
      LOG_INFO("no such field in table: %s.%s", table_name, field_name);
      return RC::SCHEMA_FIELD_MISSING;
    }

    Field      field(table, field_meta);
    FieldExpr *field_expr = new FieldExpr(field);
    field_expr->set_name(field_name);
    if (!context_.is_local_table(table)) {
      field_expr->set_outer_ref(true);
    }
    bound_expressions.emplace_back(field_expr);
  }

  return RC::SUCCESS;
}

RC ExpressionBinder::bind_field_expression(
    unique_ptr<Expression> &field_expr, vector<unique_ptr<Expression>> &bound_expressions)
{
  bound_expressions.emplace_back(std::move(field_expr));
  return RC::SUCCESS;
}

RC ExpressionBinder::bind_value_expression(
    unique_ptr<Expression> &value_expr, vector<unique_ptr<Expression>> &bound_expressions)
{
  bound_expressions.emplace_back(std::move(value_expr));
  return RC::SUCCESS;
}

RC ExpressionBinder::bind_cast_expression(
    unique_ptr<Expression> &expr, vector<unique_ptr<Expression>> &bound_expressions)
{
  if (nullptr == expr) {
    return RC::SUCCESS;
  }

  auto cast_expr = static_cast<CastExpr *>(expr.get());

  vector<unique_ptr<Expression>> child_bound_expressions;
  unique_ptr<Expression>        &child_expr = cast_expr->child();

  RC rc = bind_expression(child_expr, child_bound_expressions);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  if (child_bound_expressions.size() != 1) {
    LOG_WARN("invalid children number of cast expression: %d", child_bound_expressions.size());
    return RC::INVALID_ARGUMENT;
  }

  unique_ptr<Expression> &child = child_bound_expressions[0];
  if (child.get() == child_expr.get()) {
    return RC::SUCCESS;
  }

  child_expr.reset(child.release());
  bound_expressions.emplace_back(std::move(expr));
  return RC::SUCCESS;
}

RC ExpressionBinder::bind_comparison_expression(
    unique_ptr<Expression> &expr, vector<unique_ptr<Expression>> &bound_expressions)
{
  if (nullptr == expr) {
    return RC::SUCCESS;
  }

  auto comparison_expr = static_cast<ComparisonExpr *>(expr.get());

  vector<unique_ptr<Expression>> child_bound_expressions;
  unique_ptr<Expression>        &left_expr  = comparison_expr->left();
  unique_ptr<Expression>        &right_expr = comparison_expr->right();

  RC rc = bind_expression(left_expr, child_bound_expressions);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  if (child_bound_expressions.size() != 1) {
    LOG_WARN("invalid left children number of comparison expression: %d", child_bound_expressions.size());
    return RC::INVALID_ARGUMENT;
  }

  unique_ptr<Expression> &left = child_bound_expressions[0];
  if (left.get() != left_expr.get()) {
    left_expr.reset(left.release());
  }

  child_bound_expressions.clear();
  rc = bind_expression(right_expr, child_bound_expressions);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  if (child_bound_expressions.size() != 1) {
    LOG_WARN("invalid right children number of comparison expression: %d", child_bound_expressions.size());
    return RC::INVALID_ARGUMENT;
  }

  unique_ptr<Expression> &right = child_bound_expressions[0];
  if (right.get() != right_expr.get()) {
    right_expr.reset(right.release());
  }

  bound_expressions.emplace_back(std::move(expr));
  return RC::SUCCESS;
}

RC ExpressionBinder::bind_conjunction_expression(
    unique_ptr<Expression> &expr, vector<unique_ptr<Expression>> &bound_expressions)
{
  if (nullptr == expr) {
    return RC::SUCCESS;
  }

  auto conjunction_expr = static_cast<ConjunctionExpr *>(expr.get());

  vector<unique_ptr<Expression>>  child_bound_expressions;
  vector<unique_ptr<Expression>> &children = conjunction_expr->children();

  for (unique_ptr<Expression> &child_expr : children) {
    child_bound_expressions.clear();

    RC rc = bind_expression(child_expr, child_bound_expressions);
    if (rc != RC::SUCCESS) {
      return rc;
    }

    if (child_bound_expressions.size() != 1) {
      LOG_WARN("invalid children number of conjunction expression: %d", child_bound_expressions.size());
      return RC::INVALID_ARGUMENT;
    }

    unique_ptr<Expression> &child = child_bound_expressions[0];
    if (child.get() != child_expr.get()) {
      child_expr.reset(child.release());
    }
  }

  bound_expressions.emplace_back(std::move(expr));

  return RC::SUCCESS;
}

RC ExpressionBinder::bind_arithmetic_expression(
    unique_ptr<Expression> &expr, vector<unique_ptr<Expression>> &bound_expressions)
{
  if (nullptr == expr) {
    return RC::SUCCESS;
  }

  auto arithmetic_expr = static_cast<ArithmeticExpr *>(expr.get());

  vector<unique_ptr<Expression>> child_bound_expressions;
  unique_ptr<Expression>        &left_expr  = arithmetic_expr->left();
  unique_ptr<Expression>        &right_expr = arithmetic_expr->right();

  RC rc = bind_expression(left_expr, child_bound_expressions);
  if (OB_FAIL(rc)) {
    return rc;
  }

  if (child_bound_expressions.size() != 1) {
    LOG_WARN("invalid left children number of comparison expression: %d", child_bound_expressions.size());
    return RC::INVALID_ARGUMENT;
  }

  unique_ptr<Expression> &left = child_bound_expressions[0];
  if (left.get() != left_expr.get()) {
    left_expr.reset(left.release());
  }

  child_bound_expressions.clear();
  rc = bind_expression(right_expr, child_bound_expressions);
  if (OB_FAIL(rc)) {
    return rc;
  }

  if (child_bound_expressions.size() != 1) {
    LOG_WARN("invalid right children number of comparison expression: %d", child_bound_expressions.size());
    return RC::INVALID_ARGUMENT;
  }

  unique_ptr<Expression> &right = child_bound_expressions[0];
  if (right.get() != right_expr.get()) {
    right_expr.reset(right.release());
  }

  bound_expressions.emplace_back(std::move(expr));
  return RC::SUCCESS;
}

RC check_aggregate_expression(AggregateExpr &expression)
{
  // 必须有一个子表达式
  Expression *child_expression = expression.child().get();
  if (nullptr == child_expression) {
    LOG_WARN("child expression of aggregate expression is null");
    return RC::INVALID_ARGUMENT;
  }

  // 校验数据类型与聚合类型是否匹配
  AggregateExpr::Type aggregate_type   = expression.aggregate_type();
  AttrType            child_value_type = child_expression->value_type();
  switch (aggregate_type) {
    case AggregateExpr::Type::SUM:
    case AggregateExpr::Type::AVG: {
      // 仅支持数值类型
      if (!is_numerical_type(child_value_type)) {
        LOG_WARN("invalid child value type for aggregate expression: %d", static_cast<int>(child_value_type));
        return RC::INVALID_ARGUMENT;
      }
    } break;

    case AggregateExpr::Type::COUNT:
    case AggregateExpr::Type::MAX:
    case AggregateExpr::Type::MIN: {
      // 任何类型都支持
    } break;
  }

  // 子表达式中不能再包含聚合表达式
  function<RC(unique_ptr<Expression>&)> check_aggregate_expr = [&](unique_ptr<Expression> &expr) -> RC {
    RC rc = RC::SUCCESS;
    if (expr->type() == ExprType::AGGREGATION) {
      LOG_WARN("aggregate expression cannot be nested");
      return RC::INVALID_ARGUMENT;
    }
    rc = ExpressionIterator::iterate_child_expr(*expr, check_aggregate_expr);
    return rc;
  };

  RC rc = ExpressionIterator::iterate_child_expr(expression, check_aggregate_expr);

  return rc;
}

RC ExpressionBinder::bind_aggregate_expression(
    unique_ptr<Expression> &expr, vector<unique_ptr<Expression>> &bound_expressions)
{
  if (nullptr == expr) {
    return RC::SUCCESS;
  }

  auto unbound_aggregate_expr = static_cast<UnboundAggregateExpr *>(expr.get());
  const char *aggregate_name = unbound_aggregate_expr->aggregate_name();
  AggregateExpr::Type aggregate_type;
  RC rc = AggregateExpr::type_from_string(aggregate_name, aggregate_type);
  if (OB_FAIL(rc)) {
    LOG_WARN("invalid aggregate name: %s", aggregate_name);
    return rc;
  }

  unique_ptr<Expression>        &child_expr = unbound_aggregate_expr->child();
  vector<unique_ptr<Expression>> child_bound_expressions;

  if (child_expr->type() == ExprType::STAR) {
    if (aggregate_type != AggregateExpr::Type::COUNT) {
      LOG_WARN("only COUNT supports '*'");
      return RC::INVALID_ARGUMENT;
    }
    ValueExpr *value_expr = new ValueExpr(Value(1));
    child_expr.reset(value_expr);
  } else {
    rc = bind_expression(child_expr, child_bound_expressions);
    if (OB_FAIL(rc)) {
      return rc;
    }

    if (child_bound_expressions.size() != 1) {
      LOG_WARN("invalid children number of aggregate expression: %d", child_bound_expressions.size());
      return RC::INVALID_ARGUMENT;
    }

    if (child_bound_expressions[0].get() != child_expr.get()) {
      child_expr.reset(child_bound_expressions[0].release());
    }
  }

  auto aggregate_expr = make_unique<AggregateExpr>(aggregate_type, std::move(child_expr));
  aggregate_expr->set_name(unbound_aggregate_expr->name());
  rc = check_aggregate_expression(*aggregate_expr);
  if (OB_FAIL(rc)) {
    return rc;
  }

  bound_expressions.emplace_back(std::move(aggregate_expr));
  return RC::SUCCESS;
}

RC ExpressionBinder::bind_function_expression(
    unique_ptr<Expression> &expr, vector<unique_ptr<Expression>> &bound_expressions)
{
  if (nullptr == expr) {
    return RC::SUCCESS;
  }

  auto unbound_function_expr = static_cast<UnboundFunctionExpr *>(expr.get());
  auto func_type             = unbound_function_expr->func_type();

  for (unique_ptr<Expression> &param_expr : unbound_function_expr->params()) {
    vector<unique_ptr<Expression>> child_bound_expressions;
    RC                             rc = bind_expression(param_expr, child_bound_expressions);
    if (OB_FAIL(rc)) {
      return rc;
    }
    if (child_bound_expressions.size() != 1) {
      LOG_WARN("invalid children number of function expression: %d", child_bound_expressions.size());
      return RC::INVALID_ARGUMENT;
    }
    if (child_bound_expressions[0].get() != param_expr.get()) {
      param_expr.reset(child_bound_expressions[0].release());
    }
  }

  const vector<unique_ptr<Expression>> &params = unbound_function_expr->params();
  if (func_type == UnboundFunctionExpr::FuncType::LENGTH) {
    if (params.size() != 1 || params[0]->value_type() != AttrType::CHARS) {
      return RC::INVALID_ARGUMENT;
    }
  } else if (func_type == UnboundFunctionExpr::FuncType::ROUND) {
    if (params.size() != 1 || params[0]->value_type() != AttrType::FLOATS) {
      return RC::INVALID_ARGUMENT;
    }
  } else if (func_type == UnboundFunctionExpr::FuncType::DATE_FORMAT) {
    if (params.size() != 2 || params[0]->value_type() != AttrType::DATES ||
        params[1]->value_type() != AttrType::CHARS) {
      return RC::INVALID_ARGUMENT;
    }
  } else {
    return RC::INVALID_ARGUMENT;
  }

  vector<unique_ptr<Expression>> moved_params;
  for (unique_ptr<Expression> &param_expr : unbound_function_expr->params()) {
    moved_params.emplace_back(param_expr.release());
  }
  unbound_function_expr->params().clear();

  auto function_expr = make_unique<FunctionExpr>(func_type, std::move(moved_params));
  function_expr->set_name(unbound_function_expr->name());
  bound_expressions.emplace_back(std::move(function_expr));
  return RC::SUCCESS;
}

RC ExpressionBinder::bind_subquery_expression(
    unique_ptr<Expression> &expr, vector<unique_ptr<Expression>> &bound_expressions)
{
  if (db_ == nullptr) {
    LOG_WARN("db is null when binding subquery");
    return RC::INVALID_ARGUMENT;
  }

  auto unbound_expr = static_cast<UnboundSubQueryExpr *>(expr.get());
  Stmt *stmt        = nullptr;
  RC    rc          = SelectStmt::create(db_, unbound_expr->select_sql(), stmt, &context_);
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to create subquery select stmt. rc=%s", strrc(rc));
    return rc;
  }

  unique_ptr<SelectStmt> select_stmt(static_cast<SelectStmt *>(stmt));
  if (select_stmt->query_expressions().size() != 1) {
    LOG_WARN("subquery must return exactly one column");
    return RC::INVALID_ARGUMENT;
  }

  unique_ptr<Expression> &value_expr = select_stmt->query_expressions().front();
  if (value_expr->type() == ExprType::STAR) {
    LOG_WARN("subquery does not support select *");
    return RC::INVALID_ARGUMENT;
  }

  unique_ptr<Expression> value_expr_copy = value_expr->copy();
  if (value_expr_copy == nullptr) {
    LOG_WARN("failed to copy subquery value expression");
    return RC::INTERNAL;
  }

  bound_expressions.emplace_back(
      make_unique<SubQueryExpr>(std::move(select_stmt), std::move(value_expr_copy)));
  auto &subquery_expr = static_cast<SubQueryExpr &>(*bound_expressions.back());
  subquery_expr.set_correlated(select_stmt_has_correlation(subquery_expr.select_stmt()));
  return RC::SUCCESS;
}

RC ExpressionBinder::bind_in_subquery_expression(
    unique_ptr<Expression> &expr, vector<unique_ptr<Expression>> &bound_expressions)
{
  auto in_expr = static_cast<InSubQueryExpr *>(expr.get());

  vector<unique_ptr<Expression>> left_bound;
  RC                             rc = bind_expression(in_expr->left(), left_bound);
  if (OB_FAIL(rc)) {
    return rc;
  }
  if (left_bound.size() != 1) {
    LOG_WARN("invalid left expression for IN subquery");
    return RC::INVALID_ARGUMENT;
  }

  vector<unique_ptr<Expression>> sub_bound;
  rc = bind_expression(in_expr->subquery(), sub_bound);
  if (OB_FAIL(rc)) {
    return rc;
  }
  if (sub_bound.size() != 1 || sub_bound[0]->type() != ExprType::SUBQUERY) {
    LOG_WARN("invalid subquery for IN expression");
    return RC::INVALID_ARGUMENT;
  }

  in_expr->left()    = std::move(left_bound[0]);
  in_expr->subquery() = std::move(sub_bound[0]);
  bound_expressions.emplace_back(std::move(expr));
  return RC::SUCCESS;
}
