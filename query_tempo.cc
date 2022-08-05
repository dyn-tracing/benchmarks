#include "query_tempo.h"

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
    std::string url = std::string(TEMPO_IP) + std::string(TEMPO_TRACES) + trace_id;
    auto raw_response = http_get(url);
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
    if (trace_json["data"].size() < 1) {
        return {};
    }
    auto trace = trace_json["data"][0];
    auto trace_processes = trace["processes"];
    auto trace_spans = trace["spans"];

    std::unordered_map<std::string, int> span_id_to_node_id;
    int id = 0;
    for (auto ele : trace_spans) {
        span_id_to_node_id[ele["spanID"]] = id;
        id++;
    }

    trace_structure response;
    response.num_nodes = span_id_to_node_id.size();

    for (auto ele : trace_spans) {
        auto id = span_id_to_node_id[ele["spanID"]];
        std::string processId = ele["processID"];
        response.node_names[id] = std::string(trace_processes[processId]["serviceName"]);
    }

    for (auto ele : trace_spans) {
        auto id = span_id_to_node_id[ele["spanID"]];
        auto refs = ele["references"];
        for (auto ref : refs) {
            if (ref["refType"] == "CHILD_OF") {
                auto parent_span_id = ref["spanID"];
                auto parent_id = span_id_to_node_id[parent_span_id];
                response.edges.insert(std::make_pair(parent_id, id));
            }
        }
    }

    return response;
}
/**
 * Call this function for 3 seconds interval
 * */
std::vector<std::string> get_traces_by_structure_and_interval(trace_structure query_trace, int start_time, int end_time, int limit) {
    if (end_time == start_time || limit < 1 || query_trace.num_nodes < 1) {
        return {};
    }

    std::string url = std::string(TEMPO_IP) + std::string(TEMPO_SEARCH) + "?start=" + \
        std::to_string(start_time) + "&end=" + std::to_string(end_time) + "&limit=" + std::to_string(limit);

    auto raw_response = http_get(url);
    auto response = convert_search_result_to_json(raw_response);
    if (response["traces"] == NULL) {
        std::cout << "No traces in interval [" << start_time << ", " << end_time << "]!" << std::endl;
        return {};
    }

    std::vector<std::string> res;
    for (auto ele : response["traces"]) {
        auto trace = convert_search_result_to_json(fetch_trace(ele["traceID"]));
        auto candidate_trace = morph_json_to_trace_struct(trace);
        auto iso_maps = get_isomorphism_mappings(candidate_trace, query_trace);
        if (iso_maps.size() == 0) {
            print_trace_structure(candidate_trace);
        } else {
            std::cout << "." << std::endl;
        }
        res.push_back(ele["traceID"]);
    }

    return res;
}

std::vector<std::string> get_traces_by_structure(trace_structure query_trace, int start_time, int end_time) {
    std::vector<std::string> response;

    int i = start_time, j = start_time + 3;
    while (i < end_time) {
        auto trace_ids = get_traces_by_structure_and_interval(query_trace, i, std::min(j, end_time), 50000);
        response.insert(response.end(), trace_ids.begin(), trace_ids.end());
        i = j+1;
        j = i+3;
    }

    return response;
}

int main() {
    trace_structure query_trace;
    query_trace.num_nodes = 3;
    query_trace.node_names.insert(std::make_pair(0, "frontend"));
    query_trace.node_names.insert(std::make_pair(1, "adservice"));
    query_trace.node_names.insert(std::make_pair(2, ASTERISK_SERVICE));

    query_trace.edges.insert(std::make_pair(0, 1));
    query_trace.edges.insert(std::make_pair(1, 2));

    // start time and end time should be in seconds. 
    auto res = get_traces_by_structure(query_trace, 1659623757, 1659623758);
    return 0;
}