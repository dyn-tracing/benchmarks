#include "query_es.h"

typedef unsigned char uchar;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static std::string base64_encode(const std::string &in) {

    std::string out;

    int val = 0, valb = -6;
    for (uchar c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val>>valb)&0x3F]);
            valb -= 6;
        }
    }
    if (valb>-6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val<<8)>>(valb+8))&0x3F]);
    while (out.size()%4) out.push_back('=');
    return out;
}

static std::string base64_decode(const std::string &in) {

    std::string out;

    std::vector<int> T(256,-1);
    for (int i=0; i<64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

    int val=0, valb=-8;
    for (uchar c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val>>valb)&0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::string http_get(std::string url, std::string auth_header="", std::string query="") {
    CURL * curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        struct curl_slist* headers = nullptr;
        auto hdr = "Authorization: Basic " + auth_header;
        headers = curl_slist_append(headers, hdr.c_str());

        hdr = "Content-Type: application/json";
        headers = curl_slist_append(headers, hdr.c_str());
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST,1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        long http_code = 0;
        res = curl_easy_perform(curl);
        curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code == 200 && res != CURLE_ABORTED_BY_CALLBACK) {
            curl_easy_cleanup(curl);
            return readBuffer;
        } else {
            curl_easy_cleanup(curl);
            std::cout << "YO HTTP GET FAILURE " << url << std::endl;
            std::cout << "http_code:  " << http_code << std::endl;
            std::cout << "curl_code:  " << res << std::endl;
            exit(1);
        }
    }

    std::cout << "YO HTTP GET FAILURE (no curl instance)" << url << std::endl;
    return "";
}

json convert_search_result_to_json(std::string data) {
    json obj;
    try {
        obj = json::parse(data);
    } catch(const std::exception& e) {
        std::cerr << std::setw(4) << "-- " << data << std::endl;
        std::cerr << e.what() << '\n';
        exit(1);
    }

    return obj;
}

/**
 * @brief Get the isomorphism mappings object
 *
 * Map: query trace => stored trace
 *
 * @param candidate_trace
 * @param query_trace
 * @return std::vector<std::unordered_map<int, int>>
 */
std::vector<std::unordered_map<int, int>> get_isomorphism_mappings (
    trace_structure &candidate_trace, trace_structure &query_trace) {
    graph_type candidate_graph = morph_trace_structure_to_boost_graph_type(candidate_trace);
    graph_type query_graph = morph_trace_structure_to_boost_graph_type(query_trace);

    vertex_comp_t vertex_comp = make_property_map_equivalent_custom(
        boost::get(boost::vertex_name_t::vertex_name, query_graph),
        boost::get(boost::vertex_name_t::vertex_name, candidate_graph));

    std::vector<std::unordered_map<int, int>> isomorphism_maps;

    vf2_callback_custom<graph_type, graph_type, std::vector<std::unordered_map<int, int>>> callback(
        query_graph, candidate_graph, isomorphism_maps);

    boost::vf2_subgraph_iso(
        query_graph,
        candidate_graph,
        callback,
        boost::vertex_order_by_mult(query_graph),
        boost::vertices_equivalent(vertex_comp));

    return isomorphism_maps;
}

graph_type morph_trace_structure_to_boost_graph_type(trace_structure &input_graph) {
    graph_type output_graph;

    for (int i = 0; i < input_graph.num_nodes; i++) {
        boost::add_vertex(vertex_property(input_graph.node_names[i], i), output_graph);
    }

    for (const auto& elem : input_graph.edges) {
        boost::add_edge(elem.first, elem.second, output_graph);
    }

    return output_graph;
}

std::string fetch_trace(std::string trace_id) {
    std::string url = std::string(TEMPO_IP) + INDEX_NAME + "/_search";

    std::string query = "                                   \
    {                                                       \
        \"query\": {                                        \
            \"match\": {                                    \
                \"traceID\":\""+trace_id+"\"                    \
            }                                               \
        }                                                   \
    }";
    
    auto raw_response = http_get(url, base64_encode("elastic:changeme"), query);
    return raw_response;
}

