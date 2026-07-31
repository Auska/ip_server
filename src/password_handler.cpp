#include "password_handler.h"

#include <chrono>
#include <optional>
#include <stdexcept>

#include "logger.h"
#include "password_generator.h"
#include "types.h"

namespace ip_server {

namespace {

bool parse_bool_param(const httplib::Request& req, const char* key, bool default_val) {
    auto val = req.get_param_value(key);
    if (val.empty()) return default_val;
    return val == "true" || val == "1";
}

}  // namespace

PasswordHandler::PasswordHandler(Metrics* metrics) : metrics_(metrics) {}

void PasswordHandler::handle_get(const httplib::Request& req, httplib::Response& res) {
    try {
        PasswordConfig config;

        auto length_param = req.get_param_value("length");
        if (!length_param.empty()) config.length = std::stoi(length_param);

        config.uppercase       = parse_bool_param(req, "uppercase", true);
        config.lowercase       = parse_bool_param(req, "lowercase", true);
        config.digits          = parse_bool_param(req, "digits", true);
        config.symbols         = parse_bool_param(req, "symbols", true);
        config.exclude_similar = parse_bool_param(req, "exclude_similar", true);

        std::string error_message;
        if (!PasswordGenerator::validate_config(config, error_message)) {
            send_error_response(res, 400, "Bad Request", error_message);
            return;
        }

        ScopedTimer timer;
        auto result = PasswordGenerator::generate(config);

        metrics_->record_request(false, timer.elapsed());

        nlohmann::json response;
        response["password"] = result.password;
        response["length"] = result.length;
        response["entropy"] = result.entropy;
        response["strength"] = result.strength;

        send_json_response(res, response, 200);

    } catch (const std::invalid_argument& e) {
        send_error_response(res, 400, "Bad Request", "Invalid parameter value: " + std::string(e.what()));
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

        auto read_bool = [&](const char* key, bool& field) {
            if (body.contains(key) && body[key].is_boolean())
                field = body[key].get<bool>();
        };
        auto read_int = [&](const char* key, int& field) {
            if (body.contains(key) && body[key].is_number_integer())
                field = body[key].get<int>();
        };

        read_int("length", config.length);
        read_bool("uppercase", config.uppercase);
        read_bool("lowercase", config.lowercase);
        read_bool("digits", config.digits);
        read_bool("symbols", config.symbols);
        read_bool("exclude_similar", config.exclude_similar);

        int count = 1;
        read_int("count", count);

        if (count < 1) {
            send_error_response(res, 400, "Bad Request", "Count must be at least 1");
            return;
        }

        if (count > PasswordGenerator::MAX_BATCH) {
            send_error_response(res, 400, "Bad Request",
                                "Count cannot exceed " + std::to_string(PasswordGenerator::MAX_BATCH));
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
        response["count"] = static_cast<int>(results.size());
        response["passwords"] = nlohmann::json::array();

        for (const auto& result : results) {
            nlohmann::json password_json;
            password_json["password"] = result.password;
            password_json["length"] = result.length;
            password_json["entropy"] = result.entropy;
            password_json["strength"] = result.strength;
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

void PasswordHandler::send_error_response(httplib::Response& res, int status,
                                          const std::string& error, const std::string& message) {
    nlohmann::json error_json;
    error_json["error"] = error;
    error_json["message"] = message;
    res.status = status;
    res.set_content(error_json.dump(), "application/json");
}

void PasswordHandler::send_json_response(httplib::Response& res, const nlohmann::json& data,
                                         int status) {
    res.status = status;
    res.set_content(data.dump(), "application/json");
}

}  // namespace ip_server
