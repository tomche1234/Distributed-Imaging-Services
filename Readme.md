# Distributed Image Processing Pipeline

Using Docker, RabbitMQ, PostgreSQL, OpenCV, and C++

This project implements a distributed image-processing pipeline using three C++ applications communicating via RabbitMQ and storing results in PostgreSQL.
All applications share a common Ubuntu-based Docker image with OpenCV, Boost, SimpleAmqpClient, and JSON libraries pre-installed.

---

# 🧱 1. Base Docker Image

Before running the system, build the shared base image that installs all required build tools and libraries:

## Dockerfile.base

This is the exact base image used:

```
# Base image
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install build tools and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    pkg-config \
    libopencv-dev \
    cmake \
    git \
    libboost-all-dev \
    librabbitmq-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Install nlohmann/json
RUN git clone https://github.com/nlohmann/json.git /tmp/json && \
    mkdir -p /usr/include/nlohmann && \
    cp /tmp/json/single_include/nlohmann/json.hpp /usr/include/nlohmann/ && \
    rm -rf /tmp/json

# Install SimpleAmqpClient
RUN git clone https://github.com/alanxz/SimpleAmqpClient.git /tmp/SimpleAmqpClient && \
    mkdir -p /tmp/SimpleAmqpClient/build && \
    cd /tmp/SimpleAmqpClient/build && \
    cmake .. && make && make install && \
    rm -rf /tmp/SimpleAmqpClient

# Base image is ready for C++ apps
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### Build the shared base image:

```sh
docker build -t my-base-image -f Dockerfile.base .
```

All apps (image-generator, feature-extrator, data-logger) inherit from this image.

---

# 🚀 2. Start the Entire System

Once the base image is built:

```sh
docker-compose up --build -d
```

This launches:

* PostgreSQL
* RabbitMQ
* image-generator
* feature-extrator
* data-logger

---

# 📦 3. System Components

## 3.1 PostgreSQL Database

Used by App3 to store processed image metadata and SIFT keypoint data.

Database: `voyis_main`
Table structure:

```sql
CREATE TABLE files (
  id SERIAL PRIMARY KEY,
  filename VARCHAR(100) NOT NULL,
  backup_path VARCHAR(255) NOT NULL,
  json_data JSONB NOT NULL,
  created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
```

---

## 3.2 RabbitMQ

Message broker for communication:

| Queue Name        | Producer         | Consumer             |
| ----------------- | ---------------- | -------------------- |
| `file_queue`      | image-generator  | feature-extrator     |
| `extract_results` | feature-extrator | data-logger          |

---

# 🖼️ 4. Image Generator

Reads images from a folder and continuously publishes them via RabbitMQ.

### Responsibilities

* Input folder: `image/in`
* Reads all image files
* Supports small to very large images (>30 MB)
* Loops forever
* Publishes raw binary via SimpleAmqpClient
* Saves backup binary into `image/backup/YYYY/MM/filename`
* Sends messages to `file_queue`

---

# 🔍 5. Feature Extrator

Receives binary images, runs SIFT, and forwards results.

### Pipeline

1. Consumes from `file_queue`
2. Decodes binary → `cv::Mat`
3. Runs SIFT detector:

   * Keypoints
   * Descriptors
4. Packages image + JSON metadata
5. Publishes to `extract_results`

---

# 🗄️ 6. Storage Service

Consumes processed messages and saves them to PostgreSQL.

### Responsibilities

* Listen on `extract_results`
* Insert into table `files`
* Store:

  * filename
  * backup path
  * JSON data (keypoints/descriptors)
  * timestamp

---

# 📁 Folder Structure

```
project/
├── Dockerfile.base
├── docker-compose.yml
├── image/
│   ├── in/             # image-generator reads
│   └── backup/         # image-generator stores YYYY/MM/...
├── image-generator/
│   └── ...
├── feature-extrator/
│   └── ...
└── data-logger/
    └── ...
```

---

# ▶️ Running & Debugging

### Start all:

```
docker-compose up --build -d
```

### View logs:

```
docker logs cpp-image-generator-app -f
docker logs cpp-feature-extrator-app -f
docker logs cpp-data-logger-app -f
```

### Stop:

```
docker-compose down
```

---

# 🔗 Data Flow Overview

```
           (Images)
image/in ───► image-generator
              │
              ▼
        RabbitMQ: file_queue
              │
              ▼
             feature-extrator
              │
              ▼
        RabbitMQ: extract_results
              │
              ▼
             data-logger
              │
              ▼
        PostgreSQL (files)
```

---

# ✔️ Summary

This pipeline provides:

* Continuous image streaming
* IPC messaging via RabbitMQ
* SIFT extraction using OpenCV
* JSON metadata handling via nlohmann/json
* Storage us
