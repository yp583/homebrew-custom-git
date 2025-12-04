#ifndef UTILS_HPP
#define UTILS_HPP

#include <vector>
#include <string>
#include <future>
#include <nlohmann/json.hpp>
#include "openai_api.hpp"
#include "async_openai_api.hpp"
using namespace std;

static constexpr unsigned char UTF8_CONTINUATION_MASK = 0xC0;
static constexpr unsigned char UTF8_CONTINUATION_BYTE = 0x80;

float cos_sim(vector<float> a, vector<float> b);
string generate_commit_message(OpenAIAPI& chat_api, const string& code_changes);
future<string> async_generate_commit_message(AsyncOpenAIAPI& chat_api, const string& code_changes);
string parse_chat_response(const string& response);
vector<float> parse_embedding(const string& response);
string utf8_substr(const string& str, size_t max_bytes);
#endif // UTILS_HPP