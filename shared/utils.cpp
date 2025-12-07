#include "utils.hpp"

using json = nlohmann::json;

float cos_sim(vector<float> a, vector<float> b) {
  float dot = 0.0;
  for (size_t i = 0; i < a.size(); i++) {
    dot += a[i] * b[i];
  }
  //assuming vectors are normalized
  return dot; 
}

vector<float> parse_embedding(const string& response) {
    try {
        json j = json::parse(response);
        vector<float> embedding = j["data"][0]["embedding"].get<vector<float>>();
        return embedding;
    } catch (json::exception& e) {
        cerr << "JSON parsing error with response: " << response << endl;
        return vector<float>();
    }
}

string generate_commit_message(OpenAIAPI& chat_api, const string& code_changes) {
    // TODO: This function is currently non-functional since post_chat is commented out
    // It uses the chat API which requires a different endpoint than embeddings
    return "update code"; // Placeholder until chat functionality is implemented

    /* Original implementation - requires post_chat to be functional:
    nlohmann::json messages = {
        {
            {"role", "system"},
            {"content", "You are a git commit message generator. Analyze the code changes and generate a concise commit message that describes what was actually modified, added, or fixed in the code. Focus on the technical changes, not meta-commentary. Return only the commit message without quotes or explanations. Examples: 'add HTTP chunked encoding support', 'handle SSL connection errors', 'extract JSON parsing logic'."}
        },
        {
            {"role", "user"},
            {"content", "Generate a commit message for these code changes:\n" + code_changes}
        }
    };

    return chat_api.post_chat(messages, 50, 0.3);
    */
}

string parse_chat_response(const string& response) {
    try {
        json j = json::parse(response);
        string message = j["choices"][0]["message"]["content"].get<string>();

        // Trim whitespace and remove quotes if present
        size_t start = message.find_first_not_of(" \t\n\r\"");
        size_t end = message.find_last_not_of(" \t\n\r\"");

        if (start == string::npos) return "update code";

        return message.substr(start, end - start + 1);
    } catch (json::exception& e) {
        cerr << "Chat JSON parsing error with response: " << response << endl;
        return "update code"; // fallback message
    }
}

future<string> async_generate_commit_message(AsyncOpenAIAPI& chat_api, const string& code_changes) {
    json messages = {
        {
            {"role", "system"},
            {"content", "You are a git commit message generator. Analyze the code changes and generate a concise commit message that describes what was actually modified, added, or fixed in the code. Focus on the technical changes, not meta-commentary. Return only the commit message without quotes or explanations. Examples: 'add HTTP chunked encoding support', 'handle SSL connection errors', 'extract JSON parsing logic'."}
        },
        {
            {"role", "user"},
            {"content", "Generate a commit message for these code changes:\n" + code_changes}
        }
    };

    future<HTTPSResponse> response_future = chat_api.async_chat(messages, 50, 0.3);

    return std::async(std::launch::deferred, [](future<HTTPSResponse> resp_fut) {
        HTTPSResponse response = resp_fut.get();
        return parse_chat_response(response.body);
    }, std::move(response_future));
}

string utf8_substr(const string& str, size_t max_bytes) {
    if (str.size() <= max_bytes) {
        return str;
    }
    size_t pos = max_bytes;
    while (pos > 0 && (str[pos] & UTF8_CONTINUATION_MASK) == UTF8_CONTINUATION_BYTE) {
        pos--;
    }
    return str.substr(0, pos);
}