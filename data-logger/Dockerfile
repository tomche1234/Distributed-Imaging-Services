FROM my-base-image

RUN apt-get update && \
    apt-get install -y \
    libpqxx-dev \
    libboost-date-time-dev \
    && rm -rf /var/lib/apt/lists/*
# --- END libpqxx DEPENDENCY INSTALLATION ---

# ... (rest of your existing setup)

# --- APPLICATION BUILD START ---
WORKDIR /app
COPY main.cpp ./

# Simplified Compilation: Uses pkg-config for all necessary libpqxx flags
# Note: You can remove the old ODB files (file.hxx, file-odb.cxx, etc.)
RUN g++ -o data_logger_app main.cpp $(pkg-config --cflags --libs libpqxx) -lSimpleAmqpClient -lboost_date_time

CMD ["./data_logger_app"]