# Baseline build of the unmodified upstream tree: BIOS only (upstream has no unit tests)
FROM debian:bookworm-slim
RUN apt-get update -qq && \
    apt-get install -y -qq clang llvm lld bsdmainutils make && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /src
CMD ["bash", "-c", "cd src/bios && make clean && make"]
