FROM debian:12
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libassimp-dev \
    libopenmesh-dev

WORKDIR /app
COPY . .
RUN mkdir build && cd build && cmake .. && make speedigon_cli

ENTRYPOINT ["/app/build/speedigon_cli"]