#include "async_openai_api.hpp"
#include "utils.hpp"

using json = nlohmann::json;
using namespace std;

AsyncOpenAIAPI::AsyncOpenAIAPI(AsyncHTTPSConnection& api_connection, const string& api_key) : api_connection(api_connection), api_key(api_key) {};

future<HTTPSResponse> AsyncOpenAIAPI::async_embedding(string text) {
    const vector<pair<string, string>> headers = {
        {"Authorization", "Bearer " + this->api_key},
        {"Content-Type", "application/json"}
    };

    text = utf8_substr(text, MAX_EMBEDDING_BYTES);

    json request_body = {
        {"model", "text-embedding-3-small"},
        {"input", text}
    };
    string body = request_body.dump();

    promise<HTTPSResponse> prom;
    future<HTTPSResponse> fut = prom.get_future();
    this->api_connection.post_async("api.openai.com", "/v1/embeddings", body, headers, std::move(prom));
    return fut;
}

future<HTTPSResponse> AsyncOpenAIAPI::async_chat(const nlohmann::json& messages, int max_tokens, float temperature) {
    const vector<pair<string, string>> headers = {
        {"Authorization", "Bearer " + this->api_key},
        {"Content-Type", "application/json"}
    };

    json request_body = {
        {"model", "gpt-4o-mini"},
        {"messages", messages},
        {"max_tokens", max_tokens},
        {"temperature", temperature}
    };

    string body = request_body.dump();

    promise<HTTPSResponse> prom;
    future<HTTPSResponse> fut = prom.get_future();
    this->api_connection.post_async("api.openai.com", "/v1/chat/completions", body, headers, std::move(prom));
    return fut;
}

void AsyncOpenAIAPI::run_requests() {
    this->api_connection.run_loop();
}
