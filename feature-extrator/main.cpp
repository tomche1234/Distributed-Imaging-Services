#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <SimpleAmqpClient/SimpleAmqpClient.h>

#include <iostream>
#include <vector>

using json = nlohmann::json;

json extract_sift_features(const std::string& filename, const std::vector<cv::KeyPoint>& keypoints)
// Extract SIFT Features as JSON
{
    json j;
    j["filename"] = filename;
    j["num_keypoints"] = keypoints.size();

    for (const auto& kp : keypoints) {
        j["keypoints"].push_back({
            {"x", kp.pt.x},
            {"y", kp.pt.y},
            {"size", kp.size},
            {"angle", kp.angle}
        });
    }

    return j;
}

void uploadToQueue(AmqpClient::Channel::ptr_t channel, const json& result)
// Upload JSON to result queue
{
    const std::string queue_name = "extract_results";

    // Declare results queue (once is fine — repeated declarations are idempotent)
    channel->DeclareQueue(queue_name, false, true, false, false);

    std::string body = result.dump();
    AmqpClient::BasicMessage::ptr_t msg =
        AmqpClient::BasicMessage::Create(body);

    msg->ContentType("application/json");
    msg->DeliveryMode(AmqpClient::BasicMessage::dm_persistent);

    channel->BasicPublish("", queue_name, msg);
    std::cout << " [→] Uploaded JSON to extract_results\n";
}


int main()
{
    // Connect to RabbitMQ
    const char* host_env = getenv("RABBITMQ_HOST");
    std::string host = host_env ? host_env : "localhost";

    AmqpClient::Channel::OpenOpts opts;
    opts.host = host;
    opts.port = 5672;
    opts.auth = AmqpClient::Channel::OpenOpts::BasicAuth("guest", "guest");
    auto channel = AmqpClient::Channel::Open(opts);
    
    // Declare the input queue (idempotent)
    channel->DeclareQueue("file_queue", false, true, false, false);
    
    // Register consumer ONCE
    std::string consumer_tag = channel->BasicConsume(
        "file_queue",
        "file_consumer",
        false,
        false, 
        false 
    );

    std::cout << " [*] Worker is running. Waiting for images...\n" << std::endl;

    // Infinite Worker Loop
    while (true)
    {
        AmqpClient::Envelope::ptr_t envelope;

        bool gotMessage = channel->BasicConsumeMessage(consumer_tag, envelope, 500);

        if (!gotMessage) {
            // std::cout << " [*] There is no file\n" << std::endl;
            // No work available
            continue;
        }

        auto msg = envelope->Message();

        auto headers = msg->HeaderTable();
        std::string filename = headers["filename"].GetString();
        std::string backupPath = headers["backupPath"].GetString();

        std::string body = msg->Body();
        std::cout << "\n [↓] Received file: " << filename
                  << " (" << body.size() << " bytes)" << std::endl;

        if (body.empty()) {
            std::cout << " [!] ERROR: message body empty, skipping\n";
            channel->BasicAck(envelope);
            continue;
        }

        // Decode image using OpenCV
        std::vector<uchar> buf(body.begin(), body.end());
        cv::Mat image = cv::imdecode(buf, cv::IMREAD_COLOR);

        if (image.empty()) {
            std::cout << " [!] ERROR: OpenCV cannot decode image\n";
            channel->BasicAck(envelope);
            continue;
        }

        std::cout << " [OK] Image decoded successfully\n";

        // SIFT Feature Extraction
        cv::Ptr<cv::SIFT> detector = cv::SIFT::create();

        std::vector<cv::KeyPoint> keypoints;
        detector->detect(image, keypoints);

        std::cout << " [✓] Detected " << keypoints.size() << " keypoints\n";

        // Convert to JSON + Upload
        json result = extract_sift_features(filename, keypoints);
        result["backupPath"] = backupPath;
        uploadToQueue(channel, result);

        // Acknowledge message
        channel->BasicAck(envelope);
        std::cout << " [ACK] Processed: " << filename << "\n";
    }

    return 0;
}
