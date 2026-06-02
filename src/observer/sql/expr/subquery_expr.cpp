/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/expr/subquery_expr.h"

#include "common/log/log.h"
#include "session/session.h"
#include "sql/expr/expression_iterator.h"
#include "sql/optimizer/logical_plan_generator.h"
#include "sql/optimizer/physical_plan_generator.h"
#include "sql/operator/physical_operator.h"
#include "sql/stmt/stmt.h"

using namespace std;

UnboundSubQueryExpr::UnboundSubQueryExpr(SelectSqlNode &&select_sql) : select_sql_(std::move(select_sql)) {}

unique_ptr<Expression> UnboundSubQueryExpr::copy() const
{
  return make_unique<UnboundSubQueryExpr>(SelectSqlNode());
}

SubQueryExpr::SubQueryExpr(unique_ptr<SelectStmt> select_stmt, unique_ptr<Expression> value_expr)
    : select_stmt_(std::move(select_stmt)), value_expr_(std::move(value_expr))
{}

SubQueryExpr::~SubQueryExpr() { close(); }

AttrType SubQueryExpr::value_type() const { return value_expr_ ? value_expr_->value_type() : AttrType::UNDEFINED; }

unique_ptr<Expression> SubQueryExpr::copy() const { return nullptr; }

void SubQueryExpr::set_physical_operator(unique_ptr<PhysicalOperator> oper)
{
  close();
  physical_operator_  = std::move(oper);
  scalar_materialized_  = false;
  scalar_empty_         = false;
}

RC SubQueryExpr::open(Trx *trx)
{
  if (physical_operator_ == nullptr) {
    return RC::INTERNAL;
  }
  trx_ = trx;
  if (opened_) {
    return RC::SUCCESS;
  }
  RC rc = physical_operator_->open(trx);
  if (rc == RC::SUCCESS) {
    opened_ = true;
  }
  return rc;
}

RC SubQueryExpr::close()
{
  if (!opened_ || physical_operator_ == nullptr) {
    return RC::SUCCESS;
  }
  RC rc = physical_operator_->close();
  opened_ = false;
  return rc;
}

RC SubQueryExpr::materialize_all_values(vector<Value> &values) const
{
  if (physical_operator_ == nullptr) {
    return RC::INTERNAL;
  }

  SubQueryExpr *self = const_cast<SubQueryExpr *>(this);
  RC            rc   = self->close();
  if (OB_FAIL(rc)) {
    return rc;
  }

  values.clear();
  rc = self->open(trx_);
  if (OB_FAIL(rc)) {
    return rc;
  }

  PhysicalOperator *oper = physical_operator_.get();
  while (true) {
    rc = oper->next();
    if (rc == RC::RECORD_EOF) {
      rc = RC::SUCCESS;
      break;
    }
    if (OB_FAIL(rc)) {
      break;
    }

    Tuple *tuple = oper->current_tuple();
    if (tuple == nullptr) {
      rc = RC::INTERNAL;
      break;
    }

    Value cell;
    if (tuple->cell_num() > 0) {
      rc = tuple->cell_at(0, cell);
    } else {
      rc = value_expr_->get_value(*tuple, cell);
    }
    if (OB_FAIL(rc)) {
      break;
    }
    values.push_back(cell);
  }

  RC close_rc = self->close();
  if (OB_SUCC(rc) && OB_FAIL(close_rc)) {
    rc = close_rc;
  }
  return rc;
}

RC SubQueryExpr::materialize_scalar() const
{
  if (scalar_materialized_) {
    return RC::SUCCESS;
  }

  vector<Value> values;
  RC            rc = materialize_all_values(values);
  if (OB_FAIL(rc)) {
    return rc;
  }

  if (values.size() > 1) {
    return RC::INVALID_ARGUMENT;
  }

  scalar_materialized_ = true;
  if (values.empty()) {
    scalar_empty_ = true;
    return RC::SUCCESS;
  }

  scalar_cache_ = values[0];
  scalar_empty_ = false;
  return RC::SUCCESS;
}

RC SubQueryExpr::get_value(const Tuple &tuple, Value &value) const
{
  (void)tuple;
  if (!scalar_materialized_) {
    vector<Value> values;
    RC            rc = materialize_all_values(values);
    if (OB_FAIL(rc)) {
      return rc;
    }
    if (values.size() > 1) {
      return RC::INVALID_ARGUMENT;
    }
    if (values.empty()) {
      return RC::INVALID_ARGUMENT;
    }
    scalar_cache_        = values[0];
    scalar_materialized_ = true;
    scalar_empty_        = false;
  } else if (scalar_empty_) {
    return RC::INVALID_ARGUMENT;
  }
  value = scalar_cache_;
  return RC::SUCCESS;
}

InSubQueryExpr::InSubQueryExpr(unique_ptr<Expression> left, unique_ptr<Expression> subquery, bool not_in)
    : left_(std::move(left)), subquery_(std::move(subquery)), not_in_(not_in)
{}

InSubQueryExpr::~InSubQueryExpr() { close(); }

unique_ptr<Expression> InSubQueryExpr::copy() const { return nullptr; }

