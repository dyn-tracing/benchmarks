/*
 * Contains various common functions for interacting with cloud storage
 * according to the format we've stored trace data.
*/

#ifndef COMMON_H_ // NOLINT
#define COMMON_H_

#include <iostream>
#include <unordered_map>
#include <utility>
#include <map>
#include <string>
#include <vector>
#include <future>
#include <tuple>
#include <boost/algorithm/string.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/vf2_sub_graph_iso.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string/regex.hpp>

const char BUCKET_TYPE_LABEL_KEY[] = "bucket_type";
const char BUCKET_TYPE_LABEL_VALUE_FOR_SPAN_BUCKETS[] = "microservice";
const char PROJECT_ID[] = "dynamic-tracing";
const char BUCKETS_LOCATION[] = "us-central1";

const char TRACE_STRUCT_BUCKET[] = "dyntraces-snicket2";
const char TRACE_HASHES_BUCKET[] = "tracehashes-snicket2";
const char BUCKETS_SUFFIX[] = "-snicket2";

const char TRACE_STRUCT_BUCKET_PREFIX[] = "dyntraces";
const int TRACE_ID_LENGTH = 32;
const int SPAN_ID_LENGTH = 16;
const int element_count = 10000;
const char hyphen[] = "-";
const char newline[] = "\n";
const char colon[] = ":";

constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

typedef std::map<std::string, std::vector<std::string>> objname_to_matching_trace_ids;

enum index_type {
    bloom,
    folder,
    none,
    not_found,
};

/// **************** pure string processing ********************************
std::vector<std::string> split_by_string(const std::string& str,  const char* ch);
std::string hex_str(const std::string &data, int len);
bool is_same_hex_str(const std::string &data, const std::string &compare);
std::string strip_from_the_end(std::string object, char stripper);
void replace_all(std::string& str, const std::string& from, const std::string& to);
void print_progress(float progress, std::string label, bool verbose);

#endif  // COMMON_H_ // NOLINT