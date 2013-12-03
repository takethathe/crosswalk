// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_APPLICATION_BROWSER_APPLICATION_STORAGE_H_
#define XWALK_APPLICATION_BROWSER_APPLICATION_STORAGE_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "xwalk/application/common/application_data.h"
#include "xwalk/application/common/application_storage_impl.h"

namespace xwalk {
class Runtime;
class RuntimeContext;
}

namespace xwalk {
namespace application {

class ApplicationStorage {
 public:
  class Observer {
   public:
    virtual void ApplicationAdded(
        scoped_refptr<ApplicationData> application) = 0;
    virtual void ApplicationRemoved(const std::string& id) = 0;
    virtual void ApplicationUpdated(
        scoped_refptr<ApplicationData> application) = 0;

   protected:
    virtual ~Observer() {}
  };
  explicit ApplicationStorage(const base::FilePath& path);
  ~ApplicationStorage();

  bool AddApplication(scoped_refptr<ApplicationData> application);

  bool RemoveApplication(const std::string& id);

  bool UpdateApplication(scoped_refptr<ApplicationData> application);

  bool Contains(const std::string& app_id) const;

  scoped_refptr<ApplicationData> GetApplicationByID(
      const std::string& application_id) const;

  const ApplicationData::ApplicationDataMap* GetInstalledApplications() const;

  bool SetApplicationEvents(const std::string& id,
                            const std::vector<std::string>& events);
  const std::vector<std::string> GetApplicationEvents(const std::string& id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  bool Insert(scoped_refptr<ApplicationData> application);
  base::FilePath data_path_;
  scoped_ptr<ApplicationStorageImpl> impl_;
  scoped_ptr<ApplicationData::ApplicationDataMap> applications_;
  ObserverList<Observer, true> observers_;
  DISALLOW_COPY_AND_ASSIGN(ApplicationStorage);
};

}  // namespace application
}  // namespace xwalk

#endif  // XWALK_APPLICATION_BROWSER_APPLICATION_STORAGE_H_
