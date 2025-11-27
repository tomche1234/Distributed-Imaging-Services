FROM my-base-image

WORKDIR /app

# Copy app source and images
COPY main.cpp .
# COPY ../images /images

# Build the app
RUN g++ -o image_generator_app main.cpp $(pkg-config --cflags --libs opencv4) -lSimpleAmqpClient -lboost_system -lboost_thread -lpthread

# Run the app
CMD ["./image_generator_app"]