/* Copyright (c) OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/operator/update_physical_operator.h"

#include "common/log/log.h"
#include "sql/expr/subquery_expr.h"
#include "sql/expr/tuple.h"
#include "storage/field/field_meta.h"
#include "storage/table/table.h"
#include "storage/table/table_meta.h"
#include "storage/trx/trx.h"

namespace {

RC set_update_field_value(Table *table, Record &record, const FieldMeta *field, const Value &value)
{
  char             *record_data = record.data();
  const TableMeta  &table_meta  = table->table_meta();
  if (value.is_null()) {
    if (!field->nullable()) {
      LOG_WARN("NULL value not allowed for NOT NULL field. table=%s, field=%s", table->name(), field->name());
      return RC::INVALID_ARGUMENT;
    }
    table_meta.set_field_null(record_data, field->field_id(), true);
    return RC::SUCCESS;
  }

  table_meta.set_field_null(record_data, field->field_id(), false);
  if (field->type() != value.attr_type()) {
    Value real_value;
    RC    rc = Value::cast_to(value, field->type(), real_value);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to cast update value. table=%s, field=%s", table->name(), field->name());
      return rc;
    }
    return table->set_record_value(record, real_value, field);
  }
  return table->set_record_value(record, value, field);
}

}  // namespace

UpdatePhysicalOperator::UpdatePhysicalOperator(
    Table *table, const FieldMeta *field, unique_ptr<Expression> value_expr)
    : table_(table), field_(field), value_expr_(std::move(value_expr))
{}

RC UpdatePhysicalOperator::open(Trx *trx)
{
  trx_ = trx;

  if (value_expr_ == nullptr) {
    LOG_WARN("update value expression is null");
    return RC::INVALID_ARGUMENT;
  }

  RC rc = open_subquery_expressions(*value_expr_, trx);
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to open subquery expressions for update. rc=%s", strrc(rc));
    return rc;
  }

  if (!children_.empty()) {
    unique_ptr<PhysicalOperator> &child = children_[0];
    rc                                  = child->open(trx);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to open child operator: %s", strrc(rc));
      return rc;
    }

    while (OB_SUCC(rc = child->next())) {
      Tuple *tuple = child->current_tuple();
      if (nullptr == tuple) {
        LOG_WARN("failed to get current tuple");
        return RC::INTERNAL;
      }

      RowTuple *row_tuple = static_cast<RowTuple *>(tuple);
      records_.emplace_back(row_tuple->record());
    }
    if (rc != RC::RECORD_EOF) {
      LOG_WARN("failed to iterate child operator. rc=%s", strrc(rc));
      return rc;
    }
    child->close();
  }

  RowTuple row_tuple;
  row_tuple.set_schema(table_, table_->table_meta().field_metas());

  for (Record &old_record : records_) {
    row_tuple.set_record(&old_record);

    Value value;
    rc = value_expr_->get_value(row_tuple, value);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to evaluate update value expression. rc=%s", strrc(rc));
      RC rb_rc = rollback_updates();
      if (rb_rc != RC::SUCCESS) {
        LOG_ERROR("failed to rollback partial update. rc=%s", strrc(rb_rc));
      }
      return rc;
    }

    Record new_record;
    rc = new_record.copy_data(old_record.data(), old_record.len());
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to copy record data. rc=%s", strrc(rc));
      RC rb_rc = rollback_updates();
      if (rb_rc != RC::SUCCESS) {
        LOG_ERROR("failed to rollback partial update. rc=%s", strrc(rb_rc));
      }
      return rc;
    }
    new_record.set_rid(old_record.rid());

    rc = set_update_field_value(table_, new_record, field_, value);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to set value to record. rc=%s", strrc(rc));
      RC rb_rc = rollback_updates();
      if (rb_rc != RC::SUCCESS) {
        LOG_ERROR("failed to rollback partial update. rc=%s", strrc(rb_rc));
      }
      return rc;
    }

    rc = trx_->update_record(table_, old_record, new_record);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to update record: %s", strrc(rc));
      RC rb_rc = rollback_updates();
      if (rb_rc != RC::SUCCESS) {
        LOG_ERROR("failed to rollback partial update. rc=%s", strrc(rb_rc));
      }
      return rc;
    }

    updated_new_records_.push_back(std::move(new_record));
  }

  return RC::SUCCESS;
}

RC UpdatePhysicalOperator::rollback_updates()
{
  RC rc = RC::SUCCESS;
  for (int i = static_cast<int>(updated_new_records_.size()) - 1; i >= 0; i--) {
    Record &new_record = updated_new_records_[static_cast<size_t>(i)];
    Record &old_record = records_[static_cast<size_t>(i)];
    RC      rb_rc      = trx_->update_record(table_, new_record, old_record);
    if (OB_FAIL(rb_rc)) {
      LOG_ERROR("failed to rollback updated record. rc=%s", strrc(rb_rc));
      rc = rb_rc;
    }
  }
  updated_new_records_.clear();
  return rc;
}

RC UpdatePhysicalOperator::next() { return RC::RECORD_EOF; }

RC UpdatePhysicalOperator::close()
{
  if (value_expr_ != nullptr) {
    close_subquery_expressions(*value_expr_);
  }
  records_.clear();
  updated_new_records_.clear();
  return RC::SUCCESS;
}
