#include <pika/execution.hpp>
#include <pika/future.hpp>
#include <pika/init.hpp>
#include <pika/program_options.hpp>
//
#include <range/v3/algorithm/starts_with.hpp>
//
#include <ripple/protocol/Seed.h>
#include <ripple/protocol/KeyType.h>
#include <ripple/protocol/SecretKey.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/protocol/jss.h>
//
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
//
// Boost Accumulators
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
//
namespace ba = boost::accumulators;
namespace bt = ba::tag;
using rolling_mean = ba::accumulator_set<double, ba::stats<bt::rolling_mean>>;
rolling_mean computation_rate_(ba::tag::rolling_window::window_size = 10);

typedef pika::lcos::local::spinlock  mutex_type;
typedef std::lock_guard<mutex_type> scoped_lock;
//
using namespace std::chrono;
//
//
mutex_type                 output_mutex;
std::atomic<std::size_t>   keys_tested;
std::size_t                reference_keys;
high_resolution_clock::time_point start_time, reference_time;
std::atomic<bool> abort_job{false};
//
std::vector<std::string> searches;
std::vector<std::string> r_searches;
//
using namespace std::string_view_literals;
constexpr std::string_view RippleAlphabet =
    "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz"sv;
std::string out_filename;

//-----------------------------------------------------------------------------
bool is_bad_char(const char c) {
    return (std::find(RippleAlphabet.begin(), RippleAlphabet.end(), c) == RippleAlphabet.end());
}

//-----------------------------------------------------------------------------
std::vector<std::string> all_copies_using_ripple_alphabet(const std::vector<std::string> &words)
{
    std::vector<std::string> result;
    //
    for (auto const &word :words) {
        std::vector<std::string> intermediate;
        std::string word_l = word;
        std::string word_u = word;
        std::string word_f = word;
        // convert to lower case
        std::transform(word_l.begin(), word_l.end(), word_l.begin(),
            [](unsigned char c){ return std::tolower(c); });
        // convert to upper case
        std::transform(word_u.begin(), word_u.end(), word_u.begin(),
            [](unsigned char c){ return std::toupper(c); });
        //
        unsigned int s = word.size();
        unsigned int permutations = (1 << s);
        // now count from zero to power(2,length of string)
        // replace 0 with lower, 1 with upper case parts of word
        for (unsigned int i=0; i<permutations; ++i) {
            for (unsigned int c=0; c<s; ++c) {
                unsigned int mask = 1 << c;
                unsigned int index = s-1-c;
                word_f[index] = ((i & mask)!=mask) ? word_l[index] : word_u[index];
            }
            bool ok = (std::find_if(word_f.begin(), word_f.end(), is_bad_char) == word_f.end());
            if (ok) {
                intermediate.push_back(word_f);
                result.push_back(word_f);
            }
        }
        std::stringstream tempstr;
        tempstr << "Word: " << std::setw(12) << word << " " << std::setw(5) << intermediate.size() << " : ";
        std::copy(intermediate.begin(), intermediate.end(), std::ostream_iterator<std::string>(tempstr, ", "));
        std::cout << tempstr.str().substr(0,150);
        if (tempstr.str().size()>150) std::cout << "...";
        std::cout << std::endl;
    }
    std::cout << std::endl;
    return result;
}