void print_trace_structure(trace_structure trace) {
    std::cout << "n: " << trace.num_nodes << std::endl;
    std::cout << "node names:" << std::endl;
    for (const auto& elem : trace.node_names) {
        std::cout << elem.first << " : " << elem.second << std::endl;
    }
    std::cout << "edges:" << std::endl;
    for (const auto& elem : trace.edges) {
        std::cout << elem.first << " : " << elem.second << std::endl;
    }
}


trace_structure morph_json_to_trace_struct(json trace_json) {
    trace_structure res;
    if (trace_json == NULL) {
        return res;
    }

    std::unordered_map<std::string, int> span_id_to_node_id;
    int id = 0;
    for (auto ele : trace_json) {
        span_id_to_node_id[ele["_source"]["spanID"]] = id;
        res.node_names[id] = ele["_source"]["process"]["serviceName"];
        id++;
    }

    res.num_nodes = span_id_to_node_id.size();

    for (auto ele : trace_json) {
        auto id = span_id_to_node_id[ele["_source"]["spanID"]];
        auto refs = ele["_source"]["references"];
        for (auto ref : refs) {
            if (ref["refType"] == "CHILD_OF") {
                auto parent_span_id = ref["spanID"];
                auto parent_id = span_id_to_node_id[parent_span_id];
                res.edges.insert(std::make_pair(parent_id, id));
            }
        }
    }

    return res;
}

json get_trace_ids_for_interval(std::string start_time, std::string end_time, int limit) {
    std::string url = std::string(TEMPO_IP) + INDEX_NAME + "/_search?size=" + std::to_string(limit);

    std::string query = "                                   \
    {                                                       \
        \"_source\": \"traceID\",                           \
        \"query\": {                                        \
            \"range\": {                                    \
                \"startTimeMillis\": {                      \
                    \"gte\": "+ start_time + ",             \
                    \"lte\": "+ end_time + "                \
                }                                           \
            }                                               \
        }                                                   \
    }";

    
    auto raw_response = http_get(url, base64_encode("elastic:changeme"), query);

    auto response = convert_search_result_to_json(raw_response);
    auto traces = response["hits"]["hits"];

    if (traces.size() == 0) {
        std::cout << "No traces in interval [" << start_time << ", " << end_time << "]!" << std::endl;
        return {};
    }
    return traces;
}

// is not very general, good enough for benchmarks queries
bool does_trace_satisfy_condition_for_service(std::string service, json& trace, std::vector<std::string> condition) {
    for (auto span : trace) {
        if (span["_source"]["process"]["serviceName"] == service) {
            if (int(span["_source"][condition[1]]) >= std::stoi(condition[2])) {
                return true;
            }
        }
    }
    return false;
}

std::string fetch_and_filter(json trace_metadata, trace_structure query_trace, std::string start, std::string end, std::vector<std::vector<std::string>> conditions) {
    auto trace_id = trace_metadata["_source"]["traceID"];
    auto raw_trace = fetch_trace(trace_id);
    auto trace = convert_search_result_to_json(raw_trace)["hits"]["hits"];
    auto candidate_trace = morph_json_to_trace_struct(trace); // todo
    auto iso_maps = get_isomorphism_mappings(candidate_trace, query_trace);
    if (iso_maps.size() == 0) {
        return "";
    }

    if (conditions.size() < 1) {
        return trace_id;
    }

    for (auto iso_map : iso_maps) {
        bool all_holds = true;
        for (auto cond: conditions) {
            auto service_name = candidate_trace.node_names[iso_map[std::stoi(cond[0])]];
            auto condition_holds = does_trace_satisfy_condition_for_service(service_name, trace, cond);
            if (!condition_holds) {
                all_holds = false;
                break;
            }
        }
        if (all_holds) {
            return trace_id;
        }
    }

    return "";
    
}

/**
 * Call this function for 1 seconds interval
 * */
