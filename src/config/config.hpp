#pragma once

#include <string>
#include <vector>
//
#include <QApplication>
#include <QMenu>
#include <QString>

// @TODO: Put back string with cleanup operation
using secure_string = std::string;

namespace ads {
  class CDockManager;
}

class exchange;
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
  QString configLocation;
  //
  secure_string grox_password;
  secure_string randomBytes;
  //
  std::vector<std::shared_ptr<exchange>> networks_;
  //
  std::shared_ptr<ads::CDockManager> dock_manager_;
  QMenu* dockwindows_menu_;
  //
  std::shared_ptr<abstract_dataset_manager> data_manager_;
  //
  QNetworkAccessManager* networkmanager_;
};

inline app_settings global_settings;