//-----------------------------------------------------------------------------
std::size_t findkey(std::size_t iterations)
{
    using namespace ranges;
    using namespace ripple;

    auto filewrite_func = [&](const Seed& newSeed, std::string_view pub_str){
        auto const sec_str = toBase58(newSeed);
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        scoped_lock lock(output_mutex);
        std::ofstream outfile(out_filename, std::ios_base::app);
        std::cout << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S] ");
        std::cout << pub_str << " => " << sec_str << std::endl;
        outfile << pub_str << " => " << sec_str << std::endl;
        outfile.flush();
    };

    for (std::size_t i=0; i<iterations; ++i) {
        Seed newSeed = randomSeed();
        auto const newpublicKey = generateKeyPair(KeyType::secp256k1, newSeed).first;
        auto const pub_str = toBase58(calcAccountID(newpublicKey));


        // words not starting with 'R' are searched one letter offset from the string
        for (auto& search : searches) {
            if (starts_with(begin(pub_str)+1, end(pub_str), begin(search), end(search))) {
                filewrite_func(newSeed, pub_str);
                break;
            }
        }
        // words starting with 'R' can be searched from the start of the string
        for (auto& search : r_searches) {
            if (starts_with(begin(pub_str), end(pub_str), begin(search), end(search))) {
                filewrite_func(newSeed, pub_str);
                break;
            }
        }
    }
    return iterations;
}

//-----------------------------------------------------------------------------
void vg_output_timing_console(double rate, unsigned long long total, double elapsed)
{
    char linebuf[80];
    double targ = rate;
    char const *unit = "key/s";
    if (targ > 1000) {
        unit = "Kkey/s";
        targ /= 1000.0;
        if (targ > 1000) {
            unit = "Mkey/s";
            targ /= 1000.0;
        }
    }

    size_t rem = sizeof(linebuf);
    size_t p = snprintf(linebuf, rem, "[%.2f %s] [keys %'14lld / secs %8.1f]", targ, unit, total, elapsed);

    rem -= p;
    if (rem < 0)
        rem = 0;

    if (rem) {
        memset(&linebuf[sizeof(linebuf) - rem], 0x20, rem);
        linebuf[sizeof(linebuf) - 1] = '\0';
    }

    std::cout << linebuf << "\r" << std::flush;
}


//-----------------------------------------------------------------------------
void update_count(std::size_t n)
{
    keys_tested += n;
    //
    std::unique_lock lock(output_mutex, std::try_to_lock_t{});

    // don't print anything out if we are exiting of didn't get the lock
    if (abort_job || !lock.owns_lock()) return;

    auto now = high_resolution_clock::now();
    double secs = std::chrono::duration_cast<duration<double>>(now - reference_time).count();
    if (secs>1) {
        double keys = keys_tested - reference_keys;
        double rate = keys/secs;
        reference_time = now;
        reference_keys = keys_tested;

        // insert data into boost accumulator
        computation_rate_(rate);
        auto rt = boost::accumulators::rolling_mean(computation_rate_);
        //
        double elapsed = std::chrono::duration_cast<duration<double>>(now - start_time).count();
        vg_output_timing_console(rt, keys_tested, elapsed);
    }
}

//-----------------------------------------------------------------------------
pika::future<std::size_t> calculate(std::size_t iterations)
{
    return pika::async(findkey, iterations).then(
        [&](pika::future<std::size_t> &&f) {
            std::size_t n = f.get();
            update_count(n);
            return n;
    }).then([=](pika::future<std::size_t> &&f){
        if (!abort_job) {
            // run another set of iterations
            return calculate(iterations);
        }
        scoped_lock lock(output_mutex);
        std::cout << "Worker thread aborting " << pika::get_worker_thread_num() << std::endl;
        return pika::make_ready_future<std::size_t>(0);
    });
}

