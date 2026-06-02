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
#include "sql/expr/tuple.h"
#include "storage/field/field_meta.h"
#include "storage/table/table.h"
#include "storage/trx/trx.h"

RC UpdatePhysicalOperator::open(Trx *trx)
{
  if (!children_.empty()) {
    unique_ptr<PhysicalOperator> &child = children_[0];
    RC                             rc   = child->open(trx);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to open child operator: %s", strrc(rc));
      return rc;
    }

    trx_ = trx;
    while (OB_SUCC(rc = child->next())) {
      Tuple *tuple = child->current_tuple();
      if (nullptr == tuple) {
        LOG_WARN("failed to get current tuple");
        return RC::INTERNAL;
      }

      RowTuple *row_tuple = static_cast<RowTuple *>(tuple);
      records_.emplace_back(row_tuple->record());
    }
    child->close();
  }

  for (Record &old_record : records_) {
    Record new_record;
    RC     rc = new_record.copy_data(old_record.data(), old_record.len());
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to copy record data. rc=%s", strrc(rc));
      return rc;
    }
    new_record.set_rid(old_record.rid());

    rc = table_->set_record_value(new_record, value_, field_);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to set value to record. rc=%s", strrc(rc));
      return rc;
    }

    rc = trx_->update_record(table_, old_record, new_record);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to update record: %s", strrc(rc));
      return rc;
    }
  }

  return RC::SUCCESS;
}

RC UpdatePhysicalOperator::next() { return RC::RECORD_EOF; }

RC UpdatePhysicalOperator::close() { return RC::SUCCESS; }
