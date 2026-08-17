#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>

#include "exchange/xrpl_network.hpp"

// ----------------------------------------------------------------------------
class xrpl_settings_dialog : public QDialog
{
  Q_OBJECT

  public:
  explicit xrpl_settings_dialog(QWidget* parent, xrpl_network* network);

  private:
  void populate_list();
  void update_ui();
  void apply_edits_to_row(int row);
  void apply_edits_to_current();
  void refresh_active_marker();

  private slots:
  void on_selection_changed(int current_row);
  void on_add();
  void on_remove();
  void on_set_active();
  void accept() override;

  private:
  xrpl_network* network_;
  std::vector<xrpl_network::server_config> servers_;
  std::size_t selected_ = 0;
  int last_row_ = -1;

  QListWidget* list_;
  QLineEdit* name_edit_;
  QLineEdit* ws_host_edit_;
  QSpinBox* ws_port_spin_;
  QLineEdit* rpc_host_edit_;
  QSpinBox* rpc_port_spin_;
};
