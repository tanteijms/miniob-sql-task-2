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
// Created by Wangyunlai on 2022/5/22.
//

#include "sql/stmt/update_stmt.h"

#include "common/log/log.h"
#include "sql/stmt/filter_stmt.h"
#include "storage/db/db.h"
#include "storage/field/field_meta.h"
#include "storage/table/table.h"

UpdateStmt::UpdateStmt(Table *table, FilterStmt *filter_stmt, const FieldMeta *field, const Value &value)
    : table_(table), filter_stmt_(filter_stmt), field_(field), value_(value)
{}

UpdateStmt::~UpdateStmt()
{
  if (filter_stmt_ != nullptr) {
    delete filter_stmt_;
    filter_stmt_ = nullptr;
  }
}

RC UpdateStmt::create(Db *db, const UpdateSqlNode &update_sql, Stmt *&stmt)
{
  const char *table_name = update_sql.relation_name.c_str();
  if (nullptr == db || nullptr == table_name) {
    LOG_WARN("invalid argument. db=%p, table_name=%p", db, table_name);
    return RC::INVALID_ARGUMENT;
  }

  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  const FieldMeta *field = table->table_meta().field(update_sql.attribute_name.c_str());
  if (nullptr == field) {
    LOG_WARN("no such field. table=%s, field=%s", table_name, update_sql.attribute_name.c_str());
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }

  Value value = update_sql.value;
  if (value.is_null()) {
    if (!field->nullable()) {
      LOG_WARN("field does not allow null. table=%s, field=%s", table_name, field->name());
      return RC::INVALID_ARGUMENT;
    }
    value.set_type(field->type());
  } else if (field->type() != value.attr_type()) {
    Value cast_value;
    RC    rc = Value::cast_to(value, field->type(), cast_value);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to cast update value. table=%s, field=%s, rc=%s",
          table_name, field->name(), strrc(rc));
      return rc;
    }
    value = cast_value;
  }

  unordered_map<string, Table *> table_map;
  table_map.emplace(table_name, table);

  FilterStmt *filter_stmt = nullptr;
  RC          rc          = FilterStmt::create(db,
      table,
      &table_map,
      update_sql.conditions.data(),
      static_cast<int>(update_sql.conditions.size()),
      filter_stmt);
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to create filter statement. rc=%s", strrc(rc));
    return rc;
  }

  stmt = new UpdateStmt(table, filter_stmt, field, value);
  return RC::SUCCESS;
}
