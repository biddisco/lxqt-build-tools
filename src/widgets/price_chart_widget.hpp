#pragma once

#include <QAbstractTableModel>
#include <QComboBox>
#include <QPushButton>
#include <QTableView>
#include <QWidget>

//
#include "exchange/exchange.hpp"
#include "indicators/indicator_definitions.hpp"
#include "indicators/indicator_types.hpp"
#include "plot/indicator_plot.hpp"
#include "plot/ohlc_picker.hpp"
#include "plot/ohlc_price_plot.hpp"

class ohlc_dataset_view;

namespace Ui {
  class price_chart_widget;
}

/// indicator_ptr - contains a shared_ptr to a vtable which invokes the indicator API
struct indicator_ptr
{
  // ----------------------------------------------------------------------------
  /// invokes indicator_API - vtable of functions
  struct indicator_API_vtable
  {
    virtual ~indicator_API_vtable() = default;
    // access the base pointer
    virtual indicators::indicator_base* ptr() = 0;
    // access the specialized operator overloads
    virtual void call_operator(std::uint64_t) = 0;
  };

  // ----------------------------------------------------------------------------
  /// indicator_API_binding - templated binding of type to parameter API
  template <typename Algorithm>
  struct indicator_API_binding : indicator_API_vtable
  {
    indicator_API_binding(Algorithm const& x)
      : alg_(x)
    {
    }

    // ----------------------------------------------------------------------------
    indicators::indicator_base* ptr() override { return &alg_; }

    // ----------------------------------------------------------------------------
    void call_operator(std::uint64_t N) override { call_operator_impl(N); }

    // ----------------------------------------------------------------------------
    /// The algorithm might not return a single value, so we provide
    /// overloads that can handle vectors of values
    template <typename T = Algorithm,
        typename std::enable_if_t<std::is_same<typename T::result_type, double>::value, bool>
            Enable = false>
    void call_operator_impl(std::uint64_t N)
    {
      auto const input = alg_.get_input_data()[0];
      auto output = alg_.get_output_datasets()[0];
      //
      if (N == 0)
      {
        for (auto const& ohlc : input->data())
        {
          auto vals = alg_.operator()(ohlc);
          QPointF xyval(ohlc.time, vals);
          output->data().push_back(xyval);
        }
      }
    }

    // ----------------------------------------------------------------------------
    template <typename T = Algorithm,
        typename std::enable_if_t<std::is_same<typename T::result_type, std::vector<float>>::value,
            bool>
            Enable = false>
    void call_operator_impl(std::uint64_t N)
    {
      auto const input = alg_.get_input_data()[0];
      auto outputs = alg_.get_output_datasets();
      //
      if (N == 0)
      {
        for (auto const& ohlc : input->data())
        {
          auto vals = alg_.operator()(ohlc);
          for (int i = 0; i < alg_.num_outputs(); ++i)
          {
            QPointF xyval(ohlc.time, vals[i]);
            outputs[i]->data().push_back(xyval);
          }
        }
      }
    }

    Algorithm alg_;
  };

  // ----------------------------------------------------------------------------
  /// constructor - creates the internal vtable enabled object
  template <typename Algorithm>
  indicator_ptr(Algorithm const& alg, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc_)
  {
    // this is backwards - the shared pointer holds the API binding instead of the alg
    binding = std::make_shared<indicator_API_binding<Algorithm>>(alg);
    std::shared_ptr<Algorithm> temp = alg.create(alg, hdf5_ohlc_);
    // copy through and then let the other go
    std::dynamic_pointer_cast<indicator_API_binding<Algorithm>>(binding)->alg_ = *temp;
  }

  // ----------------------------------------------------------------------------
  void call_operator(std::uint64_t N) { binding->call_operator(N); }

  // ----------------------------------------------------------------------------
  indicators::indicator_base* ptr() const { return binding->ptr(); }

  // ----------------------------------------------------------------------------
  std::shared_ptr<indicator_API_vtable> binding;
  indicator_plot* plot{nullptr};
  std::vector<timebased_data_curve*> curves;
};

// ----------------------------------------------------------------------------
class indicators_model : public QAbstractTableModel
{
  Q_OBJECT
  public:
  explicit indicators_model(QObject* parent = nullptr);

  int rowCount(QModelIndex const& parent = QModelIndex()) const override;
  int columnCount(QModelIndex const& parent = QModelIndex()) const override;
  QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
  //
  void dataAdded();
  //    QModelIndex index(int row, int column,
  //                              const QModelIndex &parent = QModelIndex()) const = 0;
  //    QModelIndex parent(const QModelIndex &child) const = 0;

  //    QModelIndex sibling(int row, int column, const QModelIndex &idx) const;
  //    int rowCount(const QModelIndex &parent = QModelIndex()) const = 0;
  //    int columnCount(const QModelIndex &parent = QModelIndex()) const = 0;
  //    bool hasChildren(const QModelIndex &parent = QModelIndex()) const;

  //    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const = 0;

  std::vector<indicator_ptr> indicators_;
};

// ----------------------------------------------------------------------------
class price_chart_widget : public QWidget
{
  Q_OBJECT

  private:
  Ui::price_chart_widget* ui;
  //
  ohlc_price_plot* price_plot_;
  std::vector<indicator_plot*> filter_plots_;
  indicator_plot* assets_plot_;
  QPushButton* btn_indicator_;
  //
  std::shared_ptr<ohlc_dataset_view> hdf5_ohlc_;
  std::shared_ptr<exchange> exchange_;
  std::string ticker_string_;

  indicators_model ind_model_;
  QTableView* ind_vis_;

  public:
  price_chart_widget(
      QWidget*, std::shared_ptr<ohlc_dataset_view>, std::shared_ptr<exchange> ex, currency_pair cp);
  ~price_chart_widget();

  void connect_gui();
  void graph_rescale(int range);

  void update_live_data(ohlctv_sample const& new_sample)
  {
    price_plot_->update_live_data(new_sample);
  }
  void replot() { price_plot_->replot(); }

  void show_plot_axes();

  std::tuple<indicator_plot*, timebased_data_curve*> add_indicator_plot(QString const& title,
      point_chart_data* data, QColor const& color, indicators::y_limits ylimits = {0.0, 0.0});

  void remove_indicator_plot(indicator_plot* filter_plot, timebased_data_curve* curve);

  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
};
