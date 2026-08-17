#include "widgets/xrpl_settings_dialog.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include "debug/logging.hpp"

namespace {
  static auto settings_log = grox::log::create("XRPL-sets");

  QString item_label(xrpl_network::server_config const& cfg, bool active)
  {
    QString label = QString::fromStdString(cfg.name);
    if (active) { label += "  (active)"; }
    return label;
  }
}    // namespace

// ----------------------------------------------------------------------------
xrpl_settings_dialog::xrpl_settings_dialog(QWidget* parent, xrpl_network* network)
  : QDialog(parent)
  , network_(network)
  , servers_(network->server_list())
  , selected_(network->selected_server_index())
{
  setWindowTitle(QString("%1 Server Settings").arg(QString::fromStdString(network_->get_name())));
  auto* main_layout = new QVBoxLayout(this);

  main_layout->addWidget(new QLabel("Configured XRPL servers. The active server is used for "
                                    "JSON-RPC and websocket connections."));

  auto* hlayout = new QHBoxLayout();
  list_ = new QListWidget();
  list_->setMaximumWidth(300);
  hlayout->addWidget(list_);

  auto* form = new QFormLayout();
  name_edit_ = new QLineEdit();
  ws_host_edit_ = new QLineEdit();
  ws_port_spin_ = new QSpinBox();
  ws_port_spin_->setRange(1, 65535);
  rpc_host_edit_ = new QLineEdit();
  rpc_port_spin_ = new QSpinBox();
  rpc_port_spin_->setRange(1, 65535);

  form->addRow("Name:", name_edit_);
  form->addRow("Websocket host:", ws_host_edit_);
  form->addRow("Websocket port:", ws_port_spin_);
  form->addRow("RPC host:", rpc_host_edit_);
  form->addRow("RPC port:", rpc_port_spin_);
  hlayout->addLayout(form);
  main_layout->addLayout(hlayout);

  auto* buttons = new QHBoxLayout();
  auto* add_btn = new QPushButton("Add");
  auto* remove_btn = new QPushButton("Delete");
  auto* active_btn = new QPushButton("Set active");
  buttons->addWidget(add_btn);
  buttons->addWidget(remove_btn);
  buttons->addWidget(active_btn);
  buttons->addStretch();
  main_layout->addLayout(buttons);

  auto* dialog_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  main_layout->addWidget(dialog_buttons);

  // Populate the list and select the active item *before* wiring up the
  // currentRowChanged signal, so that the initial selection does not
  // trigger apply_edits_to_current() with empty editor contents.
  populate_list();
  if (!servers_.empty())
  {
    list_->setCurrentRow(static_cast<int>(selected_));
    last_row_ = static_cast<int>(selected_);
  }
  update_ui();

  connect(
      list_, &QListWidget::currentRowChanged, this, &xrpl_settings_dialog::on_selection_changed);
  connect(add_btn, &QPushButton::clicked, this, &xrpl_settings_dialog::on_add);
  connect(remove_btn, &QPushButton::clicked, this, &xrpl_settings_dialog::on_remove);
  connect(active_btn, &QPushButton::clicked, this, &xrpl_settings_dialog::on_set_active);
  connect(dialog_buttons, &QDialogButtonBox::accepted, this, &xrpl_settings_dialog::accept);
  connect(dialog_buttons, &QDialogButtonBox::rejected, this, &xrpl_settings_dialog::reject);
}

// ----------------------------------------------------------------------------
void xrpl_settings_dialog::populate_list()
{
  list_->clear();
  for (std::size_t i = 0; i < servers_.size(); ++i)
  {
    list_->addItem(item_label(servers_[i], i == selected_));
  }
}

// ----------------------------------------------------------------------------
void xrpl_settings_dialog::update_ui()
{
  int row = list_->currentRow();
  bool const have_selection = row >= 0 && static_cast<std::size_t>(row) < servers_.size();

  if (have_selection)
  {
    auto const& cfg = servers_[row];
    name_edit_->setText(QString::fromStdString(cfg.name));
    ws_host_edit_->setText(QString::fromStdString(cfg.ws_host));
    ws_port_spin_->setValue(cfg.ws_port);
    rpc_host_edit_->setText(QString::fromStdString(cfg.rpc_host));
    rpc_port_spin_->setValue(cfg.rpc_port);
  }
  else
  {
    name_edit_->clear();
    ws_host_edit_->clear();
    ws_port_spin_->setValue(443);
    rpc_host_edit_->clear();
    rpc_port_spin_->setValue(443);
  }
}

