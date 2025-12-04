#ifndef ASYNC_OPENAI_API_HPP
#define ASYNC_OPENAI_API_HPP

#include "async_https_api.hpp"
#include <string>
#include <vector>
#include <future>
#include <nlohmann/json.hpp>

using namespace std;
static constexpr size_t MAX_EMBEDDING_BYTES = 16000;

class AsyncOpenAIAPI {
  private:
    AsyncHTTPSConnection& api_connection;
    string api_key;
  public:
    AsyncOpenAIAPI(AsyncHTTPSConnection& api_connection, const string& api_key);
    future<HTTPSResponse> async_embedding(string text);
    future<HTTPSResponse> async_chat(const nlohmann::json& messages, int max_tokens = 100, float temperature = 0.7);
    void run_requests();
};

#endif // ASYNC_OPENAI_API_HPP
