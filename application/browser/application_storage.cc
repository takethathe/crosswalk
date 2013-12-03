// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/browser/application_storage.h"

#include <utility>

#include "xwalk/application/common/application_file_util.h"
#include "xwalk/runtime/browser/runtime_context.h"

namespace xwalk {
namespace application {

ApplicationStorage::ApplicationStorage(const base::FilePath& path)
    : data_path_(path),
      impl_(new ApplicationStorageImpl(path)) {
  impl_->InitDB();
  applications_.reset(impl_->GetInstalledApplications());
}

ApplicationStorage::~ApplicationStorage() {
}

bool ApplicationStorage::AddApplication(
    scoped_refptr<ApplicationData> application) {
  if (Contains(application->ID())) {
    LOG(WARNING) << "Application " << application->ID()
                 << " has been already installed";
    return false;
  }

  if (!impl_->AddApplication(application.get(), base::Time::Now()) ||
      !Insert(application))
    return false;
  FOR_EACH_OBSERVER(Observer, observers_, ApplicationAdded(application));
  return true;
}

bool ApplicationStorage::RemoveApplication(const std::string& id) {
  if (applications_->erase(id) != 1) {
    LOG(ERROR) << "Application " << id << " is invalid.";
    return false;
  }

  if (!impl_->RemoveApplication(id)) {
    LOG(ERROR) << "Error occurred while trying to remove application"
                  "information with id "
               << id << " from database.";
    return false;
  }
  FOR_EACH_OBSERVER(
      Observer, observers_, ApplicationRemoved(id));
  return true;
}

bool ApplicationStorage::UpdateApplication(
    scoped_refptr<ApplicationData> application) {
  ApplicationData::ApplicationDataMapIterator it =
      applications_->find(application->ID());
  if (it == applications_->end()) {
    LOG(ERROR) << "Application " << application->ID() << " is invalid.";
    return false;
  }

  it->second = application;
  if (!impl_->UpdateApplication(application, base::Time::Now()))
    return false;

  FOR_EACH_OBSERVER(
      Observer, observers_, ApplicationUpdated(application));
  return true;
}

bool ApplicationStorage::Contains(const std::string& app_id) const {
  return applications_->find(app_id) != applications_->end();
}

scoped_refptr<ApplicationData> ApplicationStorage::GetApplicationByID(
    const std::string& application_id) const {
  ApplicationData::ApplicationDataMapIterator it =
      applications_->find(application_id);
  if (it != applications_->end()) {
    return it->second;
  }

  return NULL;
}

const ApplicationData::ApplicationDataMap*
ApplicationStorage::GetInstalledApplications() const {
  return applications_.get();
}

const std::vector<std::string> ApplicationStorage::GetApplicationEvents(
    const std::string& id) {
  scoped_refptr<ApplicationData> application = GetApplicationByID(id);
  if (!application) {
    LOG(ERROR) << "Application " << id << " is not installed. "
               << "Could not get system events for it.";
    return std::vector<std::string>();
  }
  return application->GetEvents();
}

bool ApplicationStorage::SetApplicationEvents(
    const std::string& id,
    const std::vector<std::string>& events) {
  scoped_refptr<ApplicationData> application = GetApplicationByID(id);
  if (application) {
    LOG(ERROR) << "Application " << id << " is not installed. "
               << "Could not set system events for it.";
    return false;
  }

  application->SetEvents(events);
  if (!events.size())
    return impl_->DeleteEvents(id);

  return impl_->UpdateEvents(id, events);
}

bool ApplicationStorage::Insert(scoped_refptr<ApplicationData> application) {
  return applications_->insert(
      std::pair<std::string, scoped_refptr<ApplicationData> >(
          application->ID(), application)).second;
}

void ApplicationStorage::AddObserver(Observer *observer) {
  observers_.AddObserver(observer);
}

void ApplicationStorage::RemoveObserver(Observer *observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace application
}  // namespace xwalk
