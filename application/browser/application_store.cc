// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/browser/application_store.h"

#include <utility>

#include "xwalk/application/common/application_file_util.h"
#include "xwalk/application/common/db_store_sqlite_impl.h"
#include "xwalk/runtime/browser/runtime_context.h"

namespace xwalk {
namespace application {

const char ApplicationStore::kManifestPath[] = "manifest";

const char ApplicationStore::kApplicationPath[] = "path";

const char ApplicationStore::kInstallTime[] = "install_time";

const char ApplicationStore::kRegisteredEvents[] = "registered_events";

namespace {
inline const std::string GetRegisteredEventsPath(
    const std::string& application_id) {
  return application_id + "." + ApplicationStore::kRegisteredEvents;
}
}

ApplicationStore::ApplicationStore(xwalk::RuntimeContext* runtime_context)
    : runtime_context_(runtime_context),
      db_store_(new DBStoreSqliteImpl(runtime_context->GetPath())),
      applications_(new ApplicationMap) {
  db_store_->AddObserver(this);
  db_store_->InitDB();
}

ApplicationStore::~ApplicationStore() {
  db_store_->RemoveObserver(this);
}

bool ApplicationStore::AddApplication(
    scoped_refptr<const Application> application) {
  if (Contains(application->ID()))
    return true;

  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  base::DictionaryValue* manifest =
      application->GetManifest()->value()->DeepCopy();
  value->Set(ApplicationStore::kManifestPath, manifest);
  value->SetString(ApplicationStore::kApplicationPath,
                   application->Path().value());
  value->SetDouble(ApplicationStore::kInstallTime,
                   base::Time::Now().ToDoubleT());

  if (!db_store_->Insert(application->ID(), value.get()) ||
      !Insert(application))
    return false;

  return true;
}

bool ApplicationStore::RemoveApplication(const std::string& id) {
  ApplicationMapIterator it = applications_->find(id);
  if (it == applications_->end()) {
    LOG(ERROR) << "Application " << id << " is invalid.";
    return false;
  }
  applications_->erase(it);

  if (!db_store_->Delete(id)) {
    LOG(ERROR) << "Error occurred while trying to remove application"
                  "information with id "
               << id << " from database.";
    return false;
  }
  return true;
}

bool ApplicationStore::UpdateApplication(
    scoped_refptr<const Application> application) {
  bool ret = false;
  ApplicationMapIterator it = applications_->find(application->ID());
  if (it != applications_->end()) {
    scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue);
    base::DictionaryValue* manifest =
        application->GetManifest()->value()->DeepCopy();
    value->Set(ApplicationStore::kManifestPath, manifest);
    value->SetString(ApplicationStore::kApplicationPath,
                     application->Path().value());
    value->SetDouble(ApplicationStore::kInstallTime,
                     base::Time::Now().ToDoubleT());
    if (db_store_->Update(application->ID(), value.get())) {
      it->second = application;
      ret = true;
    }
  }
  return ret;
}

bool ApplicationStore::Contains(const std::string& app_id) const {
  return applications_->find(app_id) != applications_->end();
}

scoped_refptr<const Application> ApplicationStore::GetApplicationByID(
    const std::string& application_id) const {
  ApplicationMapIterator it = applications_->find(application_id);
  if (it != applications_->end()) {
    return it->second;
  }

  return NULL;
}

ApplicationStore::ApplicationMap*
ApplicationStore::GetInstalledApplications() const {
  return applications_.get();
}

base::ListValue* ApplicationStore::GetApplicationEvents(const std::string& id) {
  return static_cast<base::ListValue*>(
      db_store_->Query(GetRegisteredEventsPath(id)));
}

bool ApplicationStore::SetApplicationEvents(
    const std::string& id,
    base::ListValue* events) {
  base::ListValue* old_events = GetApplicationEvents(id);
  if (!old_events)
    return db_store_->Insert(GetRegisteredEventsPath(id), events);
  else if (!old_events->Equals(events))
    return db_store_->Update(GetRegisteredEventsPath(id), events);
  else
    return true;
}

void ApplicationStore::InitApplications(const base::DictionaryValue* db) {
  CHECK(db);

  for (base::DictionaryValue::Iterator it(*db); !it.IsAtEnd();
       it.Advance()) {
    const std::string& id = it.key();
    const base::DictionaryValue* value;
    const base::DictionaryValue* manifest;
    std::string app_path;
    if (!it.value().GetAsDictionary(&value) ||
        !value->GetString(ApplicationStore::kApplicationPath, &app_path) ||
        !value->GetDictionary(ApplicationStore::kManifestPath, &manifest))
      break;

    std::string error;
    scoped_refptr<Application> application =
        Application::Create(base::FilePath::FromUTF8Unsafe(app_path),
                            Manifest::INTERNAL,
                            *manifest,
                            id,
                            &error);
    if (!application) {
      LOG(ERROR) << "Load appliation error: " << error;
      break;
    }

    if (!Insert(application)) {
      LOG(ERROR) << "An error occurred while"
                    "initializing the application data.";
      break;
    }
  }
}

bool ApplicationStore::Insert(scoped_refptr<const Application> application) {
  return applications_->insert(
      std::pair<std::string, scoped_refptr<const Application> >(
          application->ID(), application)).second;
}

void ApplicationStore::OnDBValueChanged(const std::string& key,
                                        const base::Value* value) {
}

void ApplicationStore::OnDBInitializationCompleted(bool succeeded) {
  if (succeeded) {
    scoped_ptr<base::DictionaryValue> apps(
        static_cast<base::DictionaryValue*>(db_store_->Query("")));
    InitApplications(apps.get());
  }
}

}  // namespace application
}  // namespace xwalk