std::vector<std::string> get_traces_by_structure_for_interval(trace_structure query_trace, std::string start_time, std::string end_time, int limit, std::vector<std::vector<std::string>> conditions) {
    auto traces_metadata = get_trace_ids_for_interval(start_time, end_time, limit);

    std::vector<std::future<std::string>> response_futures;
    std::unordered_set<std::string> alread_seen;

    int count = 1;
    for (auto ele : traces_metadata) {
        std::string traceid = ele["_source"]["traceID"];


        if (alread_seen.find(traceid) == alread_seen.end()) {
            response_futures.push_back(
            std::async(std::launch::async, fetch_and_filter, ele, query_trace, start_time, end_time, conditions));
            alread_seen.insert(traceid);
            if (count%50 == 0) {
                response_futures[response_futures.size()-1].wait();
            }
            count++;
        }
    }

    std::vector<std::string> response;
    for_each(response_futures.begin(), response_futures.end(),
    [&response](std::future<std::string>& fut) {
        std::string trace_id = fut.get();
        if (false == trace_id.empty()) {
            response.push_back(trace_id);
        }
	});
    return response;
}

std::vector<std::string> get_traces_by_structure(trace_structure query_trace, std::string start_time, std::string end_time, std::vector<std::vector<std::string>> conditions) {
    std::vector<std::future<std::vector<std::string>>> response_futures;
    int limit = 10000;

    long int end = std::stol(end_time);

    for (long int i = std::stol(start_time); i < end; i+=1000) {
        response_futures.push_back(
            std::async(std::launch::async, get_traces_by_structure_for_interval, query_trace, std::to_string(i), std::to_string(i+999), limit, conditions)
        );
    }

    std::vector<std::string> response;
    for_each(response_futures.begin(), response_futures.end(),
    [&response](std::future<std::vector<std::string>>& fut) {
        std::vector<std::string> trace_ids = fut.get();
        response.insert(response.end(), trace_ids.begin(), trace_ids.end());
	});

    return response;
}

json get_trace_by_id(std::string trace_id, std::string index_name, int start_time = -1, int end_time = -1) {
    std::string url = std::string(TEMPO_IP) + "/" + index_name + "/_search?q=traceID:" + trace_id; 
    
    if (start_time != -1 && end_time != -1) {
        url += "?start=" + std::to_string(start_time) + "&end=" + std::to_string(end_time);
    }

    auto raw_response = http_get(url, base64_encode("elastic:changeme"));
    return convert_search_result_to_json(raw_response);
}

std::string read_file(std::string absolute_filename) {
    std::ifstream myFile(absolute_filename);
    std::ostringstream tmp;
    tmp << myFile.rdbuf();
    std::string s = tmp.str();
    return s;
}

// bazel run :es_gq http://34.132.177.153:9200 jaeger-span-2022-09-08 1662648656720 1662648656720
int main(int argc, char *argv[]) {

    if (argc < 5) {
        std::cerr << "Die!" << std::endl;
        exit(1);
    }

    TEMPO_IP = argv[1];
    TEMPO_IP += "/";
    INDEX_NAME = argv[2];
    std::string start_time = argv[3];
    std::string end_time = argv[4];

    // auto res = http_get("http://34.132.177.153:9200/jaeger-span-2022-09-08/_search", base64_encode("elastic:changeme"), query);
    // auto jres = convert_search_result_to_json(res);
    // std::cout << std::setw(4) << jres << std::endl;

    // return 0;

    trace_structure query_trace;
    query_trace.num_nodes = 3;
    query_trace.node_names.insert(std::make_pair(0, "frontend"));
    query_trace.node_names.insert(std::make_pair(1, "adservice"));
    query_trace.node_names.insert(std::make_pair(2, "checkoutservice"));

    query_trace.edges.insert(std::make_pair(0, 1));
    query_trace.edges.insert(std::make_pair(1, 2));

    std::vector<std::vector<std::string>> conditions = {
        {"0", "duration", "100"}
    };

    for (int i = 0; i < 1; i++) {
        boost::posix_time::ptime start, stop;
        start = boost::posix_time::microsec_clock::local_time();

        auto res = get_traces_by_structure(query_trace, start_time, end_time, conditions);

        stop = boost::posix_time::microsec_clock::local_time();
        boost::posix_time::time_duration dur = stop - start;
        int64_t milliseconds = dur.total_milliseconds();
        std::cout << milliseconds << std::endl;
        std::cout << "res: " << res.size() << std::endl;
        sleep(3);
    }
    return 0;
}
