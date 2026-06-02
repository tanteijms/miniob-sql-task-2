/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include "sql/expr/expression.h"
#include "sql/parser/parse_defs.h"
#include "sql/stmt/select_stmt.h"
#include "storage/trx/trx.h"

class Trx;
class Session;
class PhysicalOperator;

/**
 * @brief 未绑定的子查询（解析阶段）
 */
class UnboundSubQueryExpr : public Expression
{
public:
  explicit UnboundSubQueryExpr(SelectSqlNode &&select_sql);
  ~UnboundSubQueryExpr() override = default;

  ExprType type() const override { return ExprType::UNBOUND_SUBQUERY; }
  AttrType value_type() const override { return AttrType::UNDEFINED; }

  unique_ptr<Expression> copy() const override;

  RC get_value(const Tuple &tuple, Value &value) const override { return RC::INTERNAL; }

  SelectSqlNode &select_sql() { return select_sql_; }
  const SelectSqlNode &select_sql() const { return select_sql_; }

private:
  SelectSqlNode select_sql_;
};

/**
 * @brief 已绑定的非关联子查询
 */
class SubQueryExpr : public Expression
{
public:
  SubQueryExpr(unique_ptr<SelectStmt> select_stmt, unique_ptr<Expression> value_expr);
  ~SubQueryExpr() override;

  ExprType type() const override { return ExprType::SUBQUERY; }
  AttrType value_type() const override;

  unique_ptr<Expression> copy() const override;

  RC get_value(const Tuple &tuple, Value &value) const override;

  void set_trx(Trx *trx) { trx_ = trx; }

  RC open(Trx *trx);
  RC close();

  RC materialize_all_values(vector<Value> &values, const Tuple *outer_tuple = nullptr) const;

  RC materialize_scalar(const Tuple *outer_tuple = nullptr) const;

  void set_correlated(bool correlated) { correlated_ = correlated; }
  bool correlated() const { return correlated_; }

  SelectStmt *select_stmt() const { return select_stmt_.get(); }

  PhysicalOperator *physical_operator() const { return physical_operator_.get(); }

  void set_physical_operator(unique_ptr<PhysicalOperator> oper);

private:
  unique_ptr<SelectStmt>            select_stmt_;
  unique_ptr<Expression>              value_expr_;
  unique_ptr<PhysicalOperator>        physical_operator_;
  Trx                                  *trx_ = nullptr;
  mutable bool                        opened_ = false;
  mutable bool                        scalar_materialized_ = false;
  mutable bool                        scalar_empty_ = false;
  mutable Value                       scalar_cache_;
  bool                                correlated_ = false;
};

const Tuple *subquery_outer_tuple();

bool select_stmt_has_correlation(const SelectStmt *select_stmt);

/**
 * @brief expr IN (SELECT ...) / NOT IN
 */
class InSubQueryExpr : public Expression
{
public:
  InSubQueryExpr(unique_ptr<Expression> left, unique_ptr<Expression> subquery, bool not_in);
  ~InSubQueryExpr() override;

  ExprType type() const override { return ExprType::IN_SUBQUERY; }
  AttrType value_type() const override { return AttrType::BOOLEANS; }

  unique_ptr<Expression> copy() const override;

  RC get_value(const Tuple &tuple, Value &value) const override;

  RC open(Trx *trx);
  RC close();

  unique_ptr<Expression> &left() { return left_; }
  unique_ptr<Expression> &subquery() { return subquery_; }

private:
  unique_ptr<Expression> left_;
  unique_ptr<Expression> subquery_;
  bool                   not_in_;
  mutable bool           materialized_ = false;
  mutable vector<Value>  cached_values_;
};

RC prepare_subquery_expressions(Expression &expr, Session *session);
RC open_subquery_expressions(Expression &expr, Trx *trx);
RC close_subquery_expressions(Expression &expr);
