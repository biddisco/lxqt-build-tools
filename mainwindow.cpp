#include <QApplication>
#include <QDebug>
#include <QAction>
#include <QKeySequence>
#include <QShortcut>
#include <QDateTime>
//
#include <filesystem>
//
#include <boost/iterator/zip_iterator.hpp>
//
#include "mainwindow.hpp"
#include "ui_mainwindow.h"
//
#include "bitstamp/ohlc.h"
#include "settings.hpp"
//
#include "hdf5.h"

// ----------------------------------------------------------------------------
GroxMainWindow::GroxMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::GroxMainWindow)
{
    repeat_ohlc_ = false;
    //
    // Build GUI from designer generated widgets/controls
    //
    ui->setupUi(this);

    //
    // Create candlestick/volume plots
    //
    stackedStockCharts_ = new StackedStockCharts(this);
    priceAndPatternPlot_ = stackedStockCharts_->priceAndPatternPlot();
    ui->candlestick_layout->addWidget(stackedStockCharts_,30);

    //
    // Create orderbook plo
    //
    OrderBookPlot_ = new OrderBookPlot();
    ui->orderbook_layout->addWidget(OrderBookPlot_,30);

    //
    // setup Qt actions/connections
    //
    createActions();
    createMenus();

    read_hdf5();
    //
    // just an experiment to display an image
    //
    // scale pixmap to fit in label'size and keep ratio of pixmap
    QPixmap pix(":/images/xrp.jpg");
    pix = pix.scaled(ui->image_label->size(), Qt::KeepAspectRatio);
    ui->image_label->setPixmap(pix);
}

// ----------------------------------------------------------------------------
GroxMainWindow::~GroxMainWindow()
{
    delete ui;
    delete OrderBookPlot_;
    delete stackedStockCharts_;
}

// ----------------------------------------------------------------------------
void GroxMainWindow::appExitCleanupHandler()
{
    qDebug() << "Main Window: appExitCleanupHandler()";
    // call clean up handlers of any components/widgets
    // block here to prevent access of temp buffers that are deleted
    // by the program/qt/etc
    qDebug() << "websockets: shutdown start";
    if (ws_trades) ws_trades->shutdown_blocking();
    if (ws_bidask) ws_bidask->shutdown_blocking();
    qDebug() << "websockets: shutdown complete";
}

