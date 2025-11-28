FROM my-base-image

WORKDIR /app

# Copy app source
COPY main.cpp .

# Build the app
RUN g++ -o feature_extrator_app main.cpp $(pkg-config --cflags --libs opencv4) -lSimpleAmqpClient -lboost_system -lboost_thread -lpthread

# Run the app
CMD ["./feature_extrator_app"]