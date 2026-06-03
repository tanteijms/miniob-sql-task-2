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
// Created by wangyunlai.wyl on 2021/5/19.
//

#include "storage/index/index.h"
#include <cstring>

RC Index::init(const IndexMeta &index_meta, const vector<FieldMeta> &field_metas)
{
  if (field_metas.empty()) {
    return RC::INVALID_ARGUMENT;
  }
  index_meta_ = index_meta;
  field_metas_ = field_metas;
  attr_length_ = 0;
  for (const FieldMeta &field_meta : field_metas_) {
    attr_length_ += field_meta.len();
  }
  return RC::SUCCESS;
}

RC Index::make_key(const char *record, char *key) const
{
  int offset = 0;
  for (const FieldMeta &field_meta : field_metas_) {
    if (field_meta.field_id() >= 0) {
      const int byte_index = field_meta.field_id() / 8;
      const int bit_index  = field_meta.field_id() % 8;
      if ((record[byte_index] & (1 << bit_index)) != 0) {
        return RC::RECORD_INVALID_KEY;
      }
    }
    memcpy(key + offset, record + field_meta.offset(), field_meta.len());
    offset += field_meta.len();
  }
  return RC::SUCCESS;
}
