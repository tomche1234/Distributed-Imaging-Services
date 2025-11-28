// producer/producer.cpp
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <iostream>
#include <unistd.h>
#include <chrono>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp> 
using json = nlohmann::json;

void insertFile(const json& body)
// insert file record into the database
{
  pqxx::connection C(
    "dbname=voyis_main user=postgres password=postgres host=postgres port=5432"
  );
  if (C.is_open()) {
    std::cout << "Opened database successfully: " << C.dbname() << std::endl;
  } else {
    std::cerr << "Can't open database" << std::endl;
  }

  pqxx::work W(C);

  // Define the SQL command
  std::string sql = "INSERT INTO files (filename, backup_path, json_data) VALUES ($1, $2, $3)";

  // Execute the command using prepared statement substitution for safety
  W.exec_params(
    sql, 
    body["filename"].get<std::string>(), 
    body["backupPath"].get<std::string>(), 
    body["keypoints"].dump()
  );

  W.commit();
  std::cout << "Records created successfully" << std::endl;
  C.disconnect();
}

int main() {
  try {
    const char* host_env = getenv("RABBITMQ_HOST");
    std::string host = host_env ? host_env : "localhost";
    AmqpClient::Channel::OpenOpts opts;
    int port = 5672;
    std::string user = "guest";
    std::string pass = "guest";
    opts.host = host;
    opts.port = port;
    opts.auth = AmqpClient::Channel::OpenOpts::BasicAuth(user, pass); // Use BasicAuth for user/pass
    auto channel = AmqpClient::Channel::Open(opts);
    channel->DeclareQueue("extract_results", false, true, false, false);
    std::string consumer_tag = channel->BasicConsume("extract_results", "json_result", true, false, false);
    while (true)
    {
      AmqpClient::Envelope::ptr_t envelope;
      channel->BasicConsumeMessage("json_result", envelope);
      std::string message = envelope->Message()->Body();
      std::cout << " [x] Received: json data" << std::endl;
      json body = json::parse(message);
      insertFile(body);
      channel->BasicAck(envelope);
      std::cout << " [✓] Done & ACKed\n";
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}