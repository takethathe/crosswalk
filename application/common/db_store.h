// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_APPLICATION_COMMON_DB_STORE_H_
#define XWALK_APPLICATION_COMMON_DB_STORE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "xwalk/application/common/application.h"

namespace xwalk {
namespace application {

class DBStore {
 public:
  // Observer interface for monitoring DBStore.
  class Observer {
   public:
    // Called when the value for the given |key| in the store changes.
    virtual void OnDBValueChanged(const std::string& key,
                                  const base::Value* value) = 0;
    // Notification about the DBStore being fully initialized.
    virtual void OnDBInitializationCompleted(bool succeeded) = 0;

   protected:
    virtual ~Observer() {}
  };
  explicit DBStore(base::FilePath path);
  virtual ~DBStore();
  virtual bool Insert(const std::string& key, base::Value* value) = 0;
  virtual bool Update(const std::string& key, base::Value* value) = 0;
  virtual bool Delete(const std::string& key) = 0;
  virtual base::Value* Query(const std::string& key) = 0;

  void AddObserver(DBStore::Observer* observer) {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(DBStore::Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // Initialize the database, calling OnInitializationCompleted for each
  // observer on completion.
  virtual bool InitDB() = 0;

 protected:
  base::FilePath data_path_;
  ObserverList<DBStore::Observer, true> observers_;
};

}  // namespace application
}  // namespace xwalk

#endif  // XWALK_APPLICATION_COMMON_DB_STORE_H_
