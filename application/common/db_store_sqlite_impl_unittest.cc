// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/common/db_store_sqlite_impl.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "xwalk/application/browser/application_store.h"

namespace xwalk {
namespace application {

class DBStoreSqliteImplTest : public testing::Test {
 public:
  void TestInit() {
    base::FilePath tmp;
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &tmp));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDirUnderPath(tmp));
    db_store_.reset(new DBStoreSqliteImpl(temp_dir_.path()));
    ASSERT_TRUE(db_store_->InitDB());
  }

 protected:
  base::ScopedTempDir temp_dir_;
  scoped_ptr<DBStoreSqliteImpl> db_store_;
};

TEST_F(DBStoreSqliteImplTest, CreateDBFile) {
  TestInit();
  EXPECT_TRUE(base::PathExists(
      temp_dir_.path().Append(DBStoreSqliteImpl::kDBFileName)));
}

TEST_F(DBStoreSqliteImplTest, DBQueryAndInsert) {
  TestInit();
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  value->SetString(ApplicationStore::kApplicationPath, "no path");
  base::DictionaryValue* manifest = new base::DictionaryValue;
  manifest->SetString("a", "b");
  value->Set(ApplicationStore::kManifestPath, manifest);
  value->SetDouble(ApplicationStore::kInstallTime, 0);
  std::string id("test_id");
  ASSERT_TRUE(db_store_->Insert(id, value.release()));

  scoped_ptr<base::DictionaryValue> old_value(
      static_cast<base::DictionaryValue*>(db_store_->Query("")->DeepCopy()));
  EXPECT_TRUE(old_value->HasKey(id));
}

TEST_F(DBStoreSqliteImplTest, DBDelete) {
  TestInit();
  scoped_ptr<base::DictionaryValue> old_value(
      static_cast<base::DictionaryValue*>(db_store_->Query("")->DeepCopy()));
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  value->SetString(ApplicationStore::kApplicationPath, "no path");
  base::DictionaryValue* manifest = new base::DictionaryValue;
  manifest->SetString("a", "b");
  value->Set(ApplicationStore::kManifestPath, manifest);
  value->SetDouble(ApplicationStore::kInstallTime, 0);
  std::string id("test_id");
  ASSERT_TRUE(db_store_->Insert(id, value.release()));

  EXPECT_TRUE(db_store_->Delete(id));
}

TEST_F(DBStoreSqliteImplTest, DBUpgradeToV1) {
  base::FilePath tmp;
  ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &tmp));
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDirUnderPath(tmp));

  scoped_ptr<base::DictionaryValue> db_value(new base::DictionaryValue);
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  value->SetString(ApplicationStore::kApplicationPath, "path");
  base::DictionaryValue* manifest = new base::DictionaryValue;
  manifest->SetString("a", "b");
  value->Set(ApplicationStore::kManifestPath, manifest);
  value->SetDouble(ApplicationStore::kInstallTime, 0);
  db_value->Set("test_id", value.release());

  base::FilePath v0_db_file(
      temp_dir_.path().AppendASCII("applications_db"));
  JSONFileValueSerializer serializer(v0_db_file);
  ASSERT_TRUE(serializer.Serialize(*db_value.get()));
  ASSERT_TRUE(base::PathExists(v0_db_file));

  db_store_.reset(new DBStoreSqliteImpl(temp_dir_.path()));
  ASSERT_TRUE(db_store_->InitDB());
  ASSERT_FALSE(base::PathExists(v0_db_file));
  EXPECT_TRUE(db_value->Equals(db_store_->Query("")));
}

TEST_F(DBStoreSqliteImplTest, DBUpdate) {
  TestInit();
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  value->SetString(ApplicationStore::kApplicationPath, "path1");
  base::DictionaryValue* manifest = new base::DictionaryValue;
  manifest->SetString("a", "b");
  value->Set(ApplicationStore::kManifestPath, manifest);
  value->SetDouble(ApplicationStore::kInstallTime, 0);
  std::string id("test_id");
  scoped_ptr<base::DictionaryValue> new_value(value->DeepCopy());
  ASSERT_TRUE(db_store_->Insert(id, value.release()));

  new_value->SetString(ApplicationStore::kApplicationPath, "path2");
  base::DictionaryValue* new_manifest = manifest->DeepCopy();
  new_manifest->SetString("a", "c");
  new_value->Set(ApplicationStore::kManifestPath, new_manifest);
  new_value->SetDouble(ApplicationStore::kInstallTime, 1);
  scoped_ptr<base::DictionaryValue> changed_value(new_value->DeepCopy());
  ASSERT_TRUE(db_store_->Update(id, new_value.release()));

  scoped_ptr<base::DictionaryValue> db_cache(
      static_cast<base::DictionaryValue*>(db_store_->Query("")->DeepCopy()));
  base::DictionaryValue* db_value;
  ASSERT_TRUE(db_cache->GetDictionary(id, &db_value));
  EXPECT_TRUE(changed_value->Equals(db_value));
}

}  // namespace application
}  // namespace xwalk
