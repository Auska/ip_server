#include "password_handler.h"

#include <chrono>
#include <stdexcept>

#include "http_server.h"
#include "logger.h"
#include "password_generator.h"
#include "types.h"

namespace ip_server {

namespace {

bool parseBoolParam(const httplib::Request& req, const char* key, bool default_val) {
    auto val = req.get_param_value(key);
    if (val.empty()) return default_val;
    return val == "true" || val == "1";
}

}  // namespace

PasswordHandler::PasswordHandler(Metrics* metrics) : metrics_(metrics) {
}

void PasswordHandler::handle_get(const httplib::Request& req, httplib::Response& res) {
    try {
        PasswordConfig config;

        auto length_param = req.get_param_value("length");
        if (!length_param.empty()) config.length_ = std::stoi(length_param);

        config.uppercase_       = parseBoolParam(req, "uppercase", true);
        config.lowercase_       = parseBoolParam(req, "lowercase", true);
        config.digits_          = parseBoolParam(req, "digits", true);
        config.symbols_         = parseBoolParam(req, "symbols", true);
        config.exclude_similar_ = parseBoolParam(req, "exclude_similar", true);

        std::string error_message;
        if (!PasswordGenerator::validate_config(config, error_message)) {
            send_error_response(res, 400, "Bad Request", error_message);
            return;
        }

        ScopedTimer timer;
        auto result = PasswordGenerator::generate(config);

        metrics_->record_request(false, timer.elapsed());

        nlohmann::json response;
        response["password"] = result.password_;
        response["length"]   = result.length_;
        response["entropy"]  = result.entropy_;
        response["strength"] = result.strength_;

        send_json_response(res, response, 200);

    } catch (const std::invalid_argument& e) {
        send_error_response(res, 400, "Bad Request",
                            "Invalid parameter value: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(res, 500, "Internal Server Error", e.what());
        metrics_->record_error();
        LOG_ERROR("Error generating password: " + std::string(e.what()));
    }
}

void PasswordHandler::handle_post(const httplib::Request& req, httplib::Response& res) {
    try {
        auto body = nlohmann::json::parse(req.body);

        PasswordConfig config;

        auto readBool = [&](const char* key, bool& field) {
            if (body.contains(key) && body[key].is_boolean()) field = body[key].get<bool>();
        };
        auto readInt = [&](const char* key, int& field) {
            if (body.contains(key) && body[key].is_number_integer()) field = body[key].get<int>();
        };

        readInt("length", config.length_);
        readBool("uppercase", config.uppercase_);
        readBool("lowercase", config.lowercase_);
        readBool("digits", config.digits_);
        readBool("symbols", config.symbols_);
        readBool("exclude_similar", config.exclude_similar_);

        int count = 1;
        readInt("count", count);

        if (count < 1) {
            send_error_response(res, 400, "Bad Request", "Count must be at least 1");
            return;
        }

        if (count > PasswordGenerator::MAX_BATCH) {
            send_error_response(res, 400, "Bad Request",
                                "Count cannot exceed "
                                    + std::to_string(PasswordGenerator::MAX_BATCH));
            return;
        }

        std::string error_message;
        if (!PasswordGenerator::validate_config(config, error_message)) {
            send_error_response(res, 400, "Bad Request", error_message);
            return;
        }

        ScopedTimer timer;
        auto results = PasswordGenerator::generate_batch(config, count);

        metrics_->record_request(false, timer.elapsed());

        nlohmann::json response;
        response["count"]     = static_cast<int>(results.size());
        response["passwords"] = nlohmann::json::array();

        for (const auto& result : results) {
            nlohmann::json password_json;
            password_json["password"] = result.password_;
            password_json["length"]   = result.length_;
            password_json["entropy"]  = result.entropy_;
            password_json["strength"] = result.strength_;
            response["passwords"].push_back(password_json);
        }

        send_json_response(res, response, 200);

    } catch (const nlohmann::json::exception& e) {
        send_error_response(res, 400, "Bad Request", "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(res, 500, "Internal Server Error", e.what());
        metrics_->record_error();
        LOG_ERROR("Error generating batch passwords: " + std::string(e.what()));
    }
}

}  // namespace ip_server
