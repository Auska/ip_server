#include "password_handler.h"

#include <chrono>
#include <stdexcept>

#include "logger.h"
#include "password_generator.h"
#include "types.h"

namespace ip_server {

PasswordHandler::PasswordHandler(Metrics* metrics) : metrics_(metrics) {}

void PasswordHandler::handle_get(const httplib::Request& req, httplib::Response& res) {
    try {
        PasswordConfig config;

        auto length_param = req.get_param_value("length");
        if (!length_param.empty()) config.length = std::stoi(length_param);

        auto uppercase_param = req.get_param_value("uppercase");
        if (!uppercase_param.empty()) config.uppercase = (uppercase_param == "true" || uppercase_param == "1");

        auto lowercase_param = req.get_param_value("lowercase");
        if (!lowercase_param.empty()) config.lowercase = (lowercase_param == "true" || lowercase_param == "1");

        auto digits_param = req.get_param_value("digits");
        if (!digits_param.empty()) config.digits = (digits_param == "true" || digits_param == "1");

        auto symbols_param = req.get_param_value("symbols");
        if (!symbols_param.empty()) config.symbols = (symbols_param == "true" || symbols_param == "1");

        auto exclude_similar_param = req.get_param_value("exclude_similar");
        if (!exclude_similar_param.empty()) config.exclude_similar = (exclude_similar_param == "true" || exclude_similar_param == "1");

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

        if (body.contains("length") && body["length"].is_number_integer())
            config.length = body["length"].get<int>();
        if (body.contains("uppercase") && body["uppercase"].is_boolean())
            config.uppercase = body["uppercase"].get<bool>();
        if (body.contains("lowercase") && body["lowercase"].is_boolean())
            config.lowercase = body["lowercase"].get<bool>();
        if (body.contains("digits") && body["digits"].is_boolean())
            config.digits = body["digits"].get<bool>();
        if (body.contains("symbols") && body["symbols"].is_boolean())
            config.symbols = body["symbols"].get<bool>();
        if (body.contains("exclude_similar") && body["exclude_similar"].is_boolean())
            config.exclude_similar = body["exclude_similar"].get<bool>();

        int count = 1;
        if (body.contains("count") && body["count"].is_number_integer())
            count = body["count"].get<int>();

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