//-----------------------------------------------------------------------------
int pika_main(pika::program_options::variables_map& vm)
{
    std::size_t iterations = 1000;
    //
    std::vector<std::string> prefixes;
    if (!vm["prefixes"].empty()) {
        prefixes = vm["prefixes"].as<std::vector<std::string> >();
    }
    if (vm.count("frequency")) {
        iterations = vm["frequency"].as<std::size_t>();
    }
    if (prefixes.size()==0) {
        return 1;
    }

    // Get Parameters
    std::size_t  nthreads = pika::get_num_worker_threads();
    std::cout << "Threads  : " << nthreads << "\n" << std::endl;

    searches = all_copies_using_ripple_alphabet(prefixes);

    // move all words starting with "R' into separate list
    constexpr std::string_view r_string = "r"sv;
    constexpr std::string_view R_string = "R"sv;
    for (auto& search : searches) {
        if (ranges::starts_with(begin(search), end(search), begin(r_string), end(r_string))
            || ranges::starts_with(begin(search), end(search), begin(R_string), end(R_string))) {
            r_searches.push_back(search);
        }
    }
    searches.erase(std::remove_if(begin(searches), end(searches), [&](auto &s){
        return (ranges::starts_with(begin(s), end(s), begin(r_string), end(r_string))
            || ranges::starts_with(begin(s), end(s), begin(R_string), end(R_string)));
        }
    ), end(searches));

    // Launch Tasks
    std::vector<pika::future<void>> workers;
    workers.reserve(nthreads);

    start_time = reference_time = high_resolution_clock::now();
    keys_tested = 0;
    for (size_t i = 0; i < nthreads; i++) {
        auto fut = calculate(iterations);
        workers.push_back(std::move(fut));
    }
    pika::when_all(workers).get();

    scoped_lock lock(output_mutex);
    std::cout << "Main thread completing" << std::endl;
    return pika::finalize();
}

//-----------------------------------------------------------------------------
void turn_off_cursor() {
    printf("\e[?25l");
}

//-----------------------------------------------------------------------------
void turn_on_cursor() {
    printf("\e[?25h");
}

//-----------------------------------------------------------------------------
// signal handling function for ctrl-\ and ctrl-c
void sig_handler(int /*signo*/)
{
    turn_on_cursor();
    std::cout << "Aborting job" << std::endl;
    abort_job = true;
}

//-----------------------------------------------------------------------------
// the normal int main function that is called at startup and runs on an OS thread
// the user must call pika::init to start the pika runtime which will execute pika_main
// on an pika thread
int main(int argc, char* argv[])
{
    // install signal handler
    if (signal(SIGINT, sig_handler) == SIG_ERR)
        std::cout << "SIGINT handler not installed" << std::endl;
    if (signal(SIGQUIT, sig_handler) == SIG_ERR)
        std::cout << "SIGQUIT handler not installed" << std::endl;

    // to print numbers with commas for thousands etc, create a locale
    setlocale(LC_NUMERIC, "");
    struct lconv *ptrLocale = localeconv();
    ptrLocale->thousands_sep = (char*)"'";
    turn_off_cursor();

    pika::program_options::options_description cmdline("Options");
    cmdline.add_options()
      ("frequency,f",
          pika::program_options::value<std::size_t>()->default_value(1),
          "number of key iterations to do before outputting info")
      ("prefixes,p",
          pika::program_options::value<std::vector<std::string>>()->multitoken(),
          "list of prefixes to search for")
      ;

    // generate output filename for this run
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::stringstream filename;
    filename << std::filesystem::path(getenv("HOME")).c_str() << "/.ssh/.wallets-" << std::put_time(&tm, "%Y-%m-%d-%H-%M-%S");
    std::filesystem::path p = filename.str();
    out_filename = std::filesystem::absolute(p);

    std::cout << "example command line :\n"
              << "./vanity --prefixes johnb jbjnr johnnyb biddi biddisco olga olgy olgypops olgab sasha mila milena grox -f 100000 --pika:threads=cores \n\n"
              << "example cleanup line :\n"
              << "wc -l ~/.ssh/.wallets.txt && cat ~/.ssh/.wallets* | sortci | uniq >> temp.wallets && mv temp.wallets ~/.ssh/.wallets.txt && wc -l ~/.ssh/.wallets.txt \n\n"
              << "Alphabet : " << RippleAlphabet << "\n"
              << "Output   : " << out_filename << std::endl;

    // We force this test to use several threads by default.
    std::vector<std::string> const cfg = {"pika.os_threads=cores"};

    // Initialize and run pika
    pika::init_params init_args;
    init_args.desc_cmdline = cmdline;
    init_args.cfg = cfg;

    auto result = pika::init(pika_main, argc, argv, init_args);
    turn_on_cursor();
    return result;
}

