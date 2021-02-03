#include <curl/curl.h>
#include <openssl/hmac.h>
#include <uuid/uuid.h>

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <cassert>
//
#include "internet/evp-encrypt.hpp"

using uuid_string_t = char[256];

static size_t write_call_back(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string b2a_hex(char *byte_arr, int n)
{
    const static std::string hex_codes = "0123456789abcdef";
    std::string hex_string;
    for ( int i = 0; i < n ; ++i ) {
        unsigned char bin_value = byte_arr[i];
        hex_string += hex_codes[( bin_value >> 4 ) & 0x0F];
        hex_string += hex_codes[bin_value & 0x0F];
    }
    return hex_string;
}

std::string url_encode(std::string data)
{
    std::string res = data;
    CURL *curl = curl_easy_init();

    if(curl) {
        char *output = curl_easy_escape(curl, data.c_str(), data.length());
        if(output) {
            res = output;
            curl_free(output);
        }
    }

    return res;
}


int main() {

    const std::string api_key    = std::getenv("API_KEY");                                                                                                                                   
    const std::string api_secret = std::getenv("API_SEC");

    secure_string randbytes = "asd23asd234gf576cbv";
    encryption encyptor(api_secret, randbytes);

    std::chrono::milliseconds timestamp = std::chrono::duration_cast< std::chrono::milliseconds >(
            std::chrono::system_clock::now().time_since_epoch()
    );

    uuid_t uuid;
    uuid_string_t nonce;
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, nonce);

    std::string x_auth = "BITSTAMP " + api_key;
    std::string x_auth_nonce = nonce;
    std::string x_auth_timestamp = std::to_string(timestamp.count());
    std::string x_auth_version = "v2";
    std::string content_type = "application/x-www-form-urlencoded";
    std::string payload = url_encode("{offset:1}");

    std::string http_method = "POST";
    std::string url_host = "www.bitstamp.net";
    std::string url_path = "/api/v2/user_transactions/";
    std::string url_query = "";

    std::string data_to_sign = "";
    data_to_sign.append(x_auth);
    data_to_sign.append(http_method);
    data_to_sign.append(url_host);
    data_to_sign.append(url_path);
    data_to_sign.append(url_query);
    data_to_sign.append(content_type);
    data_to_sign.append(x_auth_nonce);
    data_to_sign.append(x_auth_timestamp);
    data_to_sign.append(x_auth_version);
    data_to_sign.append(payload);

    auto signed_hmac = encyptor.CalcHmacSHA256(api_secret, data_to_sign);
    assert(signed_hmac.size()==32);
    std::string x_auth_signature = b2a_hex( signed_hmac.data(), signed_hmac.size() );

    // send request
    CURL *curl;
    CURLcode res;
    std::string read_buffer;

    curl = curl_easy_init();

    if(curl) {

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, ("X-Auth: " + x_auth).c_str());
        headers = curl_slist_append(headers, ("X-Auth-Signature: " + x_auth_signature).c_str());
        headers = curl_slist_append(headers, ("X-Auth-Nonce: " + x_auth_nonce).c_str());
        headers = curl_slist_append(headers, ("X-Auth-Timestamp: " + x_auth_timestamp).c_str());
        headers = curl_slist_append(headers, ("X-Auth-Version: " + x_auth_version).c_str());
        headers = curl_slist_append(headers, ("Content-Type: " + content_type).c_str());

        std::string url = "https://" + url_host + url_path + url_query;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_call_back);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            std::cout << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        std::cout << "curl_easy_perform() response: " << read_buffer << std::endl;

        curl_easy_cleanup(curl);

    }

    return 0;

}
