FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    gcc \
    libc6-dev \
    libcrypt-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY c_backend/ ./c_backend/
COPY html/ ./html/

RUN gcc c_backend/server.c c_backend/bank.c -o bank_server -lcrypt

EXPOSE 8080

CMD ["./bank_server"]