RC InSubQueryExpr::open(Trx *trx)
{
  if (subquery_ == nullptr || subquery_->type() != ExprType::SUBQUERY) {
    return RC::INTERNAL;
  }
  static_cast<SubQueryExpr *>(subquery_.get())->set_trx(trx);
  return RC::SUCCESS;
}

RC InSubQueryExpr::close()
{
  materialized_ = false;
  cached_values_.clear();
  if (subquery_ == nullptr || subquery_->type() != ExprType::SUBQUERY) {
    return RC::SUCCESS;
  }
  return static_cast<SubQueryExpr *>(subquery_.get())->close();
}

RC InSubQueryExpr::get_value(const Tuple &tuple, Value &value) const
{
  Value left_value;
  RC    rc = left_->get_value(tuple, left_value);
  if (OB_FAIL(rc)) {
    return rc;
  }

  if (subquery_->type() != ExprType::SUBQUERY) {
    return RC::INTERNAL;
  }
  SubQueryExpr *subquery = static_cast<SubQueryExpr *>(subquery_.get());

  if (!materialized_) {
    rc = subquery->materialize_all_values(cached_values_);
    if (OB_FAIL(rc)) {
      return rc;
    }
    materialized_ = true;
  }

  bool found = false;
  for (const Value &candidate : cached_values_) {
    if (left_value.compare(candidate) == 0) {
      found = true;
      break;
    }
  }

  bool result = not_in_ ? !found : found;
  value.set_boolean(result);
  return RC::SUCCESS;
}

static RC prepare_subquery_expr(Expression &expr, Session *session)
{
  RC rc = RC::SUCCESS;
  if (expr.type() == ExprType::SUBQUERY) {
    auto &subquery_expr = static_cast<SubQueryExpr &>(expr);
    if (subquery_expr.select_stmt() == nullptr) {
      return RC::INTERNAL;
    }
    if (subquery_expr.physical_operator() != nullptr) {
      return RC::SUCCESS;
    }

    Stmt *stmt = subquery_expr.select_stmt();

    unique_ptr<LogicalOperator> logical_oper;
    rc = LogicalPlanGenerator().create(stmt, logical_oper);
    if (OB_FAIL(rc)) {
      return rc;
    }

    unique_ptr<PhysicalOperator> physical_oper;
    rc = PhysicalPlanGenerator().create(*logical_oper, physical_oper, session);
    if (OB_FAIL(rc)) {
      return rc;
    }

    subquery_expr.set_physical_operator(std::move(physical_oper));
    return RC::SUCCESS;
  }

  if (expr.type() == ExprType::IN_SUBQUERY) {
    auto &in_expr = static_cast<InSubQueryExpr &>(expr);
    if (in_expr.subquery() && in_expr.subquery()->type() == ExprType::SUBQUERY) {
      rc = prepare_subquery_expr(*in_expr.subquery(), session);
    }
    return rc;
  }

  if (expr.type() == ExprType::COMPARISON) {
    auto &cmp_expr = static_cast<ComparisonExpr &>(expr);
    rc           = prepare_subquery_expr(*cmp_expr.left(), session);
    if (OB_FAIL(rc)) {
      return rc;
    }
    return prepare_subquery_expr(*cmp_expr.right(), session);
  }

  return RC::SUCCESS;
}

RC prepare_subquery_expressions(Expression &expr, Session *session)
{
  RC rc = prepare_subquery_expr(expr, session);
  if (OB_FAIL(rc)) {
    return rc;
  }

  return ExpressionIterator::iterate_child_expr(expr, [&](unique_ptr<Expression> &child) -> RC {
    if (child == nullptr) {
      return RC::SUCCESS;
    }
    return prepare_subquery_expressions(*child, session);
  });
}

RC open_subquery_expressions(Expression &expr, Trx *trx)
{
  RC rc = RC::SUCCESS;
  if (expr.type() == ExprType::SUBQUERY) {
    static_cast<SubQueryExpr &>(expr).set_trx(trx);
  } else if (expr.type() == ExprType::IN_SUBQUERY) {
    rc = static_cast<InSubQueryExpr &>(expr).open(trx);
  }
  if (OB_FAIL(rc)) {
    return rc;
  }

  return ExpressionIterator::iterate_child_expr(expr, [&](unique_ptr<Expression> &child) -> RC {
    if (child == nullptr) {
      return RC::SUCCESS;
    }
    return open_subquery_expressions(*child, trx);
  });
}

RC close_subquery_expressions(Expression &expr)
{
  RC rc = RC::SUCCESS;
  if (expr.type() == ExprType::SUBQUERY) {
    rc = static_cast<SubQueryExpr &>(expr).close();
  } else if (expr.type() == ExprType::IN_SUBQUERY) {
    rc = static_cast<InSubQueryExpr &>(expr).close();
  }
  if (OB_FAIL(rc)) {
    return rc;
  }

  return ExpressionIterator::iterate_child_expr(expr, [&](unique_ptr<Expression> &child) -> RC {
    if (child == nullptr) {
      return RC::SUCCESS;
    }
    return close_subquery_expressions(*child);
  });
}
