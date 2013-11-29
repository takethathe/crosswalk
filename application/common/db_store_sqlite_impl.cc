// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/common/db_store_sqlite_impl.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "xwalk/application/browser/application_store.h"
#include "xwalk/application/common/db_store_constants.h"

namespace db_fields = xwalk::db_store_constants;
namespace xwalk {
namespace application {

const base::FilePath::CharType DBStoreSqliteImpl::kDBFileName[] =
    FILE_PATH_LITERAL("applications.db");

const char kEventSeparator = ';';

// Switching the JSON format DB(version 0) to SQLite backend version 1,
// should migrate all data from JSON DB to SQLite applications table.
static const int kVersionNumber = 1;

namespace {

inline const base::FilePath GetDBPath(const base::FilePath& path) {
  return path.Append(DBStoreSqliteImpl::kDBFileName);
}

inline const std::string GetManifestPath(const std::string& application_id) {
  return application_id + "." + ApplicationStore::kManifestPath;
}

inline const std::string GetApplicationPath(const std::string& application_id) {
  return application_id + "." + ApplicationStore::kApplicationPath;
}

inline const std::string GetInstallTimePath(const std::string& application_id) {
  return application_id + "." + ApplicationStore::kInstallTime;
}

std::vector<std::string> FormatKeys(const std::string& key) {
  std::vector<std::string> splitted_keys;
  base::SplitString(key, '.', &splitted_keys);
  return splitted_keys;
}

// Initializes the applications table, returning true on success.
bool InitTables(sql::Connection* db) {
  sql::Transaction transaction(db);
  transaction.Begin();
  // The table is named "applications", the primary key is "id".
  if (!db->DoesTableExist(db_fields::kAppTableName)) {
    if (!db->Execute(db_fields::kCreateAppTableOp)) {
      LOG(ERROR) << "Unable to open applications table.";
      return false;
    }
  }

  if (!db->DoesTableExist(db_fields::kEventTableName)) {
    if (!db->Execute(db_fields::kCreateEventTableOp)) {
      LOG(ERROR) << "Unable to open registered events table.";
      return false;
    }
  }
  return transaction.Commit();
}

}  // namespace

DBStoreSqliteImpl::DBStoreSqliteImpl(const base::FilePath& path)
    : DBStore(path),
      sqlite_db_(new sql::Connection),
      db_initialized_(false) {
  // Ensure the parent directory for database file is created before reading
  // from it.
  if (!base::PathExists(path) && !file_util::CreateDirectory(path))
    return;

  sqlite_db_->set_page_size(4096);
  sqlite_db_->set_cache_size(128);
}

bool DBStoreSqliteImpl::UpgradeToVersion1(const base::FilePath& v0_file) {
  JSONFileValueSerializer serializer(v0_file);
  int error_code;
  std::string error;
  scoped_ptr<base::Value> old_db(serializer.Deserialize(&error_code, &error));
  if (!old_db) {
    LOG(ERROR) << "Unable to read applications information from JSON DB, "
                  "the error message is: "
               << error;
    return false;
  }

  scoped_ptr<base::Value> value;
  for (base::DictionaryValue::Iterator it(
           *static_cast<base::DictionaryValue*>(old_db.get()));
       !it.IsAtEnd(); it.Advance()) {
    value.reset(it.value().DeepCopy());
    if (!InsertApplication(it.key(), value.get()))
      return false;
  }
  meta_table_.SetVersionNumber(1);

  return true;
}

DBStoreSqliteImpl::~DBStoreSqliteImpl() {
  if (sqlite_db_.get())
    sqlite_db_.reset();
}

bool DBStoreSqliteImpl::InitDB() {
  bool does_db_exist = base::PathExists(GetDBPath(data_path_));

  if (!sqlite_db_->Open(GetDBPath(data_path_))) {
    LOG(ERROR) << "Unable to open applications DB.";
    sqlite_db_.reset();
    return false;
  }

  sqlite_db_->Preload();
 if (!meta_table_.Init(sqlite_db_.get(), kVersionNumber, kVersionNumber) ||
      meta_table_.GetVersionNumber() != kVersionNumber) {
    LOG(ERROR) << "Unable to init the META table.";
    return false;
  }

  if (!InitTables(sqlite_db_.get())) {
    sqlite_db_.reset();
    return false;
  }

  base::FilePath v0_file =
      data_path_.Append(FILE_PATH_LITERAL("applications_db"));
  if (base::PathExists(v0_file) &&
      !does_db_exist) {
    if (!UpgradeToVersion1(v0_file)) {
      LOG(ERROR) << "Unable to migrate database from JSON format to SQLite.";
      return false;
    }
    // After migrated to SQLite, delete the old JSON DB file is safe,
    // since all information has been migrated and it will not be used anymore.
    if (!base::DeleteFile(v0_file, false)) {
      LOG(ERROR) << "Unalbe to delete old JSON DB file.";
      return false;
    }
  }

  if (!sqlite_db_->Execute("PRAGMA foreign_keys=ON")) {
    LOG(ERROR) << "Unable to enforce foreign key contraints.";
    return false;
  }
  db_initialized_ = true;

  FOR_EACH_OBSERVER(DBStore::Observer,
                    observers_,
                    OnDBInitializationCompleted(true));
  return true;
}

bool DBStoreSqliteImpl::Insert(const std::string& key,
                               base::Value* value) {
  if (!db_initialized_)
    return false;

  std::vector<std::string> keys = FormatKeys(key);
  if (keys.size() < 1)
    return false;

  bool ret = false;
  std::string application_id = keys[0];
  switch (keys.size()) {
    case 1:
      ret = InsertApplication(application_id, value);
      break;
    case 2:
      if (keys[1].compare(db_fields::kEventsName) == 0)
        ret = InsertEventsValue(key, value);
      break;
    case 3:
      break;
    default:
      break;
  }

  if (ret)
    ReportValueChanged(key, value);

  return ret;
}

bool DBStoreSqliteImpl::Update(const std::string& key,
                               base::Value* value) {
  if (!db_initialized_)
    return false;

  std::vector<std::string> keys = FormatKeys(key);
  if (keys.size() < 1)
    return false;

  bool ret = false;
  std::string application_id = keys[0];
  switch (keys.size()) {
    case 1:
      ret = UpdateApplication(application_id, value);
      break;
    case 2:
      if (keys[1].compare(db_fields::kEventsName) == 0)
        ret = UpdateEventsValue(key, value);
      break;
    case 3:
      break;
    default:
      break;
  }

  if (ret)
    ReportValueChanged(key, value);

  return ret;
}

bool DBStoreSqliteImpl::Delete(const std::string& key) {
  if (!db_initialized_)
    return false;

  std::vector<std::string> keys = FormatKeys(key);
  if (keys.size() < 1)
    return false;

  bool ret = false;
  std::string application_id = keys[0];
  switch (keys.size()) {
    case 1:
      ret = DeleteApplication(application_id);
      break;
    case 2:
      break;
    case 3:
      break;
    default:
      break;
  }

  if (ret)
    ReportValueChanged(key, NULL);

  return ret;
}

base::Value* DBStoreSqliteImpl::Query(const std::string &key) {
  if (!db_initialized_)
    return NULL;

  std::vector<std::string> keys = FormatKeys(key);
  base::Value* value = NULL;
  switch (keys.size()) {
    case 0:
      value = QueryInstalledApplications();
      break;
    case 1:
      value = QueryApplication(keys[0]);
      break;
    case 2:
      if (keys[1].compare(db_fields::kEventsName) == 0)
        value = QueryEventsValue(keys[0]);
      break;
    case 3:
      break;
    default:
      break;
  }
  return value;
}

bool DBStoreSqliteImpl::InsertApplication(
    const std::string& id, base::Value* value) {
  if (!value) {
    LOG(ERROR) << "A value is needed when inserting into DB.";
    return false;
  }

  std::string manifest;
  JSONStringValueSerializer serializer(&manifest);
  base::Value* manifest_value;
  if (!static_cast<base::DictionaryValue*>(value)->Get(
          ApplicationStore::kManifestPath, &manifest_value) ||
      !serializer.Serialize(*manifest_value)) {
    LOG(ERROR) << "An error occured when serializing the manifest value.";
    return false;
  }

  std::string path;
  if (!static_cast<base::DictionaryValue*>(value)->GetString(
          ApplicationStore::kApplicationPath, &path)) {
    LOG(ERROR) << "An error occured when getting path information.";
    return false;
  }

  double install_time;
  if (!static_cast<base::DictionaryValue*>(value)->GetDouble(
          ApplicationStore::kInstallTime, &install_time)) {
    LOG(ERROR) << "An error occured when getting install time information.";
    return false;
  }

  sql::Transaction transaction(sqlite_db_.get());
  if (!transaction.Begin())
    return false;

  sql::Statement smt(sqlite_db_->GetCachedStatement(
      SQL_FROM_HERE, db_fields::kSetApplicationWithBindOp));
  if (!smt.is_valid()) {
    LOG(ERROR) << "Unable to insert application info into DB.";
    return false;
  }
  smt.BindString(0, manifest);
  smt.BindString(1, path);
  smt.BindDouble(2, install_time);
  smt.BindString(3, id);
  if (!smt.Run()) {
    LOG(ERROR) << "An error occured when inserting application info into DB.";
    return false;
  }

  return transaction.Commit();
}

bool DBStoreSqliteImpl::UpdateApplication(
    const std::string& id, base::Value* value) {
  if (!value) {
    LOG(ERROR) << "A value is needed when updating in DB.";
    return false;
  }

  std::string manifest;
  JSONStringValueSerializer serializer(&manifest);
  base::Value* manifest_value;
  if (!static_cast<base::DictionaryValue*>(value)->Get(
          ApplicationStore::kManifestPath, &manifest_value) ||
      !serializer.Serialize(*manifest_value)) {
    LOG(ERROR) << "An error occurred when serializing the manifest value.";
    return false;
  }

  std::string path;
  if (!static_cast<base::DictionaryValue*>(value)->GetString(
          ApplicationStore::kApplicationPath, &path)) {
    LOG(ERROR) << "An error occurred when getting path information.";
    return false;
  }

  double install_time;
  if (!static_cast<base::DictionaryValue*>(value)->GetDouble(
          ApplicationStore::kInstallTime, &install_time)) {
    LOG(ERROR) << "An error occurred when getting install time information.";
    return false;
  }

  sql::Transaction transaction(sqlite_db_.get());
  if (!transaction.Begin())
    return false;

  sql::Statement smt(sqlite_db_->GetCachedStatement(
      SQL_FROM_HERE, db_fields::kUpdateApplicationWithBindOp));
  if (!smt.is_valid()) {
    LOG(ERROR) << "Unable to update application info in DB.";
    return false;
  }
  smt.BindString(0, manifest);
  smt.BindString(1, path);
  smt.BindDouble(2, install_time);
  smt.BindString(3, id);
  if (!smt.Run()) {
    LOG(ERROR) << "An error occurred when updating application info in DB.";
    return false;
  }

  return transaction.Commit();
}

bool DBStoreSqliteImpl::DeleteApplication(const std::string& id) {
  sql::Transaction transaction(sqlite_db_.get());
  if (!transaction.Begin())
    return false;

  sql::Statement smt(sqlite_db_->GetCachedStatement(
      SQL_FROM_HERE, db_fields::kDeleteApplicationWithBindOp));
  if (!smt.is_valid()) {
    LOG(ERROR) << "Unable to delete application info in DB.";
    return false;
  }

  smt.BindString(0, id);
  if (!smt.Run()) {
    LOG(ERROR) << "An error occurred delete application "
                  "information from DB.";
    return false;
  }

  return transaction.Commit();
}

base::Value* DBStoreSqliteImpl::QueryApplication(const std::string& id) {
  sql::Statement smt(sqlite_db_->GetCachedStatement(
      SQL_FROM_HERE, db_fields::kGetApplicationWithBindOp));
  if (!smt.is_valid())
    return NULL;

  smt.BindString(0, id);
  std::string error_msg;
  base::DictionaryValue* value = NULL;
  while (smt.Step()) {
    int error_code;
    std::string manifest_str = smt.ColumnString(1);
    JSONStringValueSerializer serializer(&manifest_str);
    base::Value* manifest = serializer.Deserialize(&error_code, &error_msg);
    if (manifest == NULL) {
      LOG(ERROR) << "An error occured when deserializing the manifest, "
                    "the error message is: "
                 << error_msg;
      return NULL;
    }
    std::string path = smt.ColumnString(2);
    double install_time = smt.ColumnDouble(3);

    value = new base::DictionaryValue;
    value->Set(ApplicationStore::kManifestPath, manifest);
    value->SetString(ApplicationStore::kApplicationPath, path);
    value->SetDouble(ApplicationStore::kInstallTime, install_time);
  }

  return value;
}

base::Value* DBStoreSqliteImpl::QueryInstalledApplications() {
  sql::Statement smt(sqlite_db_->GetCachedStatement(
      SQL_FROM_HERE, db_fields::kGetInstalledApplicationsOp));
  if (!smt.is_valid())
    return NULL;

  std::string error_msg;
  base::DictionaryValue* value = new base::DictionaryValue;
  while (smt.Step()) {
    std::string id = smt.ColumnString(0);

    int error_code;
    std::string manifest_str = smt.ColumnString(1);
    JSONStringValueSerializer serializer(&manifest_str);
    base::Value* manifest = serializer.Deserialize(&error_code, &error_msg);
    if (manifest == NULL) {
      LOG(ERROR) << "An error occured when deserializing the manifest, "
                    "the error message is: "
                 << error_msg;
      return NULL;
    }
    std::string path = smt.ColumnString(2);
    double install_time = smt.ColumnDouble(3);

    base::DictionaryValue* app_value = new base::DictionaryValue;
    app_value->Set(ApplicationStore::kManifestPath, manifest);
    app_value->SetString(ApplicationStore::kApplicationPath, path);
    app_value->SetDouble(ApplicationStore::kInstallTime, install_time);
    value->Set(id, app_value);
  }

  return value;
}

bool DBStoreSqliteImpl::InsertEventsValue(const std::string& id,
                                          base::Value* value) {
  if (!value)
    return false;

  base::ListValue* events = static_cast<base::ListValue*>(value);
  sql::Transaction transaction(sqlite_db_.get());

  std::vector<std::string> events_vec;
  for (size_t i = 0; i < events->GetSize(); ++i) {
    std::string event;
    if (events->GetString(i, &event))
      events_vec.push_back(event);
  }
  std::string events_list(JoinString(events_vec, kEventSeparator));

  if (!transaction.Begin())
    return false;

  sql::Statement smt(sqlite_db_->GetCachedStatement(
      SQL_FROM_HERE, db_fields::kInsertEventsWithBindOp));
  smt.BindString(0, events_list);
  smt.BindString(1, id);
  if (!smt.Run()) {
    LOG(ERROR) << "An error occurred when inserting event information into DB.";
    return false;
  }

  return transaction.Commit();
}

bool DBStoreSqliteImpl::UpdateEventsValue(const std::string &id,
                                          base::Value* value) {
  if (!value)
    return false;

  base::ListValue* events = static_cast<base::ListValue*>(value);
  sql::Transaction transaction(sqlite_db_.get());

  std::vector<std::string> events_vec;
  for (size_t i = 0; i < events->GetSize(); ++i) {
    std::string event;
    if (events->GetString(i, &event))
      events_vec.push_back(event);
  }
  std::string events_list(JoinString(events_vec, kEventSeparator));

  if (!transaction.Begin())
    return false;

  sql::Statement smt(sqlite_db_->GetCachedStatement(
      SQL_FROM_HERE, db_fields::kUpdateEventsWithBindOp));
  smt.BindString(0, events_list);
  smt.BindString(1, id);
  if (!smt.Run()) {
    LOG(ERROR) << "An error occurred when updating event information in DB.";
    return false;
  }

  return transaction.Commit();
}

bool DBStoreSqliteImpl::DeleteEventsValue(
    const std::string& id) {
  sql::Transaction transaction(sqlite_db_.get());
  if (!transaction.Begin())
    return false;

  sql::Statement smt(sqlite_db_->GetCachedStatement(
      SQL_FROM_HERE, db_fields::kDeleteEventsWithBindOp));
  smt.BindString(0, id);

  if (!smt.Run()) {
    LOG(ERROR) << "An error occurred when deleting event information from DB.";
    return false;
  }

  return transaction.Commit();
}

base::Value* DBStoreSqliteImpl::QueryEventsValue(const std::string& id) {
  sql::Statement smt(sqlite_db_->GetCachedStatement(
      SQL_FROM_HERE, db_fields::kGetEventsWithBindOp));
  if (!smt.is_valid())
    return NULL;

  smt.BindString(0, id);
  std::string error_msg;
  base::ListValue* value = NULL;
  while (smt.Step()) {
    std::vector<std::string> events_vec;
    value = new base::ListValue;
    base::SplitString(smt.ColumnString(0), kEventSeparator, &events_vec);
    value->AppendStrings(events_vec);
  }

  return value;
}

bool DBStoreSqliteImpl::InsertPermissionsValue(const std::string& id,
                                               base::Value* value) {
  return true;
}

bool DBStoreSqliteImpl::UpdatePermissionsValue(const std::string& id,
                                               base::Value* value) {
  return true;
}

bool DBStoreSqliteImpl::DeletePermissionsValue(const std::string &id) {
  return true;
}

base::Value* DBStoreSqliteImpl::QueryPermissionsValue(const std::string &id) {
  return NULL;
}

void DBStoreSqliteImpl::ReportValueChanged(const std::string& key,
                                           const base::Value* value) {
  FOR_EACH_OBSERVER(
      DBStore::Observer, observers_, OnDBValueChanged(key, value));
}

}  // namespace application
}  // namespace xwalk
