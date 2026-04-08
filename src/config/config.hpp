#pragma once

#include <memory>
#include <string>
#include <vector>
//
#include <QMenu>
#include <QString>

// @TODO: Put back string with cleanup operation
using secure_string = std::string;

namespace ads {
  class CDockManager;
}

class abstract_exchange;
class abstract_dataset_manager;
class QNetworkAccessManager;

// ----------------------------------------------------------------------------
struct app_settings
{
  // file names for data and log storage
  QString iniFileName;
  std::string logFileName;
  std::string hdfFileName;
  //
  std::string appDataLocation;
  std::string tempLocation;
  std::string configLocation;
  //
  secure_string grox_password;
  secure_string randomBytes;
  //
  bool extra_debug = false;
  //
  std::vector<std::shared_ptr<abstract_exchange>> networks_;
  //
  // std::shared_ptr<ads::CDockManager> dock_manager_;
  ads::CDockManager* dock_manager_{nullptr};
  QMenu* dockwindows_menu_{nullptr};
  //
  std::shared_ptr<abstract_dataset_manager> data_manager_;
  //
  QNetworkAccessManager* networkmanager_{nullptr};
};

inline app_settings global_settings;
