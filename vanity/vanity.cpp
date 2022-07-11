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

typedef pika::lcos::local::spinlock  mutex_type;
typedef std::lock_guard<mutex_type> scoped_lock;
//
using namespace std::chrono;
//
//
mutex_type                 output_mutex;
std::atomic<std::size_t>   keys_tested;
high_resolution_clock::time_point start_time;
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
    result.reserve(1000);
    //
    for (auto const &word :words) {
        std::cout << "Word: " << word << std::endl;
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
                result.push_back(word_f);
            }
        }
    }
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
            bool found = starts_with(begin(pub_str)+1, end(pub_str), begin(search), end(search));
            if (found) {
                filewrite_func(newSeed, pub_str);
            }
        }
        // words starting with 'R' can be searched from the start of the string
        for (auto& search : r_searches) {
            bool found = starts_with(begin(pub_str), end(pub_str), begin(search), end(search));
            if (found) {
                filewrite_func(newSeed, pub_str);
            }
        }
    }
    return iterations;
}

void vg_output_timing_console(double rate, unsigned long long total) {
    double targ;
    char const *unit;
    char linebuf[80];
    size_t rem, p;

    targ = rate;
    unit = "key/s";
    if (targ > 1000) {
        unit = "Kkey/s";
        targ /= 1000.0;
        if (targ > 1000) {
            unit = "Mkey/s";
            targ /= 1000.0;
        }
    }

    rem = sizeof(linebuf);
    p = snprintf(linebuf, rem, "  [%.2f %s][total %lld]",
        targ, unit, total);

    rem -= p;
    if (rem < 0)
        rem = 0;

    if (rem) {
        memset(&linebuf[sizeof(linebuf) - rem], 0x20, rem);
        linebuf[sizeof(linebuf) - 1] = '\0';
    }

    scoped_lock lock(output_mutex);
    std::cout << linebuf << "\r" << std::flush;
}


//-----------------------------------------------------------------------------
void update_count(high_resolution_clock::time_point start_time, std::size_t n)
{
    if (abort_job) return;
    //
    keys_tested     += n;
    //
    double secs = std::chrono::duration_cast<duration<double>>(high_resolution_clock::now() - start_time).count();
    double rate = keys_tested/secs;
    //
    vg_output_timing_console(rate, keys_tested);
}

//-----------------------------------------------------------------------------
pika::future<std::size_t> calculate(std::size_t iterations)
{
    return pika::async(findkey, iterations).then(
        [&](pika::future<std::size_t> &&f) {
            std::size_t n = f.get();
            update_count(start_time, n);
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
    std::uint64_t  nranks = pika::get_num_localities().get();
    std::size_t  nthreads = pika::get_num_worker_threads();

    // Some messages
    std::cout << "xrp-vanity\n";
    std::cout << "Search Threads per locality    : " << nthreads << "\n";
    std::cout << "Total number of search threads : " << nthreads*nranks << "\n\n";

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
    std::cout << "Search list: ";
    std::copy(searches.begin(), searches.end(), std::ostream_iterator<std::string>(std::cout, ", "));
    std::copy(r_searches.begin(), r_searches.end(), std::ostream_iterator<std::string>(std::cout, ", "));
    std::cout << std::endl;

    // Launch Tasks
    std::vector<pika::future<void>> workers;
    workers.reserve(nthreads);

    start_time = high_resolution_clock::now();
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
// signal handling function for ctrl-\ and ctrl-c
void sig_handler(int signo)
{
    if (signo == SIGINT || signo == SIGQUIT) {
        std::cout << "Aborting job" << std::endl;
        abort_job = true;
    }
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

    std::cout << "example command line :\n"
              << "./vanity --prefixes johnb jbjnr johnnyb biddi biddisco olga olgy olgypops olgab sasha mila milena grox -f 100000 --pika:threads=cores \n"
              << "Alphabet : " << RippleAlphabet << "\n"
              << std::endl;

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
    std::cout <<"Output " << out_filename << std::endl;

    // We force this test to use several threads by default.
    std::vector<std::string> const cfg = {"pika.os_threads=cores"};

    // Initialize and run pika
    pika::init_params init_args;
    init_args.desc_cmdline = cmdline;
    init_args.cfg = cfg;

    return pika::init(pika_main, argc, argv, init_args);
}