// ----------------------------------------------------------------------------
void xrpl_settings_dialog::apply_edits_to_row(int row)
{
  if (row < 0 || static_cast<std::size_t>(row) >= servers_.size()) { return; }
  auto& cfg = servers_[row];
  cfg.name = name_edit_->text().toStdString();
  cfg.ws_host = ws_host_edit_->text().toStdString();
  cfg.ws_port = ws_port_spin_->value();
  cfg.rpc_host = rpc_host_edit_->text().toStdString();
  cfg.rpc_port = rpc_port_spin_->value();

  if (auto* item = list_->item(row))
  {
    item->setText(item_label(cfg, row == static_cast<int>(selected_)));
  }
}

// ----------------------------------------------------------------------------
void xrpl_settings_dialog::apply_edits_to_current() { apply_edits_to_row(list_->currentRow()); }

// ----------------------------------------------------------------------------
void xrpl_settings_dialog::refresh_active_marker()
{
  for (std::size_t i = 0; i < servers_.size(); ++i)
  {
    if (auto* item = list_->item(static_cast<int>(i)))
    {
      item->setText(item_label(servers_[i], i == selected_));
    }
  }
}

// ----------------------------------------------------------------------------
void xrpl_settings_dialog::on_selection_changed(int current_row)
{
  // Save any edits made to the previously selected row, then switch.
  apply_edits_to_row(last_row_);
  last_row_ = current_row;
  update_ui();
}

// ----------------------------------------------------------------------------
void xrpl_settings_dialog::on_add()
{
  apply_edits_to_current();
  xrpl_network::server_config cfg{"New server", "", 443, "", 443};
  servers_.push_back(cfg);
  list_->addItem(item_label(cfg, false));
  list_->setCurrentRow(static_cast<int>(servers_.size()) - 1);
}

// ----------------------------------------------------------------------------
void xrpl_settings_dialog::on_remove()
{
  int row = list_->currentRow();
  if (row < 0 || static_cast<std::size_t>(row) >= servers_.size()) { return; }
  servers_.erase(servers_.begin() + row);
  delete list_->takeItem(row);
  if (selected_ >= servers_.size() && selected_ > 0) { --selected_; }
  if (!servers_.empty())
  {
    list_->setCurrentRow(std::min(row, static_cast<int>(servers_.size()) - 1));
  }
}

// ----------------------------------------------------------------------------
void xrpl_settings_dialog::on_set_active()
{
  int row = list_->currentRow();
  if (row < 0 || static_cast<std::size_t>(row) >= servers_.size()) { return; }
  apply_edits_to_current();

  bool const server_changed = static_cast<std::size_t>(row) != selected_ ||
      servers_[row].ws_host != network_->websocket_address() ||
      servers_[row].ws_port != network_->websocket_port() ||
      servers_[row].rpc_host != network_->jsonrpc_address() ||
      servers_[row].rpc_port != network_->jsonrpc_port();

  selected_ = static_cast<std::size_t>(row);
  refresh_active_marker();
  network_->set_server_list(servers_, selected_);

  if (server_changed) { network_->reconnect(); }
}

// ----------------------------------------------------------------------------
void xrpl_settings_dialog::accept()
{
  apply_edits_to_current();
  if (servers_.empty())
  {
    GROX_LOG_ERROR(settings_log, "{:>20}", "Cannot save empty XRPL server list");
    return;
  }
  if (selected_ >= servers_.size()) { selected_ = 0; }

  bool const server_changed = servers_[selected_].ws_host != network_->websocket_address() ||
      servers_[selected_].ws_port != network_->websocket_port() ||
      servers_[selected_].rpc_host != network_->jsonrpc_address() ||
      servers_[selected_].rpc_port != network_->jsonrpc_port();

  network_->set_server_list(servers_, selected_);
  if (server_changed) { network_->reconnect(); }
  QDialog::accept();
}
