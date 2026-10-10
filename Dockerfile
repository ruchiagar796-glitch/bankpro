# Builds the C++ backend in the cloud and runs it. You never compile anything yourself.
# ---------- stage 1: build ----------
FROM gcc:13-bookworm AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends cmake ninja-build ca-certificates \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY include include
COPY src src
COPY tests tests
# CMake downloads the two header-only libraries (SHA-256 verified).
# The tests run during the build: if any test fails, the deployment fails instead of going live broken.
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc" \
 && cmake --build build --target bank_server bank_tests \
 && ./build/bank_tests

# ---------- stage 2: small runtime image ----------
FROM debian:bookworm-slim
RUN useradd --system --uid 10001 bank && mkdir -p /app /data && chown bank /data
WORKDIR /app
COPY --from=build /src/build/bank_server ./bank_server
COPY frontend ./frontend
USER bank
# Listen on all addresses inside the container. The server refuses to do this without BMS_PASSWORD.
ENV BMS_HOST=0.0.0.0 BMS_DATA_DIR=/data PORT=8080
EXPOSE 8080
CMD ["./bank_server"]
