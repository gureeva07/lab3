FROM ghcr.io/userver-framework/ubuntu-22.04-userver-pg:latest

WORKDIR /service_template

COPY . .

RUN cmake -B build -S . && cmake --build build

EXPOSE 8080

CMD ["./build/service_template", "--config", "/service_template/configs/static_config.yaml"]