// ----------------------------------------------------------------------------
void GroxMainWindow::createActions()
{
    actionQuit = ui->menubar->addAction(tr("Quit"));
    actionQuit->setMenuRole(QAction::QuitRole);
    actionQuit->setShortcut(QKeySequence::Quit);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::createMenus()
{
    connect(actionQuit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->connect_button, SIGNAL(clicked()), this, SLOT(start_websocket()));
    connect(this, SIGNAL(new_ticker_data_ui(QString)), ui->json_text_1, SLOT(setPlainText(QString)));
    connect(this, SIGNAL(new_order_data_ui(QString)), ui->json_text_3, SLOT(setPlainText(QString)));
    connect(this, SIGNAL(new_order_data_replot()), OrderBookPlot_, SLOT(replot()));
    connect(this, SIGNAL(new_ohlc_data_ui()), this, SLOT(new_ohlc_data()));
}

// ----------------------------------------------------------------------------
void GroxMainWindow::new_ticker_data(GroxMainWindow *mw, std::string &&data)
{
    // std::cout << "\n\nReceived " << data << std::endl;

    if (data.rfind("{\"data\":", 0) != 0) {
      return;
    }
    nlohmann::json jdata = json::parse(data);
    // extract the main subgroup
    jdata = jdata["data"];
    // std::cout << jdata.dump(4) << std::endl;

    live_trades json_trades = jdata.get<live_trades>();;

    // convert json data to trade structs
//    live_trades_string json_strings;
//    // convert json to vector of structs
//    json_strings = jdata.get<live_trades_string>();
//    live_trades json_trade(json_strings);

//    emit candlestickdata(ohlc_vector);

    QString datastring = QString::fromStdString(jdata.dump(4));
    emit mw->new_ticker_data_ui(datastring);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::merge_data(
    const QVector<QwtOHLCSample> &new_ohlc_samples,
    const std::vector<double> &new_ohlc_volumes)
{
    uint64_t update = 0;
    if (ohlc_samples.size()==0) {
        ohlc_samples = new_ohlc_samples;
        ohlc_volumes = new_ohlc_volumes;
    }
    else if (!new_ohlc_samples.empty()){
        auto last_existing = ohlc_samples.back().time;
        auto first_new = new_ohlc_samples.front().time;

        std::cout << "existing " << static_cast<uint64_t>(last_existing)
                  << " new " << static_cast<uint64_t>(first_new) << std::endl;
        if (first_new-last_existing == 60) {
            std::cout << "merging data" << std::endl;
            ohlc_samples.append(new_ohlc_samples);
            ohlc_volumes.insert(ohlc_volumes.end(), new_ohlc_volumes.begin(), new_ohlc_volumes.end());
            if (ohlc_samples.size()!=ohlc_volumes.size()) {
                throw std::runtime_error("Data merge problem");
            }
            update = new_ohlc_samples.size();
        }
        else {
            throw std::runtime_error("Data OHLC time mismatch in merge");
        }
    }
    // write an update to the main datafile
    write_hdf5(ohlc_samples, ohlc_volumes, update);
    emit new_ohlc_data_ui();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::rest_api_data(GroxMainWindow *mw, std::string &&data)
{
    try {
        // convert json data into vectors of actual data
        // std::cout << "\n\nRest API " << data << std::endl << std::endl << std::endl;
        nlohmann::json jdata = json::parse(data)["data"]["ohlc"];
        std::vector<ohlc_string> ohlc_strings = jdata.get<std::vector<ohlc_string>>();;
        //
        QVector<QwtOHLCSample> new_ohlc_samples;
        std::vector<double>    new_ohlc_volumes;
        //
        const auto N = ohlc_strings.size();
        new_ohlc_samples.reserve(N);
        new_ohlc_volumes.reserve(N);
        //
        for (auto o : ohlc_strings) {
            ohlc temp(o);
            new_ohlc_samples.push_back(QwtOHLCSample(temp.timestamp, temp.open, temp.high, temp.low, temp.close));
            new_ohlc_volumes.push_back(temp.volume);
        }
        std::cout << "Received " << ohlc_strings.size() << " new OHLC samples" << std::endl;
        mw->merge_data(new_ohlc_samples, new_ohlc_volumes);

        if (mw->repeat_ohlc_) {
            mw->request_new_candlestick_data(0);
        }
    }
    catch (std::exception &e) {
        std::cout << "JSON error decoding OHLC data: " << e.what() << "\n"
                  << data << std::endl << std::endl << std::endl;
    }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::new_ohlc_data()
{
    if (!ohlc_samples.empty())
        priceAndPatternPlot_->set_OHLC_data(ohlc_samples);
}

// ----------------------------------------------------------------------------
void bid_ask_string_to_number(nlohmann::json &json, double *x, double *y)
{
    auto bid_string = json.get<std::array<std::array<std::string,2>,100>>();
    auto zip_start = boost::make_zip_iterator(boost::make_tuple(x, y));
    std::transform(bid_string.begin(), bid_string.end(), zip_start, [](const auto & i) {
        std::pair<double,double> vals = std::make_pair(
            std::atof(i[0].c_str()), std::atof(i[1].c_str())
        );
        return vals;
    });
}

// ----------------------------------------------------------------------------
void GroxMainWindow::new_order_data(GroxMainWindow *mw, std::string &&data)
{
    // std::cout << "\n\nReceived " << data << std::endl << std::endl << std::endl;
    if (data.rfind("{\"data\":", 0) != 0) {
      return;
    }
    nlohmann::json jdata = json::parse(data)["data"];
    static std::array<double,200> data_x;
    static std::array<double,200> data_y;
    static std::array<double,200> data_z;

    // convert orders into a layout we can visualize nicely
    bid_ask_string_to_number(jdata["bids"], &data_x[0],   &data_y[0]);
    bid_ask_string_to_number(jdata["asks"], &data_x[100], &data_y[100]);
    // partial sum the 100 asks and bids, store in new y axis z array (left/bids part in reverse order)
    std::partial_sum(&data_y[  0], &data_y[100], std::reverse_iterator<double*>(&data_z[100]));
    std::partial_sum(&data_y[100], &data_y[200], &data_z[100]);
    // and flip the x axis for the bids/left side of plot
    std::reverse(&data_x[0], &data_x[100]);
    // push this data intp the graph object
    mw->OrderBookPlot_->plot_curve_->setRawSamples(data_x.begin(), data_z.begin(), 200);
    // pick x min max limits so they don't jump around constantly
    double xrange = data_x[199]-data_x[0];
    double xscale = 0.5*std::pow(10, static_cast<int64_t>(std::log10(xrange)));
    double xmin   = std::round(data_x[0]/xscale)   * xscale;
    double xmax   = std::round(data_x[199]/xscale) * xscale;
    mw->OrderBookPlot_->setAxisScale(QwtPlot::xBottom, xmin, xmax);
    // pick y min max limits so they don't jump around constantly
    double yrange = std::max(data_z[0], data_z[199]);
    double yscale = std::pow(10, static_cast<int64_t>(std::log10(yrange)));
    double ymin   = 0.0;
    double ymax   = (std::round(yrange/yscale))*yscale;
    mw->OrderBookPlot_->setAxisScale(QwtPlot::yLeft, ymin, ymax);

    emit mw->new_order_data_replot();

    QString datastring = QString::fromStdString(jdata.dump(4));
    emit mw->new_order_data_ui(datastring);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::start_websocket()
{
    using namespace std::placeholders;
    ws_trades = net::ws::create_session(
                io_contexts.ioc,
                io_contexts.ctx,
                "ws.bitstamp.net",
                "443",
                "{\"event\": \"bts:subscribe\",\"data\": {\"channel\": \"live_trades_xrpusd\"}}",
                std::bind(GroxMainWindow::new_ticker_data, this, _1));

    ws_bidask = net::ws::create_session(
                io_contexts.ioc,
                io_contexts.ctx,
                "ws.bitstamp.net",
                "443",
                "{\"event\": \"bts:subscribe\",\"data\": {\"channel\": \"order_book_xrpusd\"}}",
                std::bind(GroxMainWindow::new_order_data, this, _1));

    https_rest = net::https::create_session(
                io_contexts.ioc,
                io_contexts.ctx,
                "www.bitstamp.net",
                "443",
                std::bind(GroxMainWindow::rest_api_data, this, _1));

    // Run the I/O service on a thread.
    std::thread websocket_thread([&]()
        {
            // The call will return when the socket is closed.
            io_contexts.ioc.run();
        }
    );
    websocket_thread.detach();

    // wait for https connection to finish setting up
    while(!https_rest->ready_) { std::this_thread::yield(); }
    //
    request_new_candlestick_data(0);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::request_new_candlestick_data(uint64_t unused)
{
    // what is the last sample we currently have
    uint64_t start_t = 0;
    if (ohlc_samples.size()>0) {
        start_t = static_cast<uint64_t>(ohlc_samples.back().time);
        std::cout << "Data present up until " << start_t << std::endl;
        start_t += 60; // next sample is 60s after last
    }
    //
    QDateTime currentDateTime = QDateTime::currentDateTimeUtc();
    uint64_t unixtime = currentDateTime.toTime_t();
    //
    uint64_t    diff = unixtime - start_t;
    uint64_t samples = diff/60;
    //
    std::string req;
    repeat_ohlc_ = false;
    if (start_t==0) {
        req = "/api/v2/ohlc/xrpusd/?step=60&limit=1000";
    }
    else {
        if (samples>=1000) {
            std::cout << "Limiting request from: " << samples << std::endl;
            samples = 1000;
            repeat_ohlc_ = true;

        }
        std::string start = std::to_string(start_t);
        std::string limit = std::to_string(samples);
        // send a request for ticker data using the io context thread to make the request
        req = "/api/v2/ohlc/xrpusd/?step=60&start=" + start + "&limit=" + limit;
    }
    io_contexts.ioc.post([this, req=std::move(req)]() mutable {
        https_rest->write(std::move(req));
    });
}

// ----------------------------------------------------------------------------
void GroxMainWindow::create_data_dir()
{
    namespace fs = std::filesystem;
    app_settings *app_ini = global_settings();
    if (!fs::exists(app_ini->appDataLocation)) {
        if (!fs::create_directory(app_ini->appDataLocation)) {
            throw std::runtime_error("Failed to create dir " + app_ini->appDataLocation);
        }
    }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::validate_ohlc()
{
    using cit = QVector<QwtOHLCSample>::const_iterator;
    cit li = ohlc_samples.begin();
    bool valid = true;
    uint64_t index = 0;
    for (cit i = ohlc_samples.begin()+1; i!=ohlc_samples.end(); ++i) {
        uint64_t t1 = static_cast<uint64_t>(li->time);
        uint64_t t2 = static_cast<uint64_t>(i->time);
        if (t2 - t1 != 60) {
            std::cout << "Validation error at index " << index << " "
                      << t1 << " and " << t2
                      << "dataseet truncated " << std::endl;
            valid = false;
            break;
        }
        li = i;
        index++;
    }
    ohlc_samples.resize(index+1);
    ohlc_volumes.resize(index+1);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::read_hdf5()
{
    create_data_dir();
    //
    app_settings *app_ini = global_settings();
    //
    if (std::filesystem::exists(app_ini->hdfFileName)) {
        std::cout << "Opening: " << app_ini->hdfFileName << std::endl;
        ohlc_samples.clear();
        ohlc_volumes.clear();
        //
        hid_t file  = H5Fopen(app_ini->hdfFileName.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
        // check if datasets exist
        if (H5Lexists(file, "ohlc", H5P_DEFAULT)>0) {
            // read OHLC data
            hid_t dset1 = H5Dopen(file, "ohlc", H5P_DEFAULT);
            hid_t space1 = H5Dget_space(dset1);
            const int ndims1 = H5Sget_simple_extent_ndims(space1);
            hsize_t dims1[ndims1];
            herr_t status = H5Sget_simple_extent_dims(space1, dims1, NULL);
            //
            int N = dims1[0]/(sizeof(QwtOHLCSample)/sizeof(double));
            ohlc_samples.resize(N);
            status = H5Dread(dset1, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, ohlc_samples.data());

            // read Volume data
            hid_t dset2 = H5Dopen(file, "volume", H5P_DEFAULT);
            hid_t space2 = H5Dget_space(dset2);
            const int ndims2 = H5Sget_simple_extent_ndims(space2);
            hsize_t dims2[ndims2];
            status = H5Sget_simple_extent_dims(space2, dims2, NULL);
            if (N!=dims2[0]) {
                throw std::runtime_error("Datasets not same size");
            }
            ohlc_volumes.resize(N);
            status = H5Dread(dset2, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, ohlc_volumes.data());

            // free/close datasets
            status = H5Dclose (dset1);
            status = H5Dclose (dset2);
            // free/close dataspaces
            status = H5Sclose (space1);
            status = H5Sclose (space2);
        }
        // free/close file
        herr_t status = H5Fclose (file);
    }
    else {
        std::cout << "Creating empty: " << app_ini->hdfFileName << std::endl;

        // Create a new file using default properties.
        hid_t file_id = H5Fcreate(app_ini->hdfFileName.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        herr_t status = H5Fclose(file_id);
    }
    emit new_ohlc_data_ui();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::write_hdf5(
        const QVector<QwtOHLCSample> &samples,
        const std::vector<double> &volume,
        const uint64_t update)
{
    // check data before attempting to write to disk
//    if (debug_level>0)
        validate_ohlc();
    //
    app_settings *app_ini = global_settings();
    std::cout << "Opening: " << app_ini->hdfFileName << std::endl;

    // In the OHLC dataset:
    // There are 24*60=1440 60s candles per day, and each candle has 5 {t,o,h,l,c} entries,
    // so the chunking size ought to be around 1440 * 5 = 7200 elements minimum,
    // a week's data will be 50400 doubles, so a nice binary number size for
    // chunking dimensions will be 65536
    //
    // The volume dataset has only 1 double per entry so we'll use a chunk dimension
    // 4 times less, of 16384
    //
    // Use unlimited size so that the data can be extended arbitrarily

    const uint64_t ohlc_size = sizeof(QwtOHLCSample)/sizeof(double);
    const uint64_t N = samples.size() * ohlc_size;
    hsize_t ohlc_dims[1]  = {N};
    hsize_t vol_dims[1]   = {volume.size()};
    hsize_t max_dims[1]   = {H5S_UNLIMITED};
    hsize_t chunk_dim1[1] = {65536};
    hsize_t chunk_dim2[1] = {16384};
    herr_t  status;

    // open the file, use UNLIMITED for main dimension so we can extend datasets
    hid_t file    = H5Fopen(app_ini->hdfFileName.c_str(), H5F_ACC_RDWR, H5P_DEFAULT);

    // create datasets if they do not exist already
    if (H5Lexists(file, "ohlc", H5P_DEFAULT)<=0) {
        if (update!=0) {
            throw std::runtime_error("Cannot extend dataset before it exists");
        }
        // create a property list to set the chunking property on our OHLC dataset
        hid_t dprop1  = H5Pcreate(H5P_DATASET_CREATE);
        status        = H5Pset_chunk(dprop1, 1, chunk_dim1);

        // write OHLC data,
        hid_t space1  = H5Screate_simple(1, ohlc_dims, max_dims);
        hid_t dset1   = H5Dcreate(file, "ohlc", H5T_IEEE_F64LE, space1, H5P_DEFAULT, dprop1, H5P_DEFAULT);
        status        = H5Dwrite(dset1, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, samples.data());

        // create a property list to set the chunking property on our volume dataset
        hid_t dprop2  = H5Pcreate(H5P_DATASET_CREATE);
        status        = H5Pset_chunk(dprop2, 1, chunk_dim2);

        // write Volume data
        hid_t space2  = H5Screate_simple(1, vol_dims, max_dims);
        hid_t dset2   = H5Dcreate(file, "volume", H5T_IEEE_F64LE, space2, H5P_DEFAULT, dprop2, H5P_DEFAULT);
        status        = H5Dwrite(dset2, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, volume.data());
        // free/close datasets
        status = H5Dclose (dset1);
        status = H5Dclose (dset2);
        // free/close properties
        status = H5Pclose (dprop1);
        status = H5Pclose (dprop2);
        // free/close dataspaces
        status = H5Sclose (space1);
        status = H5Sclose (space2);
    }

    // if we are extending a dataset
    else if (update>0)
    {
        std::cout << "Extending datasets by: " << update << std::endl;
        uint64_t offset = samples.size() - update;
        hsize_t offset1[1] = {offset * ohlc_size};
        hsize_t    ext1[1] = {update * ohlc_size};
        hsize_t offset2[1] = {offset};
        hsize_t    ext2[1] = {update};

        hid_t dset1   = H5Dopen(file, "ohlc", H5P_DEFAULT);
        // extend dataset to new size
        status        = H5Dextend(dset1, ohlc_dims);
        // Select a hyperslab from the file dataspace
        hid_t fspace1 = H5Dget_space(dset1);
        // select hyperslab in new dataset : start, stride(NULL), count, block(NULL)
        status        = H5Sselect_hyperslab(fspace1, H5S_SELECT_SET, offset1, NULL, ext1, NULL);
        // Define memory space that we write our new data from
        hid_t dspace1 = H5Screate_simple(1, ext1, NULL);
        // Write new data to the hyperslab
        status        = H5Dwrite(dset1, H5T_NATIVE_DOUBLE, dspace1, fspace1, H5P_DEFAULT, &samples[offset]);

        hid_t dset2   = H5Dopen(file, "volume", H5P_DEFAULT);
        // extend dataset to new size
        status        = H5Dextend(dset2, vol_dims);
        // Select a hyperslab from the file dataspace
        hid_t fspace2 = H5Dget_space(dset2);
        // select hyperslab in new dataset : start, stride(NULL), count, block(NULL)
        status        = H5Sselect_hyperslab(fspace2, H5S_SELECT_SET, offset2, NULL, ext2, NULL);
        // Define memory space that we write our new data from
        hid_t dspace2 = H5Screate_simple(1, ext2, NULL);
        // Write new data to the hyperslab
        status        = H5Dwrite(dset2, H5T_NATIVE_DOUBLE, dspace2, fspace2, H5P_DEFAULT, &volume[offset]);

        // free/close datasets
        status = H5Dclose (dset1);
        status = H5Dclose (dset2);
        // free/close dataspaces
        status = H5Sclose (fspace1);
        status = H5Sclose (fspace2);
        status = H5Sclose (dspace1);
        status = H5Sclose (dspace2);
    }
    // free/close file
    status = H5Fclose (file);

    std::cout << "Dataset size: " << ohlc_samples.size() << std::endl;

}
