# Distributed Image Processing Pipeline

Using Docker, RabbitMQ, PostgreSQL, OpenCV, and C++

This project implements a distributed image-processing pipeline using three C++ applications communicating via RabbitMQ and storing results in PostgreSQL.
All applications share a common Ubuntu-based Docker image with OpenCV, Boost, SimpleAmqpClient, and JSON libraries pre-installed.

---

# Pre-requisites

Before running the pipeline, ensure you have Docker and Docker Compose installed.

### Install Docker

# Running Instructions

Follow these steps to set up and run the distributed image-processing pipeline.

## 🧩 Step 1 — Clone the Repository

```sh
git clone https://github.com/tomche1234/Distributed-Imaging-Services.git
cd Distributed-Imaging-Services
```

## 🏗️ Step 2 — Build the Base Docker Image

The base image contains all shared dependencies (OpenCV, Boost, RabbitMQ client, JSON, build tools).

```sh
docker build -t my-base-image -f Dockerfile.base .
```

This must be done **before** starting any application containers.

## 📂 Step 3 — Prepare Input Folder

The image-generator reads files from:

```
images/in
```

Create these folders locally:

```sh
mkdir -p images/in
mkdir -p images/backup
```

Then place any `.jpg`, `.jpeg`, or `.png` files inside:

```
images/in/
```

## 🚀 Step 4 — Run the Pipeline

Start all services:

```sh
docker-compose up --build -d
```

This will start:

* PostgreSQL
* RabbitMQ
* image-generator
* feature-extractor
* data-logger

---

# 🧱 Base Docker Image

The pipeline uses a shared Ubuntu-based base image with OpenCV, Boost, SimpleAmqpClient, and JSON.

### Build the base image:

```sh
docker build -t my-base-image -f Dockerfile.base .
```

### Dockerfile.base (for reference):

```dockerfile
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

---

# System Components

### PostgreSQL

Stores processed data.

### RabbitMQ

Handles both message queues:

| Queue             | Producer          | Consumer          |
| ----------------- | ----------------- | ----------------- |
| `file_queue`      | image-generator   | feature-extractor |
| `extract_results` | feature-extractor | data-logger       |

---

# 🖼️ Image Generator

* Watches `images/in/` every 10 seconds
* Reads JPG/PNG (validates real images)
* Sends raw binary to RabbitMQ
* Saves backup files under:

  ```
  images/backup/YYYY/MM/
  ```

---

# 🔍 Feature Extractor

* Consumes raw image binaries
* Decodes using OpenCV
* Runs SIFT
* Publishes JSON results to `extract_results`

---

# 🗄️ Data Logger

* Saves processed results to PostgreSQL
* Inserts JSONB metadata and file info

---

# Folder Structure

```
project/
├── Dockerfile.base
├── docker-compose.yml
├── images/
│   ├── in/
│   └── backup/
├── image-generator/
├── feature-extractor/
└── data-logger/
```

---

# Running & Debugging

### Start all:

```sh
docker-compose up --build -d
```

### View logs:

```sh
docker logs cpp-image-generator-app -f
docker logs cpp-feature-extractor-app -f
docker logs cpp-data-logger-app -f
```

### Stop:

```sh
docker-compose down
```

---

# Data Flow

```
images/in
   │
   ▼
image-generator
   │
   ▼
RabbitMQ → file_queue
   │
   ▼
feature-extractor (SIFT)
   │
   ▼
RabbitMQ → extract_results
   │
   ▼
data-logger → PostgreSQL (files table)
```

---

# Summary

This project provides:

* Continuous image ingestion
* Distributed processing via RabbitMQ
* SIFT feature extraction
* JSON-based metadata
* PostgreSQL storage
