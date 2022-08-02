#include "query_tempo.h"

#include "nlohmann/json.hpp"

#include <curl/curl.h>

using json = nlohmann::json;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string http_get(std::string url) {
    CURL * curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        return readBuffer;
    }

    std::cout << "YO HTTP GET FAILURE " << url << std::endl;
    exit(1);
    return "";
}

json convert_search_result_to_json(std::string data) {
    json obj = json::parse(data);
    return obj;
}

/**
 * Call this function for 3 seconds interval
 * */
std::vector<std::string> get_traces_by_structure_and_interval(trace_structure query_trace, int start_time, int end_time) {
    if (end_time-start_time > 3) {
        std::cerr << "get_traces_by_structure_and_interval should \
            only be called for atmost 3 seconds interval" << std::endl;  
    }

    /**
     * 1. Get all trace ids of the period
     * 2. For each trace id, check if the trace conforms to the query_trace stucture using isomorphism
     * 2.a. fetch trace
     * 2.b. convert to trace_structure object
     * 3.c. apply boost v2 subisomorphims
     * */
    std::string url = std::string(TEMPO_IP) + std::string(TEMPO_SEARCH) ; // + "?start=" + \
        // std::to_string(start_time) + "&end=" + std::to_string(end_time) + "&limit=50000";
    
    std::cout << url << std::endl;
    auto raw_response = http_get(url);
    auto response = convert_search_result_to_json(raw_response);
    if (response["traces"] == NULL) {
        std::cout << "No traces in interval [" << start_time << ", " << end_time << "]!" << endl;
        return {};
    }
    for (auto ele : response["traces"]) {
        // convert it to trace_structure
        // check for isomorphism
    }
    return {};
}

int main() {
    trace_structure query_trace;
    query_trace.num_nodes = 3;
    query_trace.node_names.insert(std::make_pair(0, "frontend"));
    query_trace.node_names.insert(std::make_pair(1, "adservice"));
    query_trace.node_names.insert(std::make_pair(2, ASTERISK_SERVICE));

    query_trace.edges.insert(std::make_pair(0, 1));
    query_trace.edges.insert(std::make_pair(1, 2));

    auto res = get_traces_by_structure_and_interval(query_trace, 1659468533, 1659468534);
    return 0;
}