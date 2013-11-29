// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_APPLICATION_COMMON_DB_STORE_SQLITE_IMPL_H_
#define XWALK_APPLICATION_COMMON_DB_STORE_SQLITE_IMPL_H_

#include <string>

#include "base/files/file_path.h"
#include "sql/connection.h"
#include "sql/meta_table.h"
#include "xwalk/application/common/db_store.h"

namespace xwalk {
namespace application {

// The Sqlite backend implementation of DBStore.
class DBStoreSqliteImpl: public DBStore {
 public:
  static const base::FilePath::CharType kDBFileName[];
  explicit DBStoreSqliteImpl(const base::FilePath& path);
  virtual ~DBStoreSqliteImpl();

  // Implement the DBStore inferface.
  virtual bool Insert(const std::string& key, base::Value* value) OVERRIDE;
  virtual bool Update(const std::string& key, base::Value* value) OVERRIDE;
  virtual bool Delete(const std::string& key) OVERRIDE;
  virtual base::Value* Query(const std::string& key) OVERRIDE;
  virtual bool InitDB() OVERRIDE;

 private:
  void ReportValueChanged(const std::string& key,
                          const base::Value* value);
  bool UpgradeToVersion1(const base::FilePath& v0_file);
  bool InsertApplication(const std::string& id, base::Value* value);
  bool UpdateApplication(const std::string& id, base::Value* value);
  bool DeleteApplication(const std::string& id);
  base::Value* QueryApplication(const std::string& id);
  base::Value* QueryInstalledApplications();
  bool InsertEventsValue(const std::string& id,
                         base::Value* value);
  bool UpdateEventsValue(const std::string& id,
                         base::Value* value);
  bool DeleteEventsValue(const std::string& id);
  base::Value* QueryEventsValue(const std::string& id);
  bool InsertPermissionsValue(const std::string& id,
                              base::Value* value);
  bool UpdatePermissionsValue(const std::string& id,
                              base::Value* value);
  bool DeletePermissionsValue(const std::string& id);
  base::Value* QueryPermissionsValue(const std::string& id);

  scoped_ptr<sql::Connection> sqlite_db_;
  sql::MetaTable meta_table_;
  bool db_initialized_;
};

}  // namespace application
}  // namespace xwalk

#endif  // XWALK_APPLICATION_COMMON_DB_STORE_SQLITE_IMPL_H_
